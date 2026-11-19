#include <BoardSupport.h>
#include <I18n.h>
#include <Logging.h>
#include <Noise.h>

#include <algorithm>
#include <cmath>
#include <cstdio>

#include "App.h"

namespace {

constexpr const char* TAG = "NOISE";
constexpr float kPi = 3.14159265f;
constexpr uint32_t kFrameMs = 40;
// The ring is a gauge of kSegments from the lower left over the top to the lower right. 0 degrees is 3 o'clock.
constexpr int kSegments = 54;
constexpr float kStartDeg = 135, kSegmentDeg = 5, kGapDeg = 1.2f;
constexpr int kRingInner = 196, kRingOuter = 226, kLabelR = 170;
constexpr uint16_t kUnlit = 0x18E3;
// The codec filter and the zero level of M5Unified settle after the start of the capture.
constexpr uint32_t kSettleMs = 300;
// Without a sound frame for this time, the microphone has an error. Zero frames are no sound.
constexpr uint32_t kStallMs = 2000;
// The peak mark stays for kPeakHoldMs. Then it falls one segment in each kFrameMs.
constexpr uint32_t kPeakHoldMs = 1200;
// The overload mark stays for this time after a sample at full scale.
constexpr uint32_t kOverMs = 1000;
constexpr uint32_t kTextMs = 250;
constexpr int kTextW = 300, kTextH = 150;
// The calibration shows the average level in the place of the picker unit.
constexpr int kCalW = 260, kCalH = 30, kCalY = 92;
// The correction is in steps of 0.5 dB.
constexpr int kMaxCorrection = 12;
constexpr int kNorms[] = {30, 40, 55, 70, 85};
constexpr int kNormCount = sizeof(kNorms) / sizeof(*kNorms);

// The ring color at each step above the normal level, in dB. Between two steps the colors blend.
struct Step {
  float excess;
  uint8_t r, g, b;
};
constexpr Step kSteps[] = {{-10, 0, 150, 230}, {0, 40, 220, 90}, {10, 255, 210, 0}, {20, 255, 40, 30}};
constexpr int kStepCount = sizeof(kSteps) / sizeof(*kSteps);

struct NoiseSettings {
  uint8_t version = 1;
  uint8_t norm = 2;
  // Steps of 0.5 dB that the app adds to each level.
  int8_t correction = 0;
};

enum class Screen : uint8_t { Meter, Calibrate };

float angle(float db) {
  return kStartDeg + (db - noise::kMinDb) / (noise::kMaxDb - noise::kMinDb) * kSegments * kSegmentDeg;
}

// The codec gives zero frames for up to about 1 s after power-up. A real microphone frame is never all zero.
bool silent(const int16_t* frame) {
  return std::all_of(frame, frame + board::kVoiceFrame, [](int16_t s) { return s == 0; });
}

void segment(lgfx::LGFXBase& g, int i, uint16_t color) {
  const float a = std::fmod(kStartDeg + i * kSegmentDeg, 360.0f);
  g.fillArc(ui::cx(), ui::cy(), kRingInner, kRingOuter, a + kGapDeg / 2, a + kSegmentDeg - kGapDeg / 2, color);
}

// The microphone runs while the app is open and the screen is on. The ring shows the level, and its color shows how
// far the level is above the normal level.
class NoiseApp : public App {
 public:
  const AppInfo& info() const override {
    static const AppInfo i = {"noise", tr(NOISE_METER), lgfx::color565(40, 220, 90), icon::meter};
    return i;
  }

  void boot() override {
    if (!prefs_.begin(info().id, false)) LOG_ERR(TAG, "NVS open failed");
    NoiseSettings s;
    if (shell::load(prefs_, "settings", s) && s.version == NoiseSettings{}.version && s.norm < kNormCount &&
        std::abs(s.correction) <= kMaxCorrection) {
      settings_ = s;
    }
  }

  void start() override {
    open_ = true;
    meter_ = {};
    live_ = failed_ = false;
    peakTop_ = 0;
    overMs_ = 0;
    if (!text_.createSprite(kTextW, kTextH) || !cal_.createSprite(kCalW, kCalH)) {
      LOG_ERR(TAG, "Text sprite alloc failed");
      text_.deleteSprite();
      cal_.deleteSprite();
    }
  }

  void stop() override {
    open_ = false;
    // Leaving the calibration without green keeps the saved correction.
    if (screen_ == Screen::Calibrate) settings_.correction = before_;
    screen_ = Screen::Meter;
    end();
    text_.deleteSprite();
    cal_.deleteSprite();
  }

  void update(const ui::Input& in) override {
    if (screen_ == Screen::Calibrate) {
      // The hold that opens the calibration sends repeats until the finger comes up. They do not turn the wheel.
      if (in.hold) held_ = true;
      if (in.repeat && !held_) return;
      if (in.blue) {
        settings_.correction = before_;
        screen_ = Screen::Meter;
        dirty = true;
      } else if (in.green) {
        LOG_INF(TAG, "Correction %+.1f dB", correction());
        screen_ = Screen::Meter;
        settingsChanged();
      } else if (ui::pickerUpdate(picker_, in)) {
        settings_.correction = static_cast<int8_t>(std::lround(picker_.value * 2));
        dirty = true;
      }
      return;
    }
    if (in.blue) {
      shell::home();
    } else if (in.green && failed_) {
      failed_ = false;
      dirty = true;
    } else if (in.green) {
      meter_.reset();
      peakTop_ = 0;
      textMs_ = 0;
    } else if (in.hold && cal_.getBuffer()) {
      calibrate();
    } else if (in.tap) {
      settings_.norm = (settings_.norm + 1) % kNormCount;
      settingsChanged();
    }
  }

  void draw() override {
    ui::clear();
    textMs_ = millis();
    if (screen_ == Screen::Calibrate) {
      ui::pickerDraw(picker_, tr(DONE));
      drawCalibration(ui::canvas());
      return;
    }
    auto& c = ui::canvas();
    for (int i = 0; i < kSegments; ++i) {
      segment(c, i, kUnlit);
      shown_[i] = kUnlit;
    }
    const int norm = kNorms[settings_.norm];
    for (int db = static_cast<int>(noise::kMinDb); db <= noise::kMaxDb; db += 20) {
      if (std::abs(db - norm) >= 10) label(db, ui::kGrey);
    }
    label(norm, TFT_WHITE);
    const float a = angle(norm) * kPi / 180;
    const float x = ui::cx() + std::cos(a) * (kRingInner - 4), y = ui::cy() + std::sin(a) * (kRingInner - 4);
    c.fillCircle(x, y, 4, TFT_WHITE);
    ui::text(tr(NOISE_TAP), ui::cx(), 356, &fonts::DejaVu18, ui::kGrey);
    ui::text(tr(NOISE_HOLD), ui::cx(), 380, &fonts::DejaVu18, ui::kGrey);
    ui::hints(ui::kHintsY, tr(BACK), failed_ ? tr(RETRY) : tr(RESET));
    if (!text_.getBuffer()) ui::text(tr(OUT_OF_MEMORY), ui::cx(), ui::cy(), &fonts::DejaVu24, ui::kRed);
    drawText(c);
  }

  void animate() override {
    const uint32_t now = millis(), dt = now - lastMs_;
    lastMs_ = now;
    if (screen_ == Screen::Calibrate) {
      if (now - textMs_ >= kTextMs) {
        textMs_ = now;
        drawCalibration(M5.Display);
      }
      return;
    }
    notePeak(now);
    const int on = lit(), peak = peakAt(now);
    const uint16_t color = ringColor(std::round(level() - kNorms[settings_.norm]), dt);
    for (int i = 0; i < kSegments; ++i) {
      const uint16_t want = i < on ? color : (i == peak - 1 ? TFT_WHITE : kUnlit);
      if (want == shown_[i]) continue;
      // The panel frame buffer sends the bounding box of each write. One write for each segment keeps it small.
      M5.Display.startWrite();
      segment(M5.Display, i, want);
      M5.Display.endWrite();
      shown_[i] = want;
    }
    if (now - textMs_ >= kTextMs) {
      textMs_ = now;
      drawText(M5.Display);
    }
  }

  void background() override {
    if (!open_) return;
    if (board::light() == board::Light::Off) {
      end();
      return;
    }
    if (!capturing_) {
      if (!failed_) begin();
      return;
    }
    const uint32_t now = millis();
    int16_t frame[board::kVoiceFrame];
    while (board::voiceRead(frame)) {
      if (silent(frame)) continue;
      lastFrameMs_ = now;
      if (now - startMs_ < kSettleMs) continue;
      if (meter_.add(frame, board::kVoiceFrame)) overMs_ = now;
      live_ = true;
    }
    if (now - lastFrameMs_ > kStallMs) {
      LOG_ERR(TAG, "No microphone frames for %lu ms", static_cast<unsigned long>(now - lastFrameMs_));
      fail();
      return;
    }
    notePeak(now);
  }

  bool keepAwake() const override { return screen_ == Screen::Calibrate; }

  uint32_t frameMs() const override { return kFrameMs; }

  ui::Row* settings(int& count) override {
    static char labels[kNormCount][12];
    static const char* options[kNormCount];
    for (int i = 0; i < kNormCount; ++i) {
      snprintf(labels[i], sizeof(labels[i]), tr(FMT_DB), kNorms[i]);
      options[i] = labels[i];
    }
    static ui::Row rows[] = {{tr(NORMAL_LEVEL), options, kNormCount, &settings_.norm}};
    count = 1;
    return rows;
  }

  void settingsChanged() override {
    shell::save(prefs_, "settings", settings_);
    dirty = true;
  }

  void clearData() override {
    App::clearData();
    settings_ = NoiseSettings{};
    dirty = true;
  }

 private:
  static void label(int db, uint16_t color) {
    char s[8];
    snprintf(s, sizeof(s), "%d", db);
    const float a = angle(db) * kPi / 180;
    ui::text(s, ui::cx() + std::cos(a) * kLabelR, ui::cy() + std::sin(a) * kLabelR, &fonts::DejaVu12, color);
  }

  // Each new capture starts with no level, also after the screen is off. The maximum and the average stay.
  void begin() {
    capturing_ = board::voiceCaptureBegin(board::kUnityGain);
    startMs_ = lastFrameMs_ = millis();
    live_ = false;
    meter_.clearLevel();
    peakTop_ = 0;
    if (capturing_) {
      LOG_INF(TAG, "Measure, normal level %d dB", kNorms[settings_.norm]);
      return;
    }
    LOG_ERR(TAG, "Microphone start failed");
    fail();
  }

  // Green on the meter screen tries again. The calibration closes and keeps the saved correction.
  void fail() {
    end();
    failed_ = true;
    live_ = false;
    peakTop_ = 0;
    if (screen_ == Screen::Calibrate) settings_.correction = before_;
    screen_ = Screen::Meter;
    dirty = true;
  }

  void end() {
    if (!capturing_) return;
    board::voiceCaptureEnd();
    capturing_ = false;
    if (!meter_.empty()) {
      LOG_INF(TAG, "Max %.1f dB, average %.1f dB, correction %+.1f dB", meter_.max() + correction(),
              meter_.average() + correction(), correction());
    }
  }

  float correction() const { return settings_.correction * 0.5f; }

  float level() const { return live_ ? meter_.level() + correction() : 0; }

  int lit() const {
    const float k = (level() - noise::kMinDb) / (noise::kMaxDb - noise::kMinDb);
    return std::clamp<int>(std::lround(k * kSegments), 0, kSegments);
  }

  int peakAt(uint32_t now) const {
    const uint32_t age = now - peakMs_;
    if (age <= kPeakHoldMs) return peakTop_;
    return std::max(0, peakTop_ - static_cast<int>((age - kPeakHoldMs) / kFrameMs));
  }

  // The measurement also sets the peak while the power question hides the ring.
  void notePeak(uint32_t now) {
    if (lit() < peakAt(now)) return;
    peakTop_ = lit();
    peakMs_ = now;
  }

  bool overloaded() const { return overMs_ && millis() - overMs_ < kOverMs; }

  // The average starts again, so it shows the level of a steady reference sound.
  void calibrate() {
    before_ = settings_.correction;
    picker_ = {tr(CORRECTION), nullptr, correction(), -kMaxCorrection * 0.5f, kMaxCorrection * 0.5f, 0.5f, 1};
    held_ = false;
    meter_.reset();
    screen_ = Screen::Calibrate;
    dirty = true;
  }

  void drawCalibration(LovyanGFX& dst) {
    if (!cal_.getBuffer()) return;
    cal_.fillScreen(TFT_BLACK);
    char s[32];
    snprintf(s, sizeof(s), tr(FMT_LEQ), meter_.average() + correction());
    cal_.setFont(&fonts::DejaVu18);
    cal_.setTextColor(overloaded() ? ui::kRed : ui::kGreen);
    cal_.setTextDatum(middle_center);
    cal_.drawString(overloaded() ? tr(OVERLOAD) : (meter_.empty() ? "--" : s), kCalW / 2, kCalH / 2);
    cal_.pushSprite(&dst, ui::cx() - kCalW / 2, kCalY - kCalH / 2);
  }

  // Blends the colors of the two steps around excess. Over the last step the ring pulses, faster with more excess.
  uint16_t ringColor(float excess, uint32_t dtMs) {
    int i = 1;
    while (i < kStepCount - 1 && excess > kSteps[i].excess) ++i;
    const Step &a = kSteps[i - 1], &b = kSteps[i];
    const float t = std::clamp((excess - a.excess) / (b.excess - a.excess), 0.0f, 1.0f);
    float k = 1;
    const float over = excess - kSteps[kStepCount - 1].excess;
    if (over > 0) {
      pulse_ = std::fmod(pulse_ + std::min(1 + over / 5, 4.0f) * dtMs / 1000, 1.0f);
      k = 0.6f + 0.4f * std::cos(2 * kPi * pulse_);
    }
    auto mix = [&](uint8_t from, uint8_t to) { return static_cast<uint8_t>((from + (to - from) * t) * k); };
    return lgfx::color565(mix(a.r, b.r), mix(a.g, b.g), mix(a.b, b.b));
  }

  void drawText(LovyanGFX& dst) {
    if (!text_.getBuffer()) return;
    text_.fillScreen(TFT_BLACK);
    text_.setTextDatum(middle_center);
    if (failed_) {
      text_.setFont(&fonts::DejaVu24);
      text_.setTextColor(ui::kRed);
      text_.drawString(tr(MIC_ERROR), kTextW / 2, kTextH / 2);
    } else {
      char s[32];
      snprintf(s, sizeof(s), "%ld", std::lround(std::max(level(), 0.0f)));
      text_.setFont(&fonts::DejaVu72);
      text_.setTextColor(TFT_WHITE);
      text_.drawString(live_ ? s : "--", kTextW / 2, 40);
      text_.setFont(&fonts::DejaVu18);
      text_.setTextColor(overloaded() ? ui::kRed : ui::kGrey);
      text_.drawString(overloaded() ? tr(OVERLOAD) : tr(DBA), kTextW / 2, 92);
      text_.setTextColor(ui::kGrey);
      if (!meter_.empty()) {
        snprintf(s, sizeof(s), tr(FMT_MAX_AVG), static_cast<int>(std::lround(meter_.max() + correction())),
                 static_cast<int>(std::lround(meter_.average() + correction())));
        text_.drawString(s, kTextW / 2, 128);
      }
    }
    text_.pushSprite(&dst, ui::cx() - kTextW / 2, ui::cy() - kTextH / 2);
  }

  Preferences prefs_;
  NoiseSettings settings_;
  noise::Meter meter_;
  M5Canvas text_{&M5.Display}, cal_{&M5.Display};
  ui::Picker picker_{};
  Screen screen_ = Screen::Meter;
  int8_t before_ = 0;
  uint16_t shown_[kSegments] = {};
  int peakTop_ = 0;
  float pulse_ = 0;
  bool open_ = false, capturing_ = false, live_ = false, failed_ = false, held_ = false;
  uint32_t startMs_ = 0, lastFrameMs_ = 0, lastMs_ = 0, peakMs_ = 0, overMs_ = 0, textMs_ = 0;
};

}  // namespace

App& noiseApp() {
  static NoiseApp a;
  return a;
}
