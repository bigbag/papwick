#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

#include "TalkSession.h"
#include "test_utils.h"

using namespace talk;

static void fill(int16_t* frame, int16_t v) {
  for (int i = 0; i < kFrame; ++i) frame[i] = v;
}

static void testJitter(TestUtils::TestRunner& runner) {
  static Jitter jitter;
  int16_t frame[kFrame], chunk[kFrame * Jitter::kChunk];
  jitter.reset();
  for (int i = 1; i < Jitter::kPrebuffer; ++i) {
    fill(frame, static_cast<int16_t>(i * 1000));
    jitter.push(frame);
    runner.expectFalse(jitter.pop(chunk, true), "prebuffer holds the sound");
  }
  fill(frame, Jitter::kPrebuffer * 1000);
  jitter.push(frame);
  runner.expectTrue(
      jitter.pop(chunk, true) && chunk[Jitter::kFade] == 1000 && chunk[kFrame] == 2000 && chunk[2 * kFrame] == 3000,
      "play in order after the prebuffer");
  runner.expectTrue(chunk[0] == 0 && chunk[Jitter::kFade / 2] == 500, "the first chunk fades in");
  runner.expectFalse(jitter.pop(chunk, false) || jitter.size() != 2, "no silence in the middle of a talk");
  fill(frame, 6000);
  jitter.push(frame);
  runner.expectTrue(jitter.pop(chunk, false) && chunk[0] == 4000 && chunk[2 * kFrame] == 6000,
                    "a full chunk plays at once");
  jitter.push(frame);
  runner.expectFalse(jitter.pop(chunk, true), "an empty speaker starts a new prebuffer");
  for (int i = 0; i < 3; ++i) jitter.push(frame);
  runner.expectFalse(jitter.pop(chunk, true), "the new prebuffer waits");
  jitter.push(frame);
  runner.expectTrue(jitter.pop(chunk, true) && chunk[0] == 0 && chunk[Jitter::kFade] == 6000,
                    "after the prebuffer the sound fades in again");
  jitter.end();
  runner.expectTrue(jitter.pop(chunk, false) && chunk[kFrame] == 6000 && chunk[2 * kFrame - Jitter::kFade] < 6000 &&
                        chunk[2 * kFrame - 1] == 0 && chunk[2 * kFrame] == 0,
                    "the end plays the rest with a fade out");
  runner.expectFalse(jitter.pop(chunk, true) || jitter.size(), "end with no sound left");
  for (int i = 0; i < 2; ++i) jitter.push(frame);
  runner.expectFalse(jitter.pop(chunk, true), "after the end, a new talk buffers again");
  jitter.end();
  runner.expectTrue(jitter.pop(chunk, true) && chunk[2 * kFrame] == 0, "end plays a short talk");
  runner.expectFalse(jitter.pop(chunk, true) || jitter.size(), "short talk drained");
  for (int i = 0; i < Jitter::kSlots + 2; ++i) {
    fill(frame, static_cast<int16_t>(i * 100));
    jitter.push(frame);
  }
  runner.expectTrue(jitter.size() == Jitter::kSlots && jitter.pop(chunk, true) && chunk[Jitter::kFade] == 200,
                    "a full buffer drops the oldest frames");
}

static int peakOf(const int16_t* frame) {
  int peak = 0;
  for (int i = 0; i < kFrame; ++i) peak = std::max(peak, std::abs(static_cast<int>(frame[i])));
  return peak;
}

static void sine(int16_t* frame, double amplitude) {
  for (int i = 0; i < kFrame; ++i) frame[i] = static_cast<int16_t>(amplitude * std::sin(2 * M_PI * i / 16));
}

static void testVoice(TestUtils::TestRunner& runner) {
  Voice voice;
  int16_t frame[kFrame];
  voice.reset();
  fill(frame, 0);
  runner.expectFalse(voice.process(frame), "zero frames after power-up are dropped");
  fill(frame, 5000);
  runner.expectTrue(voice.process(frame), "the first sound goes out");
  for (int i = 0; i < 5; ++i) {
    fill(frame, 5000);
    voice.process(frame);
  }
  runner.expectTrue(peakOf(frame) < 50, "the high-pass removes a constant offset");
  fill(frame, 0);
  runner.expectTrue(voice.process(frame), "zero frames in a talk go out");

  voice.reset();
  int peak = 0;
  for (int i = 0; i < 50; ++i) {
    sine(frame, 32000);
    voice.process(frame);
    peak = std::max(peak, peakOf(frame));
  }
  runner.expectTrue(peak < 30000, "the limiter keeps a loud voice under full scale");
  runner.expectTrue(voice.gain() == 0.5f && peakOf(frame) < 17000, "a loud voice gets less gain");

  voice.reset();
  for (int i = 0; i < 300; ++i) {
    sine(frame, 1500);
    voice.process(frame);
  }
  runner.expectTrue(voice.gain() == 1, "the gain control does not raise a quiet voice");
  runner.expectTrue(peakOf(frame) > 1400, "the gate stays open for a voice");
  const float gain = voice.gain();
  for (int i = 0; i < 50; ++i) {
    sine(frame, 100);
    voice.process(frame);
  }
  runner.expectTrue(voice.gain() == gain, "noise under the gate keeps the gain");
  runner.expectTrue(peakOf(frame) < 30, "the gate lowers the pauses by 12 dB");
  sine(frame, 2000);
  voice.process(frame);
  sine(frame, 2000);
  voice.process(frame);
  runner.expectTrue(peakOf(frame) > 1900, "the gate opens at once for a voice");
}

namespace {

constexpr uint32_t kSelf = 50, kLow = 10, kHigh = 90, kNew = 70;

uint32_t ipOf(uint32_t id) { return 1000 + id; }

Packet pkt(Type type, uint32_t id, uint8_t talk = 0, const char* name = nullptr) {
  Packet p;
  p.type = type;
  p.id = id;
  p.talk = talk;
  std::snprintf(p.name, sizeof(p.name), "%s", name ? name : (id == kLow ? "Low" : id == kHigh ? "High" : "New"));
  fill(p.frame, 1000);
  return p;
}

void recv(Session& s, Type type, uint32_t id, uint32_t now, uint8_t talk = 0) {
  s.packet(pkt(type, id, talk), ipOf(id), now);
}

std::vector<Action> take(Session& s) {
  std::vector<Action> out(Session::kMaxActions);
  out.resize(s.take(out.data(), Session::kMaxActions));
  return out;
}

int count(const std::vector<Action>& acts, Do what, Type type = Type::Hello, uint32_t ip = 0) {
  int n = 0;
  for (const Action& a : acts) {
    if (a.what != what) continue;
    if (what == Do::Send && (a.type != type || (ip && a.ip != ip))) continue;
    if (what == Do::Tone && a.tone != static_cast<Tone>(static_cast<uint8_t>(type))) continue;
    ++n;
  }
  return n;
}

int sent(const std::vector<Action>& acts, Type type, uint32_t id = 0) {
  return count(acts, Do::Send, type, id ? ipOf(id) : 0);
}

bool toned(const std::vector<Action>& acts, Tone t) {
  return count(acts, Do::Tone, static_cast<Type>(static_cast<uint8_t>(t))) == 1;
}

Status statusOf(const Session& s, uint32_t id) {
  Row rows[kMaxPeers + kMaxPaired];
  const int n = s.rows(rows, kMaxPeers + kMaxPaired);
  for (int i = 0; i < n; ++i) {
    if (rows[i].id == id) return rows[i].status;
  }
  return static_cast<Status>(255);
}

// Online at time 0 with kLow and kHigh paired and online, and kNew online.
void start(Session& s, bool withPeers = true) {
  Paired paired;
  paired.add(kLow, "Low");
  paired.add(kHigh, "High");
  s.begin(kSelf, paired, 0);
  s.link(true, 0);
  if (withPeers) {
    for (uint32_t id : {kLow, kHigh, kNew}) recv(s, Type::Reply, id, 100);
  }
  take(s);
}

void drain(Session& s) {
  int16_t chunk[kFrame * Jitter::kChunk];
  while (s.jitter.pop(chunk, true)) {
  }
}

}  // namespace

static void testSession(TestUtils::TestRunner& runner) {
  static Session s;
  Paired none;
  s.begin(kSelf, none, 0);
  s.link(true, 0);
  auto acts = take(s);
  runner.expectTrue(count(acts, Do::Send, Type::Hello, kBroadcast) == 1 && s.scanning(), "link up sends HELLO");
  recv(s, Type::Hello, kNew, 500);
  acts = take(s);
  runner.expectTrue(sent(acts, Type::Reply, kNew) == 1 && statusOf(s, kNew) == Status::New, "HELLO gets a REPLY");
  s.tick(999, true);
  runner.expectTrue(take(s).empty(), "no HELLO before 1 s");
  s.tick(1000, true);
  runner.expectTrue(sent(take(s), Type::Hello) == 1, "a second HELLO after 1 s");
  s.tick(2000, true);
  runner.expectTrue(sent(take(s), Type::Hello) == 0, "only two HELLO for each scan");
  s.tick(3000, true);
  runner.expectTrue(!s.scanning() && statusOf(s, kNew) == Status::New, "the scan keeps devices that answered");
  s.tick(33000, true);
  runner.expectTrue(sent(take(s), Type::Hello) == 1, "a new scan after 30 s");
  s.tick(36000, true);
  runner.expectTrue(statusOf(s, kNew) != Status::New, "the scan removes devices that did not answer");

  start(s);
  runner.expectTrue(statusOf(s, kLow) == Status::Paired && statusOf(s, kNew) == Status::New, "rows show the status");
  recv(s, Type::Bye, kHigh, 200);
  runner.expectTrue(statusOf(s, kHigh) == Status::Offline, "a paired device that left is offline");
  recv(s, Type::Hello, kLow, 300);
  take(s);
  s.packet(pkt(Type::Hello, kLow, 0, "Low2"), ipOf(kLow), 400);
  acts = take(s);
  runner.expectTrue(count(acts, Do::Save) == 1 && std::strcmp(s.paired().name(kLow), "Low2") == 0,
                    "a new name of a paired device is saved");

  // Talk.
  start(s);
  s.green(true, true, 1000);
  acts = take(s);
  runner.expectTrue(s.talking() && !s.live() && count(acts, Do::MicOn) == 1 && count(acts, Do::Tone) == 0,
                    "green starts a talk without a tone");
  s.micFrame(1200);
  runner.expectTrue(s.live(), "the first sound makes the talk live");
  s.green(false, false, 1300);
  runner.expectTrue(s.talking() && s.latched(), "a short push keeps the talk on");
  s.green(false, false, 1400);
  runner.expectTrue(s.talking(), "no input keeps the talk on");
  s.green(true, true, 5000);
  acts = take(s);
  runner.expectTrue(!s.talking() && count(acts, Do::MicOff) == 1 && sent(acts, Type::End, kLow) == 3 &&
                        sent(acts, Type::End, kHigh) == 3 && sent(acts, Type::End, kNew) == 0 && toned(acts, Tone::End),
                    "the next push ends the talk with END to the paired devices");
  runner.expectTrue(s.talkId() == 1, "each talk gets a new id");
  runner.expectTrue(s.fast(7999) && !s.fast(8000), "the radio stays fast for 3 s after a talk");
  s.green(true, true, 10000);
  s.green(false, true, 10500);
  s.green(false, false, 11000);
  runner.expectTrue(!s.talking(), "the release ends a long hold");
  s.green(true, true, 20000);
  take(s);
  s.micFrame(20999);
  s.tick(20999, true);
  s.takeDirty();
  s.micFrame(21000);
  s.tick(21000, true);
  runner.expectFalse(s.takeDirty(), "the screen stays the same during a talk");
  take(s);
  s.micFrame(55000);
  s.tick(55000, true);
  runner.expectTrue(sent(take(s), Type::Hello) == 0, "no scan during a talk");
  s.micFrame(80000);
  s.tick(80000, true);
  acts = take(s);
  runner.expectTrue(!s.talking() && sent(acts, Type::End) == 6 && toned(acts, Tone::End), "a talk ends after 60 s");
  runner.expectTrue(sent(acts, Type::Hello) == 1, "the delayed scan runs after the talk");
  start(s);
  s.green(true, true, 1000);
  s.tick(2999, true);
  runner.expectTrue(s.talking(), "the microphone can need time to start");
  s.tick(3000, true);
  runner.expectTrue(!s.talking() && s.notice() == Notice::NoMic, "no sound for 2 s from the start stops the talk");
  s.green(true, true, 4000);
  s.micFrame(4500);
  s.tick(4490, true);
  runner.expectTrue(s.talking(), "a frame stamped after the tick time is no timeout");
  s.tick(6499, true);
  runner.expectTrue(s.talking(), "the talk waits for the next frame");
  s.tick(6500, true);
  runner.expectTrue(!s.talking(), "no frame for 2 s stops the talk");
  s.green(true, true, 7000);
  take(s);
  s.micFailed(7010);
  acts = take(s);
  runner.expectTrue(
      !s.talking() && s.notice() == Notice::NoMic && toned(acts, Tone::Fail) && count(acts, Do::MicOff) == 1,
      "a microphone error");
  s.tick(9509, true);
  runner.expectTrue(s.notice() == Notice::NoMic, "the notice stays");
  s.tick(9510, true);
  runner.expectTrue(s.notice() == Notice::None, "the notice closes after 2.5 s");

  start(s, false);
  recv(s, Type::Reply, kNew, 100);
  take(s);
  s.green(true, true, 1000);
  acts = take(s);
  runner.expectTrue(!s.talking() && s.notice() == Notice::Nobody && toned(acts, Tone::Fail),
                    "no talk without a paired device online");

  // Receive.
  start(s);
  recv(s, Type::Audio, kHigh, 1000, 3);
  acts = take(s);
  runner.expectTrue(s.receiving() && count(acts, Do::Wake) == 1 && s.jitter.size() == 1, "AUDIO starts a receive");
  runner.expectEqual("High", s.talker(), "the screen shows the talker");
  runner.expectTrue(s.fast(1000), "the radio is fast during a receive");
  recv(s, Type::Audio, kLow, 1010, 5);
  runner.expectTrue(s.jitter.size() == 1, "a second talker is ignored");
  s.green(true, true, 1020);
  acts = take(s);
  runner.expectTrue(!s.talking() && s.notice() == Notice::Busy && toned(acts, Tone::Busy), "green during a receive");
  recv(s, Type::End, kHigh, 1030, 4);
  runner.expectTrue(s.receiving(), "END of another talk is ignored");
  recv(s, Type::End, kHigh, 1040, 3);
  s.tick(1040, true);
  acts = take(s);
  runner.expectTrue(!s.receiving() && !toned(acts, Tone::End), "END stops the receive, the speaker plays the rest");
  drain(s);
  s.tick(1100, false);
  runner.expectFalse(toned(take(s), Tone::End), "the end tone waits for the speaker");
  s.tick(1110, true);
  runner.expectTrue(toned(take(s), Tone::End), "the end tone after the last sound");
  recv(s, Type::Audio, kHigh, 1200, 3);
  runner.expectFalse(s.receiving(), "late AUDIO of an ended talk is ignored");
  s.green(true, true, 1300);
  recv(s, Type::Audio, kHigh, 1400, 3);
  runner.expectTrue(s.talking() && !s.receiving(), "late AUDIO is no collision");
  s.stopTalk(1500);
  acts = take(s);
  runner.expectTrue(!s.talking() && sent(acts, Type::End) == 6 && !toned(acts, Tone::End), "stop without a tone");
  recv(s, Type::Audio, kHigh, 2040, 3);
  runner.expectTrue(s.receiving(), "the same talk id after 1 s is a new talk");
  s.tick(2539, true);
  runner.expectTrue(s.receiving(), "a receive waits for 500 ms");
  s.tick(2540, true);
  runner.expectFalse(s.receiving(), "a receive stops without sound for 500 ms");
  recv(s, Type::Audio, kHigh, 2600, 3);
  runner.expectTrue(s.receiving(), "the talk continues after a timeout");
  recv(s, Type::Bye, kHigh, 2700);
  runner.expectTrue(!s.receiving() && statusOf(s, kHigh) == Status::Offline, "BYE stops the receive");
  take(s);
  recv(s, Type::Audio, kNew, 3000, 1);
  acts = take(s);
  runner.expectTrue(!s.receiving() && sent(acts, Type::Refuse, kNew) == 1,
                    "AUDIO from a device that is not paired gets REFUSE");
  recv(s, Type::End, kNew, 3010, 1);
  runner.expectTrue(take(s).empty(), "END from a device that is not paired is ignored");
  recv(s, Type::Audio, kHigh, 4000, 9);
  acts = take(s);
  runner.expectTrue(s.receiving() && sent(acts, Type::Hello, kHigh) == 1 && statusOf(s, kHigh) == Status::Paired,
                    "AUDIO from an unknown paired device asks its name");

  // Collision: the lower id keeps the channel.
  start(s);
  s.green(true, true, 1000);
  take(s);
  recv(s, Type::Audio, kHigh, 1100, 1);
  runner.expectTrue(s.talking() && !s.receiving(), "a higher id yields to this device");
  recv(s, Type::Audio, kLow, 1200, 1);
  acts = take(s);
  runner.expectTrue(!s.talking() && s.receiving() && count(acts, Do::MicOff) == 1 && sent(acts, Type::End) == 6 &&
                        toned(acts, Tone::Busy) && s.notice() == Notice::Busy,
                    "this device yields to a lower id");

  start(s);
  for (int i = 0; i < 3; ++i) recv(s, Type::Audio, kHigh, 1000 + i, 1);
  recv(s, Type::End, kHigh, 1010, 1);
  s.green(true, true, 1020);
  s.tick(1030, true);
  runner.expectTrue(s.jitter.size() == 0 && !toned(take(s), Tone::End),
                    "a talk drops the rest of the received talk and its end tone");
  start(s);
  s.tick(3000, true);
  s.rescan(10000);
  s.green(true, true, 10500);
  s.micFrame(12000);
  s.tick(13000, true);
  runner.expectTrue(statusOf(s, kLow) == Status::Paired, "a scan does not remove listeners during a talk");
  s.begin(kSelf, Paired{}, 0);
  s.link(true, 0x80000010u);
  runner.expectFalse(s.fast(0x80000010u), "the radio is not fast after the millis() sign change");
  start(s);
  s.tap(kNew, 1000);
  recv(s, Type::Bye, kNew, 2000);
  s.tick(32000, true);
  runner.expectTrue(s.notice() != Notice::NoAnswer, "BYE of the asked device ends the request");

  // Pairing asked by another device.
  start(s);
  recv(s, Type::Request, kLow, 1000);
  acts = take(s);
  runner.expectTrue(sent(acts, Type::Accept, kLow) == 1 && s.prompt() == Prompt::None,
                    "a paired device gets ACCEPT without a question");
  recv(s, Type::Request, kNew, 1000);
  acts = take(s);
  runner.expectTrue(s.prompt() == Prompt::Pair && count(acts, Do::Wake) == 1 && toned(acts, Tone::Ask),
                    "REQUEST shows a question");
  runner.expectEqual("New", s.promptName(), "the question shows the name");
  s.green(true, true, 1100);
  runner.expectFalse(s.talking(), "green does not talk during a question");
  s.answer(true, 2000);
  acts = take(s);
  runner.expectTrue(s.paired().has(kNew) && count(acts, Do::Save) == 1 && sent(acts, Type::Accept, kNew) == 1 &&
                        toned(acts, Tone::Ok) && s.notice() == Notice::Connected,
                    "yes pairs and sends ACCEPT");
  start(s);
  recv(s, Type::Request, kNew, 1000);
  take(s);
  s.answer(false, 2000);
  acts = take(s);
  runner.expectTrue(!s.paired().has(kNew) && sent(acts, Type::Refuse, kNew) == 1, "no sends REFUSE");
  recv(s, Type::Request, kNew, 3000);
  s.tick(27999, true);
  runner.expectTrue(s.prompt() == Prompt::Pair, "the question waits");
  s.tick(28000, true);
  runner.expectTrue(s.prompt() == Prompt::None, "the question closes after 25 s");
  recv(s, Type::Request, kNew, 30000);
  recv(s, Type::Refuse, kNew, 30100);
  runner.expectTrue(s.prompt() == Prompt::None, "REFUSE of the asker closes the question");
  recv(s, Type::Request, kNew, 31000);
  recv(s, Type::Bye, kNew, 31100);
  runner.expectTrue(s.prompt() == Prompt::None, "BYE of the asker closes the question");
  s.green(true, true, 40000);
  recv(s, Type::Request, kNew, 40100);
  runner.expectTrue(s.prompt() == Prompt::None, "no question during a talk");
  s.stopTalk(40200);
  recv(s, Type::Audio, kLow, 41000, 1);
  recv(s, Type::Request, kNew, 41010);
  runner.expectTrue(s.prompt() == Prompt::None, "no question during a receive");
  start(s);
  recv(s, Type::Request, kNew, 1000);
  s.packet(pkt(Type::Request, 77, 0, "Other"), ipOf(77), 1100);
  runner.expectEqual("New", s.promptName(), "a second REQUEST does not replace the question");
  Paired full;
  for (uint32_t id = 100; full.count < kMaxPaired; ++id) full.add(id, "x");
  s.begin(kSelf, full, 0);
  s.link(true, 0);
  recv(s, Type::Request, kNew, 1000);
  take(s);
  s.answer(true, 2000);
  acts = take(s);
  runner.expectTrue(sent(acts, Type::Refuse, kNew) == 1 && sent(acts, Type::Accept) == 0 &&
                        s.notice() == Notice::Full && toned(acts, Tone::Fail),
                    "a full list sends REFUSE");

  // Pairing asked by this device.
  start(s);
  s.tap(kLow, 1000);
  runner.expectTrue(sent(take(s), Type::Request) == 0, "a tap on a paired device sends nothing");
  s.tap(kNew, 1000);
  acts = take(s);
  runner.expectTrue(sent(acts, Type::Request, kNew) == 1 && statusOf(s, kNew) == Status::Waiting,
                    "a tap sends REQUEST");
  s.packet(pkt(Type::Reply, 77, 0, "Other"), ipOf(77), 1100);
  s.tap(77, 1200);
  runner.expectTrue(sent(take(s), Type::Request) == 0, "one request at a time");
  recv(s, Type::Accept, kNew, 2000);
  acts = take(s);
  runner.expectTrue(s.paired().has(kNew) && count(acts, Do::Save) == 1 && toned(acts, Tone::Ok) &&
                        s.notice() == Notice::Connected && statusOf(s, kNew) == Status::Paired,
                    "ACCEPT pairs");
  start(s);
  recv(s, Type::Accept, kNew, 1000);
  acts = take(s);
  runner.expectTrue(!s.paired().has(kNew) && sent(acts, Type::Refuse, kNew) == 1,
                    "ACCEPT without a request gets REFUSE");
  s.tap(kNew, 2000);
  s.tick(31999, true);
  runner.expectTrue(statusOf(s, kNew) == Status::Waiting, "the request waits");
  s.tick(32000, true);
  acts = take(s);
  runner.expectTrue(statusOf(s, kNew) == Status::New && s.notice() == Notice::NoAnswer && toned(acts, Tone::Fail),
                    "the request ends after 30 s");
  recv(s, Type::Accept, kNew, 32100);
  acts = take(s);
  runner.expectTrue(!s.paired().has(kNew) && sent(acts, Type::Refuse, kNew) == 1, "a late ACCEPT gets REFUSE");
  s.tap(kNew, 40000);
  s.tap(kNew, 41000);
  acts = take(s);
  runner.expectTrue(sent(acts, Type::Refuse, kNew) == 1 && statusOf(s, kNew) == Status::New,
                    "a second tap cancels the request");
  s.tap(kNew, 42000);
  recv(s, Type::Refuse, kNew, 43000);
  acts = take(s);
  runner.expectTrue(s.notice() == Notice::Refused && toned(acts, Tone::Fail) && statusOf(s, kNew) == Status::New,
                    "REFUSE ends the request");
  s.tap(kNew, 44000);
  take(s);
  recv(s, Type::Request, kNew, 44100);
  acts = take(s);
  runner.expectTrue(s.paired().has(kNew) && sent(acts, Type::Accept, kNew) == 1 && s.prompt() == Prompt::None,
                    "two crossed requests pair without a question");
  s.begin(kSelf, full, 0);
  s.link(true, 0);
  recv(s, Type::Reply, kNew, 100);
  s.tap(kNew, 1000);
  take(s);
  recv(s, Type::Accept, kNew, 2000);
  acts = take(s);
  runner.expectTrue(!s.paired().has(kNew) && sent(acts, Type::Refuse, kNew) == 1, "a full list refuses an ACCEPT");

  // Forget.
  start(s);
  s.hold(kNew, 1000);
  runner.expectTrue(s.prompt() == Prompt::None, "hold on a device that is not paired");
  s.hold(kLow, 1000);
  runner.expectTrue(s.prompt() == Prompt::Forget, "hold on a paired device asks to forget it");
  runner.expectEqual("Low", s.promptName(), "the question shows the name");
  s.answer(false, 1100);
  runner.expectTrue(s.paired().has(kLow), "no keeps the device");
  s.hold(kLow, 1200);
  s.answer(true, 1300);
  acts = take(s);
  runner.expectTrue(!s.paired().has(kLow) && count(acts, Do::Save) == 1 && sent(acts, Type::Refuse, kLow) == 1 &&
                        s.notice() == Notice::Forgotten,
                    "yes forgets the device and tells it");
  recv(s, Type::Bye, kHigh, 1350);
  s.hold(kHigh, 1360);
  s.answer(true, 1370);
  acts = take(s);
  runner.expectTrue(!s.paired().has(kHigh) && sent(acts, Type::Refuse) == 0, "forget a device that is offline");
  recv(s, Type::Audio, kHigh, 1380, 1);
  runner.expectTrue(sent(take(s), Type::Refuse, kHigh) == 1, "the forgotten device gets REFUSE when it talks");
  start(s);
  recv(s, Type::Refuse, kHigh, 1400);
  acts = take(s);
  runner.expectTrue(!s.paired().has(kHigh) && count(acts, Do::Save) == 1, "REFUSE of a paired device removes it");
  s.green(true, true, 2000);
  take(s);
  recv(s, Type::Refuse, kLow, 2100);
  acts = take(s);
  runner.expectTrue(!s.talking() && s.notice() == Notice::Nobody && toned(acts, Tone::Fail),
                    "REFUSE of the last listener stops the talk");
  start(s);
  s.green(true, true, 1000);
  recv(s, Type::Refuse, kLow, 1100);
  runner.expectTrue(s.talking(), "the talk continues while a listener is left");

  // Link and list limits.
  start(s);
  s.green(true, true, 1000);
  take(s);
  s.link(false, 1100);
  acts = take(s);
  runner.expectTrue(!s.online() && !s.talking() && count(acts, Do::MicOff) == 1 && sent(acts, Type::End) == 0,
                    "link loss stops the talk");
  runner.expectTrue(statusOf(s, kLow) == Status::Offline && statusOf(s, kNew) != Status::New,
                    "link loss clears the device list");
  s.tick(40000, true);
  runner.expectTrue(take(s).empty(), "no scan without a link");
  start(s, false);
  for (uint32_t id = 200; id < 200 + kMaxPeers; ++id) recv(s, Type::Reply, id, 100);
  recv(s, Type::Reply, 300, 200);
  runner.expectTrue(statusOf(s, 300) != Status::New, "a full list ignores a new device");
  recv(s, Type::Reply, kLow, 300);
  runner.expectTrue(statusOf(s, kLow) == Status::Paired && statusOf(s, 200) != Status::New,
                    "a paired device takes the place of a device that is not paired");
  start(s, false);
  for (uint32_t id = 200; id < 200 + kMaxPeers; ++id) recv(s, Type::Reply, id, 100);
  recv(s, Type::Request, kNew, 200);
  s.answer(true, 300);
  uint32_t ips[kMaxPeers];
  const int targetCount = s.targets(ips, kMaxPeers);
  runner.expectTrue(statusOf(s, kNew) == Status::Paired && targetCount == 1 && ips[0] == ipOf(kNew),
                    "a device paired from a full list is a target at once");
  start(s);
  s.green(true, true, 1000);
  s.leave(2000);
  acts = take(s);
  runner.expectTrue(sent(acts, Type::End) == 6 && count(acts, Do::Send, Type::Bye, kBroadcast) == 2, "leave");
}

int main() {
  TestUtils::TestRunner runner("Talk");
  uint8_t buf[kMaxPacket + 1];
  Packet out, in;

  out.type = Type::Hello;
  out.id = 0xA1B2C3D4;
  out.talk = 7;
  std::strcpy(out.name, "Papwick-1A2B");
  size_t len = encode(out, buf);
  runner.expectEq(kHeader + kNameLen, len, "hello length");
  runner.expectTrue(parse(buf, len, in) && in.type == Type::Hello && in.id == 0xA1B2C3D4 && in.talk == 7,
                    "hello round trip");
  runner.expectEqual("Papwick-1A2B", in.name, "name round trip");
  runner.expectFalse(parse(buf, len - 1, in), "short hello rejected");
  runner.expectFalse(parse(buf, len + 1, in), "long hello rejected");
  std::memset(buf + kHeader, 'x', kNameLen);
  runner.expectTrue(parse(buf, len, in) && std::strlen(in.name) == kNameLen - 1, "name without NUL is cut");
  std::memcpy(buf + kHeader, "a\x01\xC3\xA9z", 6);
  runner.expectTrue(parse(buf, len, in) && std::strncmp(in.name, "a???z", 5) == 0,
                    "characters that are not printable ASCII change to ?");

  out.type = Type::Audio;
  for (int i = 0; i < kFrame; ++i) out.frame[i] = static_cast<int16_t>(i * 97 - 16000);
  out.frame[0] = -32768;
  out.frame[1] = 32767;
  len = encode(out, buf);
  runner.expectEq(kMaxPacket, len, "audio length");
  runner.expectTrue(parse(buf, len, in) && in.type == Type::Audio, "audio parses");
  runner.expectTrue(std::memcmp(in.frame, out.frame, sizeof(out.frame)) == 0, "audio samples round trip");
  runner.expectTrue(buf[kHeader] == 0x00 && buf[kHeader + 1] == 0x80, "samples are little endian");
  runner.expectFalse(parse(buf, len - 2, in), "short audio rejected");

  out.type = Type::End;
  len = encode(out, buf);
  runner.expectTrue(len == kHeader && parse(buf, len, in) && in.type == Type::End, "end round trip");
  buf[0] = 'X';
  runner.expectFalse(parse(buf, len, in), "bad magic rejected");
  buf[0] = 'P';
  buf[2] = kVersion + 1;
  runner.expectFalse(parse(buf, len, in), "other version rejected");
  buf[2] = kVersion;
  for (uint8_t t : {0, 9, 255}) {
    buf[3] = t;
    runner.expectFalse(parse(buf, len, in), "unknown type rejected");
  }
  runner.expectFalse(parse(buf, kHeader - 1, in), "short header rejected");

  out.type = Type::Request;
  std::strcpy(out.name, "Papwick-ECF3");
  len = encode(out, buf);
  runner.expectTrue(len == kHeader + kNameLen && parse(buf, len, in) && in.type == Type::Request, "request round trip");
  runner.expectEqual("Papwick-ECF3", in.name, "request carries the name");
  out.type = Type::Accept;
  len = encode(out, buf);
  runner.expectTrue(parse(buf, len, in) && in.type == Type::Accept, "accept round trip");
  runner.expectFalse(parse(buf, kHeader, in), "accept without name rejected");
  out.type = Type::Refuse;
  len = encode(out, buf);
  runner.expectTrue(len == kHeader + kNameLen && parse(buf, len, in) && in.type == Type::Refuse, "refuse round trip");

  Paired paired;
  runner.expectTrue(paired.valid() && !paired.has(7), "empty pairing list");
  runner.expectTrue(paired.add(7, "seven") && paired.has(7), "add a paired device");
  runner.expectEqual("seven", paired.name(7), "a paired device keeps its name");
  runner.expectTrue(paired.name(8) == nullptr, "no name for a device that is not paired");
  runner.expectFalse(paired.add(7, "again") || paired.count != 1, "a device pairs once");
  runner.expectFalse(paired.rename(7, "seven"), "the same name is no change");
  runner.expectTrue(paired.rename(7, "Seven") && std::strcmp(paired.name(7), "Seven") == 0, "rename");
  runner.expectFalse(paired.rename(8, "x"), "rename of a device that is not paired");
  paired.add(8, "eight");
  runner.expectTrue(paired.remove(7) && !paired.has(7) && std::strcmp(paired.name(8), "eight") == 0,
                    "remove keeps the other names");
  runner.expectFalse(paired.remove(7), "remove of a device that is not paired");
  for (uint32_t id = 100; paired.count < kMaxPaired; ++id) paired.add(id, "x");
  runner.expectFalse(paired.add(1, "x") || paired.has(1), "a full pairing list ignores new devices");
  runner.expectTrue(paired.valid(), "a full pairing list is valid");
  std::memset(paired.names[3], 'x', kNameLen);
  runner.expectFalse(paired.valid(), "a name without NUL is invalid");
  paired.count = kMaxPaired + 1;
  runner.expectFalse(paired.valid(), "a count out of range is invalid");
  paired = {};
  paired.version = 1;
  runner.expectFalse(paired.valid(), "other version is invalid");

  Peers peers;
  runner.expectTrue(peers.update(1, 100, "one", 1000), "new peer is a change");
  runner.expectFalse(peers.update(1, 101, nullptr, 1100), "a packet without name is no change");
  runner.expectTrue(peers.find(1)->ip == 101 && peers.find(1)->seen == 1100, "the peer gets the new address");
  runner.expectEqual("one", peers.find(1)->name, "nullptr keeps the name");
  runner.expectTrue(peers.update(2, 200, nullptr, 1200) && peers.find(2)->name[0] == '\0', "unknown name is empty");
  runner.expectTrue(peers.update(2, 200, "two", 1300), "a new name is a change");
  for (uint32_t id = 3; id <= kMaxPeers; ++id) peers.update(id, id, "x", 1400);
  runner.expectFalse(peers.update(99, 99, "full", 1400) || peers.find(99), "a full list ignores new peers");
  runner.expectTrue(peers.remove(1) && !peers.find(1) && peers.count == kMaxPeers - 1, "remove");
  runner.expectTrue(peers.items[0].id == 2, "remove keeps the order");
  runner.expectFalse(peers.remove(1), "remove of an unknown peer");
  runner.expectTrue(peers.prune(1400) && peers.count == kMaxPeers - 2 && !peers.find(2), "prune old peers");
  runner.expectFalse(peers.prune(1400), "prune keeps peers seen at the time");
  peers = {};
  peers.update(5, 5, "late", 0xFFFFFFF0u);
  runner.expectFalse(peers.prune(0xFFFFFF00u), "prune works across the millis() wrap");
  runner.expectTrue(peers.prune(10), "prune after the millis() wrap");

  testJitter(runner);
  testVoice(runner);
  testSession(runner);
  return runner.exitCode();
}
