#include "TalkSession.h"

#include <cstdio>
#include <cstring>

namespace talk {

void Session::begin(uint32_t self, const Paired& paired, uint8_t talkId) {
  self_ = self;
  paired_ = paired;
  peers_ = {};
  talkId_ = talkId;
  outCount_ = 0;
  online_ = tx_ = latched_ = live_ = rx_ = scanning_ = requesting_ = endTone_ = false;
  dirty_ = true;
  endedFrom_ = endedAt_ = fastUntil_ = 0;
  prompt_ = Prompt::None;
  notice_ = Notice::None;
  jitter.reset();
}

void Session::push(Do what, Type type, uint32_t ip, Tone tone) {
  // shortcut: a full list drops actions. 64 covers END x3 to 8 devices and more.
  if (outCount_ < kMaxActions) out_[outCount_++] = {what, type, ip, tone};
}

int Session::take(Action* out, int max) {
  const int n = outCount_ < max ? outCount_ : max;
  std::memcpy(out, out_, n * sizeof(Action));
  std::memmove(out_, out_ + n, (outCount_ - n) * sizeof(Action));
  outCount_ -= n;
  return n;
}

bool Session::takeDirty() {
  const bool d = dirty_;
  dirty_ = false;
  return d;
}

void Session::sendPaired(Type type) {
  uint32_t ips[kMaxPeers];
  const int n = targets(ips, kMaxPeers);
  for (int i = 0; i < n; ++i) send(type, ips[i]);
}

void Session::notify(Notice n, uint32_t now) {
  notice_ = n;
  noticeAt_ = now;
  dirty_ = true;
}

void Session::link(bool up, uint32_t now) {
  if (up == online_) return;
  online_ = up;
  dirty_ = true;
  if (up) {
    fastUntil_ = now;
    scan(now);
    return;
  }
  endTx(false, false, now);
  endRx(now);
  endTone_ = false;
  peers_ = {};
  scanning_ = requesting_ = false;
  prompt_ = Prompt::None;
}

void Session::scan(uint32_t now) {
  scanning_ = true;
  scanAt_ = now;
  hellos_ = 1;
  send(Type::Hello, kBroadcast);
  if (peers_.count == 0) dirty_ = true;
}

void Session::rescan(uint32_t now) {
  if (online_ && !tx_ && !rx_) scan(now);
}

void Session::packet(const Packet& p, uint32_t ip, uint32_t now) {
  if (!online_ || p.id == self_) return;
  if (p.type == Type::Bye) {
    if (rx_ && p.id == rxFrom_) endRx(now);
    if (prompt_ == Prompt::Pair && p.id == promptId_) prompt_ = Prompt::None;
    if (requesting_ && p.id == requestId_) requesting_ = false;
    peers_.remove(p.id);
    dirty_ = true;
    return;
  }
  const bool paired = paired_.has(p.id);
  const bool hasName = named(p.type);
  // AUDIO and END come only from paired devices. The sender lost a REFUSE or an ACCEPT. REFUSE removes its pairing.
  if (!hasName && !paired) {
    if (p.type == Type::Audio) send(Type::Refuse, ip);
    return;
  }
  // A paired device that missed our HELLO talks to us. Ask its name.
  if (!hasName && !peers_.find(p.id)) send(Type::Hello, ip);
  addPeer(p.id, ip, hasName ? p.name : nullptr, now);
  if (hasName && paired_.rename(p.id, p.name)) {
    push(Do::Save);
    dirty_ = true;
  }
  switch (p.type) {
    case Type::Hello:
      send(Type::Reply, ip);
      break;
    case Type::Request:
      request(p, ip, now);
      break;
    case Type::Accept:
      if (requesting_ && p.id == requestId_) {
        requesting_ = false;
        // The other device must not keep a one-sided pairing.
        if (!pair(p.id, p.name, now)) send(Type::Refuse, ip);
      } else if (!paired) {
        // This device cancelled the request, or the request timed out.
        send(Type::Refuse, ip);
      }
      break;
    case Type::Refuse:
      refuse(p.id, now);
      break;
    case Type::Audio:
      audio(p, now);
      break;
    case Type::End:
      if (rx_ && p.id == rxFrom_ && p.talk == rxTalk_) {
        endedFrom_ = rxFrom_;
        endedTalk_ = rxTalk_;
        endedAt_ = now;
        endRx(now);
      }
      break;
    default:
      break;
  }
}

void Session::addPeer(uint32_t id, uint32_t ip, const char* name, uint32_t now) {
  // A paired device takes the place of one device that is not paired.
  if (!peers_.find(id) && paired_.has(id) && peers_.count == kMaxPeers) {
    for (int i = 0; i < peers_.count; ++i) {
      if (paired_.has(peers_.items[i].id)) continue;
      peers_.remove(peers_.items[i].id);
      break;
    }
  }
  dirty_ |= peers_.update(id, ip, name, now);
}

bool Session::pair(uint32_t id, const char* name, uint32_t now) {
  if (!paired_.has(id)) {
    if (!paired_.add(id, name)) {
      notify(Notice::Full, now);
      tone(Tone::Fail);
      return false;
    }
    push(Do::Save);
  }
  notify(Notice::Connected, now);
  tone(Tone::Ok);
  return true;
}

void Session::request(const Packet& p, uint32_t ip, uint32_t now) {
  // The other device lost the pairing, for example after Clear data. This device already agreed.
  if (paired_.has(p.id)) {
    send(Type::Accept, ip);
    return;
  }
  // Both users tapped each other.
  if (requesting_ && p.id == requestId_) {
    requesting_ = false;
    send(pair(p.id, p.name, now) ? Type::Accept : Type::Refuse, ip);
    return;
  }
  if (tx_ || rx_ || prompt_ != Prompt::None) return;
  prompt_ = Prompt::Pair;
  promptId_ = p.id;
  promptIp_ = ip;
  promptAt_ = now;
  snprintf(promptName_, sizeof(promptName_), "%s", p.name[0] ? p.name : "?");
  push(Do::Wake);
  tone(Tone::Ask);
  dirty_ = true;
}

void Session::refuse(uint32_t id, uint32_t now) {
  if (requesting_ && id == requestId_) {
    requesting_ = false;
    notify(Notice::Refused, now);
    tone(Tone::Fail);
  } else if (prompt_ == Prompt::Pair && id == promptId_) {
    // The other device cancelled its request.
    prompt_ = Prompt::None;
    dirty_ = true;
  } else if (paired_.remove(id)) {
    // The other device removed the pairing.
    push(Do::Save);
    dirty_ = true;
    uint32_t ip;
    if (tx_ && targets(&ip, 1) == 0) {
      endTx(false, false, now);
      notify(Notice::Nobody, now);
      tone(Tone::Fail);
    }
  }
}

void Session::audio(const Packet& p, uint32_t now) {
  if (p.id == endedFrom_ && p.talk == endedTalk_ && now - endedAt_ < kLateMs) return;
  if (tx_) {
    // Two devices started at the same time. The lower id keeps the channel.
    if (p.id > self_) return;
    endTx(true, false, now);
    notify(Notice::Busy, now);
    tone(Tone::Busy);
  }
  if (!rx_) {
    rx_ = true;
    rxFrom_ = p.id;
    endTone_ = false;
    jitter.reset();
    push(Do::Wake);
    dirty_ = true;
  } else if (p.id != rxFrom_) {
    return;
  }
  rxTalk_ = p.talk;
  rxLast_ = now;
  jitter.push(p.frame);
}

void Session::endRx(uint32_t now) {
  if (!rx_) return;
  rx_ = false;
  jitter.end();
  // The end tone waits until the speaker played the rest of the talk.
  endTone_ = true;
  fastUntil_ = now + kFastMs;
  dirty_ = true;
}

void Session::green(bool press, bool down, uint32_t now) {
  if (!online_ || prompt_ != Prompt::None) return;
  if (tx_) {
    if (latched_) {
      if (press) endTx(true, true, now);
    } else if (!down && now - txStart_ < kTapMs) {
      latched_ = true;
      dirty_ = true;
    } else if (!down) {
      endTx(true, true, now);
    }
    return;
  }
  if (!press) return;
  if (rx_) {
    notify(Notice::Busy, now);
    tone(Tone::Busy);
    return;
  }
  uint32_t ip;
  if (targets(&ip, 1) == 0) {
    notify(Notice::Nobody, now);
    tone(Tone::Fail);
    return;
  }
  tx_ = true;
  latched_ = live_ = false;
  // The rest of a received talk must not play after this talk.
  jitter.reset();
  endTone_ = false;
  ++talkId_;
  txStart_ = now;
  push(Do::MicOn);
  dirty_ = true;
}

void Session::micFrame(uint32_t now) {
  if (!tx_) return;
  micAt_ = now;
  dirty_ |= !live_;
  live_ = true;
}

void Session::micFailed(uint32_t now) {
  if (!tx_) return;
  endTx(true, false, now);
  notify(Notice::NoMic, now);
  tone(Tone::Fail);
}

void Session::stopTalk(uint32_t now) { endTx(true, false, now); }

void Session::endTx(bool sendEnd, bool endTone, uint32_t now) {
  if (!tx_) return;
  tx_ = false;
  push(Do::MicOff);
  // END has no ACK. The receivers also stop after kRxTimeoutMs.
  for (int i = 0; sendEnd && i < 3; ++i) sendPaired(Type::End);
  if (endTone) tone(Tone::End);
  fastUntil_ = now + kFastMs;
  dirty_ = true;
}

void Session::tap(uint32_t id, uint32_t now) {
  if (!online_ || prompt_ != Prompt::None || tx_ || rx_ || paired_.has(id)) return;
  const Peer* p = peers_.find(id);
  if (!p) return;
  // One request at a time. A second tap on the same device cancels it.
  if (requesting_) {
    if (id != requestId_) return;
    requesting_ = false;
    send(Type::Refuse, p->ip);
  } else {
    requesting_ = true;
    requestId_ = id;
    requestAt_ = now;
    send(Type::Request, p->ip);
  }
  dirty_ = true;
}

void Session::hold(uint32_t id, uint32_t now) {
  if (prompt_ != Prompt::None || tx_ || !paired_.has(id)) return;
  prompt_ = Prompt::Forget;
  promptId_ = id;
  promptAt_ = now;
  snprintf(promptName_, sizeof(promptName_), "%s", paired_.name(id));
  dirty_ = true;
}

void Session::answer(bool yes, uint32_t now) {
  if (prompt_ == Prompt::None) return;
  const Prompt was = prompt_;
  prompt_ = Prompt::None;
  dirty_ = true;
  if (was == Prompt::Pair) {
    const bool ok = yes && pair(promptId_, promptName_, now);
    // A full device list did not take the asker. Now it is paired, so it gets a place.
    if (ok) addPeer(promptId_, promptIp_, promptName_, now);
    send(ok ? Type::Accept : Type::Refuse, promptIp_);
  } else if (yes && paired_.remove(promptId_)) {
    push(Do::Save);
    notify(Notice::Forgotten, now);
    // The other device removes the pairing too.
    if (const Peer* p = peers_.find(promptId_)) send(Type::Refuse, p->ip);
  }
}

void Session::tick(uint32_t now, bool speakerIdle) {
  if (notice_ != Notice::None && now - noticeAt_ >= kNoticeMs) {
    notice_ = Notice::None;
    dirty_ = true;
  }
  if (endTone_ && !rx_ && jitter.size() == 0 && speakerIdle) {
    endTone_ = false;
    tone(Tone::End);
  }
  if (!online_) return;
  if (requesting_ && now - requestAt_ >= kRequestMs) {
    requesting_ = false;
    notify(Notice::NoAnswer, now);
    tone(Tone::Fail);
  }
  if (prompt_ != Prompt::None && now - promptAt_ >= kPromptMs) {
    prompt_ = Prompt::None;
    dirty_ = true;
  }
  if (rx_ && now - rxLast_ >= kRxTimeoutMs) endRx(now);
  // The app stamps the frames after it reads now, so micAt_ can be later than now.
  if (tx_ && due((live_ ? micAt_ : txStart_) + kMicTimeoutMs, now)) micFailed(now);
  if (tx_ && now - txStart_ >= kMaxTalkMs) endTx(true, true, now);
  if (scanning_) {
    if (hellos_ < 2 && now - scanAt_ >= kHelloAgainMs) {
      send(Type::Hello, kBroadcast);
      ++hellos_;
    }
    if (now - scanAt_ >= kScanMs) {
      // A listener sends nothing during a talk. It stays a target until the next scan.
      if (!tx_) dirty_ |= peers_.prune(scanAt_);
      dirty_ |= peers_.count == 0;
      scanning_ = false;
      nextScan_ = now + kRescanMs;
    }
  } else if (!tx_ && !rx_ && due(nextScan_, now)) {
    scan(now);
  }
}

void Session::leave(uint32_t now) {
  endTx(true, false, now);
  if (!online_) return;
  for (int i = 0; i < 2; ++i) send(Type::Bye, kBroadcast);
}

const char* Session::talker() const {
  if (!rx_) return "";
  const Peer* p = peers_.find(rxFrom_);
  if (p && p->name[0]) return p->name;
  const char* name = paired_.name(rxFrom_);
  return name ? name : "";
}

int Session::rows(Row* out, int max) const {
  int n = 0;
  for (int i = 0; i < peers_.count && n < max; ++i) {
    const Peer& p = peers_.items[i];
    const Status s = paired_.has(p.id)                   ? Status::Paired
                     : requesting_ && p.id == requestId_ ? Status::Waiting
                                                         : Status::New;
    out[n++] = {p.id, p.name, s};
  }
  for (int i = 0; i < paired_.count && n < max; ++i) {
    if (!peers_.find(paired_.ids[i])) out[n++] = {paired_.ids[i], paired_.names[i], Status::Offline};
  }
  return n;
}

int Session::targets(uint32_t* ips, int max) const {
  int n = 0;
  for (int i = 0; i < peers_.count && n < max; ++i) {
    if (paired_.has(peers_.items[i].id)) ips[n++] = peers_.items[i].ip;
  }
  return n;
}

}  // namespace talk
