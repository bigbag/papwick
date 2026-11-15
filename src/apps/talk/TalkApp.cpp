#include <BoardSupport.h>
#include <I18n.h>
#include <Logging.h>
#include <TalkSession.h>
#include <WiFiUdp.h>
#include <esp_random.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>

#include "../../shell/Net.h"
#include "App.h"

namespace {

using talk::Session;

constexpr const char* TAG = "TALK";
constexpr uint32_t kUdpRetryMs = 1000;
constexpr int kPromptY = 370;
static_assert(talk::kRate == board::kVoiceRate && talk::kFrame == board::kVoiceFrame);
static_assert(talk::Jitter::kChunk * talk::kFrame == board::kVoiceChunk);

struct Cue {
  uint16_t hz, hz2, ms;
  uint8_t pulses;
};
// In the order of talk::Tone: End, Busy, Ok, Fail, Ask.
constexpr Cue kCues[] = {
    {1000, 700, 60, 0}, {480, 360, 120, 2}, {880, 1320, 80, 1}, {440, 330, 120, 2}, {990, 1320, 80, 1}};

// The radio stays on while the app is open. Thus an incoming talk also wakes the screen. Both devices must have the
// app open. talk::Session makes all decisions. This class moves packets, sound and input between it and the board.
class TalkApp : public App {
 public:
  const AppInfo& info() const override {
    static const AppInfo i = {"talk", tr(WALKIE_TALKIE), lgfx::color565(150, 205, 195), icon::radio};
    return i;
  }

  void boot() override {
    if (!prefs_.begin(info().id, false)) LOG_ERR(TAG, "NVS open failed");
    talk::Paired p;
    if (shell::load(prefs_, "paired", p) && p.valid()) paired_ = p;
    LOG_INF(TAG, "%u paired devices", paired_.count);
  }

  void clearData() override {
    App::clearData();
    paired_ = {};
  }

  void start() override {
    open_ = true;
    udpOn_ = fast_ = false;
    udpRetryAt_ = 0;
    out_.id = static_cast<uint32_t>(ESP.getEfuseMac() >> 16);
    snprintf(out_.name, sizeof(out_.name), "%s", shell::deviceName());
    session_.begin(out_.id, paired_, static_cast<uint8_t>(esp_random()));
    connect();
  }

  void stop() override {
    session_.leave(millis());
    act();
    session_.link(false, millis());
    act();
    board::voiceStop();
    // The radio needs time to send END and BYE before it stops.
    delay(50);
    udp_.stop();
    udpOn_ = false;
    board::wifi::setPowerSave(true);
    net::cancel();
    board::wifi::shutdown();
    open_ = false;
  }

  void update(const ui::Input& in) override {
    const uint32_t now = millis();
    polled_ = true;
    if (session_.prompt() != talk::Prompt::None) {
      if (in.green || in.blue) session_.answer(in.green, now);
    } else if (in.blue && session_.talking()) {
      session_.stopTalk(now);
    } else if (in.blue) {
      shell::home();
      return;
    } else if (!session_.online()) {
      if (in.green && !net::busy() && net::networks().count > 0) connect();
    } else {
      session_.green(in.green, in.greenDown, now);
      if (in.swipeDown) session_.rescan(now);
      const int row = rowAt(in.y);
      if (row >= 0 && in.tap) session_.tap(rows_[row].id, now);
      if (row >= 0 && in.hold) session_.hold(rows_[row].id, now);
    }
    act();
  }

  // The network and the sound run here: the shell skips update() while it shows the power question.
  void background() override {
    if (!open_) return;
    const uint32_t now = millis();
    if (!polled_) session_.stopTalk(now);
    polled_ = false;
    if (net::syncState() != shownSync_) {
      shownSync_ = net::syncState();
      dirty = true;
    }
    session_.link(linkUp(now), now);
    act();
    if (session_.online()) receive(now);
    if (session_.talking()) sendFrames();
    session_.tick(now, board::voiceIdle());
    act();
    play();
    if (session_.fast(now) != fast_) {
      // The modem sleep delays each packet up to the next beacon. A talk needs the radio on.
      fast_ = !fast_;
      board::wifi::setPowerSave(!fast_);
    }
    if (session_.takeDirty()) dirty = true;
  }

  void draw() override {
    if (session_.prompt() != talk::Prompt::None) {
      char text[48];
      const bool pair = session_.prompt() == talk::Prompt::Pair;
      snprintf(text, sizeof(text), pair ? tr(FMT_CONNECT_Q) : tr(FMT_FORGET_Q), session_.promptName());
      ui::confirmDraw(text, pair ? tr(YES) : tr(FORGET));
      return;
    }
    // A tap or a hold selects a row only on the device list.
    rowCount_ = 0;
    ui::clear();
    ui::text(shell::deviceName(), ui::cx(), 80, &fonts::DejaVu18, ui::kGrey);
    const char* green = nullptr;
    const char* prompt = nullptr;
    uint16_t promptColor = TFT_WHITE;
    if (net::networks().count == 0) {
      ui::text(tr(NO_WIFI_NETWORK), ui::cx(), ui::cy() - 20, &fonts::DejaVu24);
      ui::text(tr(ADD_IN_SETTINGS), ui::cx(), ui::cy() + 20, &fonts::DejaVu18, ui::kGrey);
    } else if (!session_.online()) {
      ui::text(net::busy() ? tr(CONNECTING) : tr(NO_CONNECTION), ui::cx(), ui::cy(), &fonts::DejaVu24);
      if (!net::busy()) green = tr(RETRY);
    } else if (session_.talking()) {
      ui::progressRing(1, ui::kGreen);
      if (session_.live()) {
        ui::text(tr(TALKING), ui::cx(), ui::cy(), &fonts::DejaVu40, ui::kGreen);
      } else {
        ui::text(tr(WAIT), ui::cx(), ui::cy(), &fonts::DejaVu40, ui::kGrey);
      }
      if (session_.latched()) {
        prompt = tr(PUSH_TO_STOP);
        promptColor = ui::kGreen;
        green = tr(STOP);
      }
    } else if (session_.receiving()) {
      ui::progressRing(1, ui::kAmber);
      ui::text(session_.talker(), ui::cx(), ui::cy() - 20, &fonts::DejaVu40, ui::kAmber);
      ui::text(tr(IS_TALKING), ui::cx(), ui::cy() + 30, &fonts::DejaVu18, ui::kAmber);
    } else {
      rowCount_ = session_.rows(rows_, ui::kMaxRows);
      if (rowCount_ == 0) {
        ui::text(tr(NO_OTHER_DEVICES), ui::cx(), ui::cy() - 20, &fonts::DejaVu24);
        ui::text(session_.scanning() ? tr(SEARCHING) : tr(SWIPE_TO_SEARCH), ui::cx(), ui::cy() + 20, &fonts::DejaVu18,
                 ui::kGrey);
      }
      bool online = false, waiting = false;
      // shortcut: shows the first kMaxRows devices only, add pages when homes have more.
      for (int i = 0; i < rowCount_; ++i) {
        const talk::Row& r = rows_[i];
        online |= r.status == talk::Status::Paired;
        waiting |= r.status == talk::Status::Waiting;
        drawRow(i, r);
      }
      if (online) {
        prompt = tr(PUSH_TO_TALK);
        promptColor = ui::kGreen;
        green = tr(TALK);
      } else if (rowCount_ > 0) {
        prompt = waiting ? tr(CONFIRM_ON_OTHER) : tr(TAP_TO_CONNECT);
      }
    }
    if (const char* n = noticeText()) {
      prompt = n;
      promptColor = session_.notice() == talk::Notice::Connected ? ui::kGreen : ui::kAmber;
    }
    if (prompt) ui::text(prompt, ui::cx(), kPromptY, &fonts::DejaVu18, promptColor);
    ui::hints(ui::kHintsY, tr(BACK), green);
  }

  bool keepAwake() const override {
    return session_.talking() || session_.receiving() || session_.prompt() != talk::Prompt::None;
  }

 private:
  int rowY(int i) const { return ui::cy() - (rowCount_ - 1) * 20 + i * 40; }

  int rowAt(int y) const {
    for (int i = 0; i < rowCount_; ++i) {
      if (std::abs(y - rowY(i)) < 20) return i;
    }
    return -1;
  }

  void drawRow(int i, const talk::Row& r) {
    char text[40];
    const char* status = nullptr;
    uint16_t color = TFT_WHITE;
    if (r.status == talk::Status::New) {
      status = tr(STATUS_NEW);
      color = ui::kGrey;
    } else if (r.status == talk::Status::Waiting) {
      status = tr(STATUS_WAITING);
      color = ui::kAmber;
    } else if (r.status == talk::Status::Offline) {
      status = tr(STATUS_OFFLINE);
      color = ui::kGrey;
    }
    if (status) {
      snprintf(text, sizeof(text), tr(FMT_ROW), r.name, status);
    } else {
      snprintf(text, sizeof(text), "%s", r.name);
    }
    ui::text(text, ui::cx(), rowY(i), &fonts::DejaVu24, color);
  }

  const char* noticeText() const {
    switch (session_.notice()) {
      case talk::Notice::Busy:
        return tr(BUSY);
      case talk::Notice::Refused:
        return tr(REFUSED);
      case talk::Notice::NoAnswer:
        return tr(NO_ANSWER);
      case talk::Notice::Connected:
        return tr(CONNECTED);
      case talk::Notice::Forgotten:
        return tr(FORGOTTEN);
      case talk::Notice::Nobody:
        return tr(NOBODY_ONLINE);
      case talk::Notice::Full:
        return tr(LIST_FULL);
      case talk::Notice::NoMic:
        return tr(MIC_ERROR);
      default:
        return nullptr;
    }
  }

  void connect() {
    // The boot sync can still run. It turns the radio off at its end.
    net::cancel();
    net::startSync(true);
    dirty = true;
  }

  bool linkUp(uint32_t now) {
    if (!board::wifi::connected()) {
      if (udpOn_) udp_.stop();
      udpOn_ = false;
      return false;
    }
    if (!udpOn_ && talk::due(udpRetryAt_, now)) {
      udpOn_ = udp_.begin(talk::kPort);
      if (udpOn_) {
        LOG_INF(TAG, "Online as %s, id %08lX", out_.name, static_cast<unsigned long>(out_.id));
      } else {
        LOG_ERR(TAG, "UDP start failed");
        udpRetryAt_ = now + kUdpRetryMs;
      }
    }
    return udpOn_;
  }

  void act() {
    talk::Action a[16];
    for (int n; (n = session_.take(a, 16)) > 0;) {
      for (int i = 0; i < n; ++i) run(a[i]);
    }
  }

  void run(const talk::Action& a) {
    switch (a.what) {
      case talk::Do::Send:
        send(a.type, a.ip);
        break;
      case talk::Do::MicOn:
        voice_.reset();
        if (!board::voiceCaptureBegin()) session_.micFailed(millis());
        break;
      case talk::Do::MicOff:
        sendFrames();
        board::voiceCaptureEnd();
        break;
      case talk::Do::Wake:
        board::wake();
        break;
      case talk::Do::Save:
        paired_ = session_.paired();
        shell::save(prefs_, "paired", paired_);
        break;
      case talk::Do::Tone: {
        const Cue& c = kCues[static_cast<int>(a.tone)];
        board::voiceTone(c.hz, c.hz2, c.ms);
        if (c.pulses) board::vibrate(60, 60, c.pulses);
        break;
      }
    }
  }

  void receive(uint32_t now) {
    // A limit for each loop, so a flood of packets does not stop the sound and the screen.
    for (int i = 0, size; i < 16 && (size = udp_.parsePacket()) > 0; ++i) {
      const int n = udp_.read(buf_, sizeof(buf_));
      // A longer packet leaves bytes in the buffer, and parsePacket() returns 0 until they go.
      if (n != size) udp_.clear();
      if (n != size || !talk::parse(buf_, n, in_)) continue;
      session_.packet(in_, static_cast<uint32_t>(udp_.remoteIP()), now);
      act();
    }
  }

  void sendFrames() {
    uint32_t ips[talk::kMaxPeers];
    const int n = session_.targets(ips, talk::kMaxPeers);
    while (board::voiceRead(out_.frame)) {
      // The zero frames of the codec start go out too. A receiver wakes its screen during them, not during the voice.
      if (voice_.process(out_.frame)) session_.micFrame(millis());
      for (int i = 0; i < n; ++i) send(talk::Type::Audio, ips[i]);
    }
  }

  void play() {
    for (int i = 0; i < 2 && board::voiceCanPlay() && session_.jitter.pop(chunk_, board::voiceIdle()); ++i) {
      if (board::voicePlay(chunk_)) continue;
      // Retry after the next prebuffer, not in each loop.
      session_.jitter.reset();
      break;
    }
  }

  void send(talk::Type type, uint32_t ip) {
    out_.type = type;
    out_.talk = session_.talkId();
    const size_t len = talk::encode(out_, buf_);
    if (!udp_.beginPacket(IPAddress(ip), talk::kPort) || udp_.write(buf_, len) != len || !udp_.endPacket())
      LOG_DBG(TAG, "Send to %08lX failed", static_cast<unsigned long>(ip));
  }

  Preferences prefs_;
  WiFiUDP udp_;
  talk::Paired paired_;
  Session session_;
  talk::Voice voice_;
  talk::Packet in_, out_;
  talk::Row rows_[ui::kMaxRows] = {};
  int rowCount_ = 0;
  uint8_t buf_[talk::kMaxPacket] = {};
  int16_t chunk_[board::kVoiceChunk] = {};
  uint32_t udpRetryAt_ = 0;
  net::Sync shownSync_ = net::Sync::Idle;
  bool open_ = false, polled_ = false, udpOn_ = false, fast_ = false;
};

}  // namespace

App& talkApp() {
  static TalkApp a;
  return a;
}
