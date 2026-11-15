#include "Talk.h"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace talk {

namespace {

size_t payloadSize(Type t) {
  if (t == Type::Audio) return kFrame * 2;
  return named(t) ? kNameLen : 0;
}

void copyName(char* out, const char* name) {
  std::strncpy(out, name, kNameLen - 1);
  out[kNameLen - 1] = '\0';
}

}  // namespace

bool named(Type t) {
  return t == Type::Hello || t == Type::Reply || t == Type::Request || t == Type::Accept || t == Type::Refuse;
}

size_t encode(const Packet& p, uint8_t* out) {
  out[0] = 'P';
  out[1] = 'W';
  out[2] = kVersion;
  out[3] = static_cast<uint8_t>(p.type);
  for (int i = 0; i < 4; ++i) out[4 + i] = static_cast<uint8_t>(p.id >> (8 * i));
  out[8] = p.talk;
  uint8_t* payload = out + kHeader;
  if (p.type == Type::Audio) {
    for (int i = 0; i < kFrame; ++i) {
      const auto v = static_cast<uint16_t>(p.frame[i]);
      payload[2 * i] = static_cast<uint8_t>(v);
      payload[2 * i + 1] = static_cast<uint8_t>(v >> 8);
    }
  } else if (named(p.type)) {
    std::memset(payload, 0, kNameLen);
    std::strncpy(reinterpret_cast<char*>(payload), p.name, kNameLen - 1);
  }
  return kHeader + payloadSize(p.type);
}

bool parse(const uint8_t* in, size_t len, Packet& p) {
  if (len < kHeader || in[0] != 'P' || in[1] != 'W' || in[2] != kVersion) return false;
  if (in[3] < static_cast<uint8_t>(Type::Hello) || in[3] > static_cast<uint8_t>(Type::Refuse)) return false;
  const auto type = static_cast<Type>(in[3]);
  if (len != kHeader + payloadSize(type)) return false;
  p.type = type;
  p.id = in[4] | in[5] << 8 | in[6] << 16 | static_cast<uint32_t>(in[7]) << 24;
  p.talk = in[8];
  const uint8_t* payload = in + kHeader;
  if (type == Type::Audio) {
    for (int i = 0; i < kFrame; ++i) p.frame[i] = static_cast<int16_t>(payload[2 * i] | payload[2 * i + 1] << 8);
  } else if (named(type)) {
    std::memcpy(p.name, payload, kNameLen);
    p.name[kNameLen - 1] = '\0';
    for (char* c = p.name; *c; ++c) {
      if (*c < 0x20 || *c > 0x7E) *c = '?';
    }
  }
  return true;
}

Peer* Peers::find(uint32_t id) {
  for (int i = 0; i < count; ++i) {
    if (items[i].id == id) return &items[i];
  }
  return nullptr;
}

const Peer* Peers::find(uint32_t id) const { return const_cast<Peers*>(this)->find(id); }

bool Peers::update(uint32_t id, uint32_t ip, const char* name, uint32_t now) {
  Peer* p = find(id);
  bool changed = false;
  if (!p) {
    if (count == kMaxPeers) return false;
    p = &items[count++];
    *p = {id, ip, {}, now};
    changed = true;
  }
  p->ip = ip;
  p->seen = now;
  if (name && std::strncmp(p->name, name, kNameLen - 1) != 0) {
    copyName(p->name, name);
    changed = true;
  }
  return changed;
}

bool Peers::remove(uint32_t id) {
  Peer* p = find(id);
  if (!p) return false;
  std::memmove(p, p + 1, (items + count - p - 1) * sizeof(Peer));
  --count;
  return true;
}

bool Peers::prune(uint32_t since) {
  const int before = count;
  int kept = 0;
  for (int i = 0; i < count; ++i) {
    if (due(since, items[i].seen)) items[kept++] = items[i];
  }
  count = kept;
  return count != before;
}

bool Paired::has(uint32_t id) const { return name(id) != nullptr; }

const char* Paired::name(uint32_t id) const {
  for (int i = 0; i < count; ++i) {
    if (ids[i] == id) return names[i];
  }
  return nullptr;
}

bool Paired::add(uint32_t id, const char* name) {
  if (has(id) || count == kMaxPaired) return false;
  ids[count] = id;
  copyName(names[count++], name);
  return true;
}

bool Paired::rename(uint32_t id, const char* name) {
  for (int i = 0; i < count; ++i) {
    if (ids[i] != id || std::strncmp(names[i], name, kNameLen - 1) == 0) continue;
    copyName(names[i], name);
    return true;
  }
  return false;
}

bool Paired::remove(uint32_t id) {
  for (int i = 0; i < count; ++i) {
    if (ids[i] != id) continue;
    std::memmove(&ids[i], &ids[i + 1], (count - i - 1) * sizeof(ids[0]));
    std::memmove(names[i], names[i + 1], (count - i - 1) * sizeof(names[0]));
    --count;
    return true;
  }
  return false;
}

bool Paired::valid() const {
  if (version != Paired{}.version || count > kMaxPaired) return false;
  for (int i = 0; i < count; ++i) {
    if (!std::memchr(names[i], '\0', kNameLen)) return false;
  }
  return true;
}

void Jitter::reset() {
  head_ = count_ = 0;
  playing_ = ended_ = false;
}

void Jitter::push(const int16_t* frame) {
  if (count_ == kSlots) {
    head_ = (head_ + 1) % kSlots;
    --count_;
  }
  std::memcpy(frames_[(head_ + count_++) % kSlots], frame, sizeof(frames_[0]));
}

void Jitter::end() { ended_ = true; }

bool Jitter::pop(int16_t* chunk, bool speakerEmpty) {
  if (count_ == 0 && ended_) {
    reset();
    return false;
  }
  if (!ended_ && count_ < (playing_ ? kChunk : kPrebuffer)) {
    if (playing_ && speakerEmpty) playing_ = false;
    return false;
  }
  const bool first = !playing_;
  playing_ = true;
  int real = 0;
  for (int i = 0; i < kChunk; ++i) {
    if (count_ == 0) {
      std::memset(chunk + i * kFrame, 0, sizeof(frames_[0]));
      continue;
    }
    std::memcpy(chunk + i * kFrame, frames_[head_], sizeof(frames_[0]));
    head_ = (head_ + 1) % kSlots;
    --count_;
    ++real;
  }
  if (first) {
    for (int i = 0; i < kFade; ++i) chunk[i] = static_cast<int16_t>(chunk[i] * i / kFade);
  }
  if (ended_ && count_ == 0) {
    int16_t* tail = chunk + real * kFrame - kFade;
    for (int i = 0; i < kFade; ++i) tail[i] = static_cast<int16_t>(tail[i] * (kFade - 1 - i) / kFade);
  }
  return true;
}

void Voice::reset() {
  x1_ = y1_ = 0;
  gain_ = gate_ = 1;
  live_ = false;
}

bool Voice::process(int16_t* frame) {
  // a = 1 / (1 + 2 * pi * 100 Hz / 16 kHz).
  constexpr float kHighPass = 0.9622f;
  constexpr float kTarget = 12000, kGate = 400, kMinGain = 0.5f, kMaxGain = 1, kRelease = 0.02f;
  // The gate opens at once for a voice and closes in about 120 ms.
  constexpr float kOpen = 1000, kFloor = 0.25f, kClose = 0.8f;
  constexpr float kKnee = 20000, kTop = 32767;
  if (!live_) {
    live_ = std::any_of(frame, frame + kFrame, [](int16_t v) { return v != 0; });
    if (!live_) return false;
  }
  float filtered[kFrame];
  float peak = 0;
  for (int i = 0; i < kFrame; ++i) {
    y1_ = kHighPass * (y1_ + frame[i] - x1_);
    x1_ = frame[i];
    filtered[i] = y1_;
    peak = std::max(peak, std::fabs(y1_));
  }
  // The gain falls at once for a loud frame, and rises slowly. Silence keeps the gain, so the noise does not grow.
  float next = gain_;
  if (peak > kGate) {
    const float want = std::clamp(kTarget / peak, kMinGain, kMaxGain);
    next = want < gain_ ? want : gain_ + (want - gain_) * kRelease;
  }
  const float nextGate = peak >= kOpen ? 1 : std::max(kFloor, gate_ * kClose);
  const float from = gain_ * gate_, to = next * nextGate;
  for (int i = 0; i < kFrame; ++i) {
    float v = filtered[i] * (from + (to - from) * i / kFrame);
    const float over = std::fabs(v) - kKnee;
    if (over > 0) v = std::copysign(kKnee + over / (1 + over / (kTop - kKnee)), v);
    frame[i] = static_cast<int16_t>(std::lround(std::clamp(v, -kTop, kTop)));
  }
  gain_ = next;
  gate_ = nextGate;
  return true;
}

}  // namespace talk
