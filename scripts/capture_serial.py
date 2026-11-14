#!/usr/bin/env python3
"""Append the Papwick serial log to a file across USB disconnects."""

import argparse
import time

import serial


def capture(port_name, output):
    with open(output, "ab", buffering=0) as log:
        while True:
            port = serial.Serial(baudrate=115200, timeout=0, exclusive=True)
            try:
                port.dtr = False
                port.rts = False
                port.port = port_name
                port.open()
                print(f"Connected: {port_name}", flush=True)
                while True:
                    data = port.read(4096)
                    if data:
                        log.write(data)
                    else:
                        time.sleep(0.01)
            except serial.SerialException as error:
                print(f"Serial disconnected: {error}; waiting to reconnect", flush=True)
                time.sleep(1)
            finally:
                port.close()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("port", help="Serial port, for example /dev/ttyACM0")
    parser.add_argument("output", help="Log file to append")
    args = parser.parse_args()
    try:
        capture(args.port, args.output)
    except KeyboardInterrupt:
        pass


if __name__ == "__main__":
    main()
