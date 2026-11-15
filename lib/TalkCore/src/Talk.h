#pragma once

#include <cstddef>
#include <cstdint>

// The walkie-talkie protocol. All devices use one UDP port. HELLO goes to the broadcast address. The other packets
// go to each peer. Any host in the network can send packets, so parse() checks each one.
namespace talk {

constexpr uint16_t kPort = 47800;
constexpr uint8_t kVersion = 1;
constexpr int kRate = 16000;
// 20 ms of 16-bit mono sound.
constexpr int kFrame = 320;
constexpr int kNameLen = 16;
constexpr int kMaxPeers = 8;
constexpr int kMaxPaired = 16;
// Magic "PW", version, type, sender id (uint32 LE), talk id.
constexpr size_t kHeader = 9;
constexpr size_t kMaxPacket = kHeader + kFrame * 2;

// True when the time at has come. Both times come from millis(). The result is correct across the millis() wrap.
inline bool due(uint32_t at, uint32_t now) { return static_cast<int32_t>(now - at) >= 0; }

// HELLO asks all devices to answer with REPLY. AUDIO carries one frame. END closes a talk. BYE: the sender leaves.
// REQUEST asks to pair. ACCEPT agrees. REFUSE refuses or cancels a request, or removes a pairing.
enum class Type : uint8_t { Hello = 1, Reply, Bye, Audio, End, Request, Accept, Refuse };

struct Packet {
  Type type = Type::Hello;
  uint32_t id = 0;
  uint8_t talk = 0;
  char name[kNameLen] = {};
  int16_t frame[kFrame] = {};
};

// True for the types that carry the name of the sender.
bool named(Type t);
// Writes the packet to out (kMaxPacket bytes) and returns its length. AUDIO sends frame. The named types send name.
size_t encode(const Packet& p, uint8_t* out);
// Rejects a packet with a wrong magic, version, type or length. The name always ends with NUL. A character that is
// not printable ASCII changes to '?', so the fonts can show the name.
bool parse(const uint8_t* in, size_t len, Packet& p);

struct Peer {
  uint32_t id, ip;
  char name[kNameLen];
  uint32_t seen;
};

// The other devices in the order they answered. A full list ignores new devices.
struct Peers {
  Peer items[kMaxPeers] = {};
  int count = 0;

  Peer* find(uint32_t id);
  const Peer* find(uint32_t id) const;
  // Adds the device or updates it. name nullptr keeps the old name. Returns true when the list shows a change.
  bool update(uint32_t id, uint32_t ip, const char* name, uint32_t now);
  bool remove(uint32_t id);
  // Removes the devices that sent nothing since the time since. Returns true when it removed one.
  bool prune(uint32_t since);
};

// The devices that the user allowed, with their last names. The app saves this struct in NVS.
// A pairing is a consent step, not a protection: any host in the network can send a packet with a paired id.
struct Paired {
  uint8_t version = 2;
  uint8_t count = 0;
  uint32_t ids[kMaxPaired] = {};
  char names[kMaxPaired][kNameLen] = {};

  bool has(uint32_t id) const;
  // Returns nullptr for a device that is not paired.
  const char* name(uint32_t id) const;
  // Returns false when the id is already in the list or the list is full.
  bool add(uint32_t id, const char* name);
  // Returns true when the name changed.
  bool rename(uint32_t id, const char* name);
  bool remove(uint32_t id);
  bool valid() const;
};

// A FIFO of received frames. Playback starts after kPrebuffer frames, so short network delays do not cut the sound.
class Jitter {
 public:
  static constexpr int kSlots = 16;
  static constexpr int kPrebuffer = 5;
  // The speaker takes 3 frames (60 ms) for each request.
  static constexpr int kChunk = 3;
  // 5 ms ramps at the start and at the end prevent clicks.
  static constexpr int kFade = 80;

  void reset();
  // A full buffer drops its oldest frame.
  void push(const int16_t* frame);
  // The talk ended. pop() plays out the frames that are left.
  void end();
  // Writes kChunk frames. Returns false while it buffers and when no sound is left after end(). During a talk it
  // waits for a full chunk. speakerEmpty tells that the speaker ran out of sound: then it buffers again.
  bool pop(int16_t* chunk, bool speakerEmpty);
  int size() const { return count_; }

 private:
  int16_t frames_[kSlots][kFrame] = {};
  int head_ = 0, count_ = 0;
  bool playing_ = false, ended_ = false;
};

// Cleans the microphone sound before it goes out. A one-pole high-pass removes hum below about 100 Hz. The gain
// control only lowers a loud voice. It never raises the level, because a near device plays the voice back into the
// microphone. A gate lowers the sound in the pauses by 12 dB, so that echo dies out. A soft limiter replaces the
// hard clip.
class Voice {
 public:
  void reset();
  // Returns false and keeps the frame while the codec gives only zero samples. That takes up to 1 s after power-up.
  bool process(int16_t* frame);
  float gain() const { return gain_; }

 private:
  float x1_ = 0, y1_ = 0, gain_ = 1, gate_ = 1;
  bool live_ = false;
};

}  // namespace talk
