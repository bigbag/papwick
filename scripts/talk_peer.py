#!/usr/bin/env python3
"""A walkie-talkie peer on a PC, to test the Papwick Walkie-talkie app without a second device.

The script joins the talk on UDP port 47800 in the same network as the device. It answers HELLO, shows the
devices that come and go, and shows each talk with its length and peak level. It accepts each pairing request.
It also asks each new device to pair. Push green on the device to allow the PC.

    python3 scripts/talk_peer.py --echo --save talk.wav

--echo sends each received talk back after its end, so one device can test both directions.
--save writes the last received talk to a 16 kHz mono WAV file.
The PC firewall must let UDP port 47800 in, also broadcast packets.
"""

import argparse
import random
import socket
import struct
import sys
import time
import wave
import zlib
from array import array
from collections import deque

PORT = 47800
VERSION = 1
RATE = 16000
FRAME = 320
HELLO, REPLY, BYE, AUDIO, END, REQUEST, ACCEPT, REFUSE = range(1, 9)
HEADER = struct.Struct("<2sBBIB")
NAME_LEN = 16
RX_TIMEOUT = 0.5


def packet(kind, sender, talk, payload=b""):
    return HEADER.pack(b"PW", VERSION, kind, sender, talk) + payload


def samples(data):
    a = array("h", data)
    if sys.byteorder == "big":
        a.byteswap()
    return a


class Peer:
    def __init__(self, args):
        self.args = args
        self.sock = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        self.sock.setsockopt(socket.SOL_SOCKET, socket.SO_BROADCAST, 1)
        self.sock.bind(("", PORT))
        # A short timeout, so the echo frames go out at their time.
        self.sock.settimeout(0.005)
        # A stable id, so each run is the same paired device.
        self.id = zlib.crc32(socket.gethostname().encode()) or 1
        self.name = args.name.encode()[: NAME_LEN - 1].ljust(NAME_LEN, b"\0")
        self.talk_id = random.randrange(256)
        self.devices = {}
        self.talk = None
        # Echo packets that wait for their time: (time, kind, ip, talk id, payload).
        self.outbox = deque()

    def send(self, kind, ip, payload=b"", talk=None):
        self.sock.sendto(packet(kind, self.id, self.talk_id if talk is None else talk, payload), (ip, PORT))

    def receive(self):
        try:
            data, (ip, _) = self.sock.recvfrom(2048)
        except socket.timeout:
            return None
        if len(data) < HEADER.size:
            return None
        magic, version, kind, sender, talk = HEADER.unpack_from(data)
        if magic != b"PW" or version != VERSION or sender == self.id:
            return None
        return kind, sender, talk, ip, data[HEADER.size :]

    def handle(self, kind, sender, talk, ip, payload):
        if kind in (HELLO, REPLY, REQUEST, ACCEPT, REFUSE) and len(payload) == NAME_LEN:
            name = payload.split(b"\0")[0].decode(errors="replace")
            if sender not in self.devices:
                self.send(REQUEST, ip, self.name)
            if self.devices.get(sender) != (ip, name):
                print(f"Device {name} at {ip}")
            self.devices[sender] = (ip, name)
            if kind == HELLO:
                self.send(REPLY, ip, self.name)
            if kind == REQUEST:
                self.send(ACCEPT, ip, self.name)
            if kind == ACCEPT:
                print(f"Paired with {name}")
            if kind == REFUSE:
                print(f"{name} refused or removed the pairing")
        elif kind == BYE:
            if self.devices.pop(sender, None):
                print(f"Device at {ip} left")
        elif kind == AUDIO and len(payload) == 2 * FRAME:
            if sender not in self.devices:
                self.send(HELLO, ip, self.name)
            # A new talk id from the same sender starts a new talk, also when its END was lost.
            if self.talk and self.talk["id"] == sender and self.talk["talk"] != talk:
                self.finish()
            if self.talk is None:
                self.talk = {"id": sender, "talk": talk, "ip": ip, "frames": []}
                print(f"Talk from {ip}")
            if self.talk["id"] == sender:
                self.talk["frames"].append(payload)
                self.talk["last"] = time.monotonic()
        elif kind == END and self.talk and self.talk["id"] == sender and self.talk["talk"] == talk:
            return True
        return False

    def finish(self):
        args = self.args
        talk, self.talk = self.talk, None
        data = b"".join(talk["frames"])
        peak = max((abs(v) for v in samples(data)), default=0)
        print(f"Talk end: {len(talk['frames'])} frames, {len(data) / 2 / RATE:.2f} s, peak {peak}")
        if args.save:
            with wave.open(args.save, "wb") as w:
                w.setnchannels(1)
                w.setsampwidth(2)
                w.setframerate(RATE)
                w.writeframes(data)
            print(f"Saved {args.save}")
        if args.echo:
            self.echo(talk)

    def echo(self, talk):
        # The device needs time to stop its microphone and start its speaker. The receive loop sends the frames.
        self.talk_id = (self.talk_id + 1) % 256
        step = FRAME / RATE
        start = max(time.monotonic() + 0.5, self.outbox[-1][0] + step if self.outbox else 0)
        for i, frame in enumerate(talk["frames"]):
            self.outbox.append((start + i * step, AUDIO, talk["ip"], self.talk_id, frame))
        for _ in range(3):
            self.outbox.append((start + len(talk["frames"]) * step, END, talk["ip"], self.talk_id, b""))

    def pump(self):
        now = time.monotonic()
        while self.outbox and self.outbox[0][0] <= now:
            _, kind, ip, talk, payload = self.outbox.popleft()
            self.send(kind, ip, payload, talk)
            if not self.outbox:
                print("Echo sent")


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--name", default="Test PC", help="Name that the devices show")
    parser.add_argument("--echo", action="store_true", help="Send each received talk back")
    parser.add_argument("--save", metavar="FILE", help="Write the last received talk to a WAV file")
    args = parser.parse_args()
    peer = Peer(args)
    peer.send(HELLO, "255.255.255.255", peer.name)
    print(f"Listening on UDP {PORT}, Ctrl+C to stop")
    try:
        while True:
            peer.pump()
            msg = peer.receive()
            ended = peer.handle(*msg) if msg else False
            if peer.talk and (ended or time.monotonic() - peer.talk["last"] > RX_TIMEOUT):
                peer.finish()
    except KeyboardInterrupt:
        for _ in range(2):
            peer.send(BYE, "255.255.255.255")


if __name__ == "__main__":
    main()
