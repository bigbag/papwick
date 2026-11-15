#pragma once

#include "Talk.h"

// The walkie-talkie rules without hardware: discovery, pairing, talk and receive. The app gives it the inputs
// (link, packets, buttons, taps, time) and does the actions it asks for (send, microphone, tones, save).
namespace talk {

constexpr uint32_t kBroadcast = 0xFFFFFFFF;

enum class Do : uint8_t { Send, MicOn, MicOff, Wake, Save, Tone };
// Ask: a pairing request came. Ok: paired. Fail: refused, no answer or nobody to talk to.
enum class Tone : uint8_t { End, Busy, Ok, Fail, Ask };
enum class Notice : uint8_t { None, Busy, Refused, NoAnswer, Connected, Forgotten, Nobody, Full, NoMic };
enum class Prompt : uint8_t { None, Pair, Forget };
enum class Status : uint8_t { Paired, New, Waiting, Offline };

struct Action {
  Do what;
  Type type;
  uint32_t ip;
  Tone tone;
};

struct Row {
  uint32_t id;
  const char* name;
  Status status;
};

class Session {
 public:
  // A shorter push keeps the talk on until the next push. A longer hold ends the talk at the release.
  static constexpr uint32_t kTapMs = 400;
  static constexpr uint32_t kMaxTalkMs = 60000;
  static constexpr uint32_t kRxTimeoutMs = 500;
  // AUDIO of an ended talk can arrive late. It must not start a new talk.
  static constexpr uint32_t kLateMs = 1000;
  // A scan sends HELLO at 0 s and 1 s. After 3 s it removes the devices that did not answer.
  static constexpr uint32_t kHelloAgainMs = 1000, kScanMs = 3000, kRescanMs = 30000;
  // The prompt closes before the request of the other device ends.
  static constexpr uint32_t kRequestMs = 30000, kPromptMs = 25000;
  static constexpr uint32_t kNoticeMs = 2500;
  // The radio stays fast for a reply after a talk.
  static constexpr uint32_t kFastMs = 3000;
  // A talk stops when the microphone gives no sound for this time.
  static constexpr uint32_t kMicTimeoutMs = 2000;
  static constexpr int kMaxActions = 64;

  void begin(uint32_t self, const Paired& paired, uint8_t talkId);
  void link(bool up, uint32_t now);
  void packet(const Packet& p, uint32_t ip, uint32_t now);
  // press: the green button went down in this loop. down: the button is down now.
  void green(bool press, bool down, uint32_t now);
  // A frame came from the microphone. The first one makes the talk live.
  void micFrame(uint32_t now);
  void micFailed(uint32_t now);
  // Ends a talk without the end tone, for example when the shell shows the power question.
  void stopTalk(uint32_t now);
  void tap(uint32_t id, uint32_t now);
  void hold(uint32_t id, uint32_t now);
  void answer(bool yes, uint32_t now);
  void rescan(uint32_t now);
  // speakerIdle: the speaker played all the voice. The end tone waits for it.
  void tick(uint32_t now, bool speakerIdle);
  // Ends a talk and says BYE.
  void leave(uint32_t now);

  // Moves the actions to out in order. Returns their count.
  int take(Action* out, int max);
  // Returns true once after each change of the screen.
  bool takeDirty();

  bool online() const { return online_; }
  bool talking() const { return tx_; }
  bool latched() const { return latched_; }
  bool live() const { return live_; }
  bool receiving() const { return rx_; }
  bool scanning() const { return scanning_; }
  // The name of the talker, or "".
  const char* talker() const;
  Prompt prompt() const { return prompt_; }
  const char* promptName() const { return promptName_; }
  Notice notice() const { return notice_; }
  // True while a talk runs and shortly after it. The app then turns the Wi-Fi power save off.
  bool fast(uint32_t now) const { return tx_ || rx_ || !due(fastUntil_, now); }
  uint8_t talkId() const { return talkId_; }
  const Paired& paired() const { return paired_; }
  // The devices for the screen: the devices online first, then the paired devices offline.
  int rows(Row* out, int max) const;
  // The addresses of the paired devices online.
  int targets(uint32_t* ips, int max) const;

  Jitter jitter;

 private:
  void push(Do what, Type type = Type::Hello, uint32_t ip = 0, Tone tone = Tone::End);
  void send(Type type, uint32_t ip) { push(Do::Send, type, ip); }
  void sendPaired(Type type);
  void tone(Tone t) { push(Do::Tone, Type::Hello, 0, t); }
  void notify(Notice n, uint32_t now);
  void scan(uint32_t now);
  bool pair(uint32_t id, const char* name, uint32_t now);
  void request(const Packet& p, uint32_t ip, uint32_t now);
  void refuse(uint32_t id, uint32_t now);
  void addPeer(uint32_t id, uint32_t ip, const char* name, uint32_t now);
  void audio(const Packet& p, uint32_t now);
  void endTx(bool sendEnd, bool endTone, uint32_t now);
  void endRx(uint32_t now);

  uint32_t self_ = 0;
  Paired paired_;
  Peers peers_;
  Action out_[kMaxActions] = {};
  int outCount_ = 0;
  bool online_ = false, tx_ = false, latched_ = false, live_ = false, rx_ = false, scanning_ = false;
  bool requesting_ = false, endTone_ = false, dirty_ = true;
  uint8_t talkId_ = 0, rxTalk_ = 0, endedTalk_ = 0, hellos_ = 0;
  uint32_t txStart_ = 0, micAt_ = 0, rxFrom_ = 0, rxLast_ = 0, endedFrom_ = 0, endedAt_ = 0;
  uint32_t scanAt_ = 0, nextScan_ = 0, requestId_ = 0, requestAt_ = 0, fastUntil_ = 0;
  Prompt prompt_ = Prompt::None;
  uint32_t promptId_ = 0, promptIp_ = 0, promptAt_ = 0;
  char promptName_[kNameLen] = {};
  Notice notice_ = Notice::None;
  uint32_t noticeAt_ = 0;
};

}  // namespace talk
