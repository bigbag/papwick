#include <BoardSupport.h>
#include <I18n.h>
#include <Logging.h>
#include <Stopwatch.h>

#include <algorithm>
#include <cstdio>
#include <cstring>

#include "App.h"

namespace {

constexpr const char* TAG = "STOPWATCH";
constexpr uint32_t kFrameMs = 40, kIdleFrameMs = 1000;
constexpr int kDigitsW = 340, kDigitsH = 84, kDigitsY = 205;
constexpr int kLapTitleY = 140, kLapY = 280, kLapStep = 32, kLapRows = 3;
// The ring starts at the top. 0 degrees is 3 o'clock.
constexpr float kTopDeg = 270;
// One pixel on the ring is about 0.26 degrees. A new arc starts this far back, so no gap stays between two arcs.
constexpr float kMinStepDeg = 0.26f, kOverlapDeg = 0.5f;
constexpr uint16_t kLight = lgfx::color565(255, 214, 150);

// What the edge ring shows. A new key needs a full frame. With the same key, only the fill grows.
struct Ring {
  uint32_t key;
  uint16_t back, fill;
  float deg;
};

// The watch uses millis(), so it runs on when the user leaves the app. A power off clears it.
class StopwatchApp : public App {
 public:
  const AppInfo& info() const override {
    static const AppInfo i = {"stopwatch", tr(STOPWATCH), ui::kAmber, icon::stopwatch};
    return i;
  }

  void start() override {
    if (!digits_.createSprite(kDigitsW, kDigitsH)) LOG_ERR(TAG, "Digits sprite alloc failed");
  }

  void stop() override { digits_.deleteSprite(); }

  bool keepAwake() const override { return watch_.running; }

  void background() override {
    const uint32_t now = millis();
    if (!watch_.running || stopwatch::elapsed(watch_, now) < stopwatch::kMaxMs) return;
    stopwatch::stop(watch_, now);
    LOG_INF(TAG, "Stop at the time limit");
    dirty = true;
  }

  void clearData() override {
    App::clearData();
    stopwatch::reset(watch_);
  }

  uint32_t frameMs() const override { return watch_.running ? kFrameMs : kIdleFrameMs; }

  void update(const ui::Input& in) override {
    const uint32_t now = millis();
    if (in.blue) {
      shell::home();
    } else if (in.green && stopwatch::elapsed(watch_, now) < stopwatch::kMaxMs) {
      if (watch_.running) {
        stopwatch::stop(watch_, now);
      } else {
        stopwatch::start(watch_, now);
      }
      LOG_INF(TAG, "%s at %lu ms", watch_.running ? "Start" : "Stop",
              static_cast<unsigned long>(stopwatch::elapsed(watch_, now)));
      dirty = true;
    } else if (in.tap && stopwatch::lap(watch_, now)) {
      board::vibrate(20);
      dirty = true;
    } else if (in.hold && !watch_.running && stopwatch::elapsed(watch_, now) > 0) {
      stopwatch::reset(watch_);
      dirty = true;
    }
  }

  void draw() override {
    const uint32_t now = millis();
    ui::clear();
    ring_ = ring(now);
    ui::edgeArc(0, 360, ring_.back);
    ui::edgeArc(kTopDeg, kTopDeg + ring_.deg, ring_.fill);
    char line[40], time[16];
    const uint32_t t = stopwatch::elapsed(watch_, now);
    const bool started = watch_.running || t > 0, full = watch_.lapCount == stopwatch::kMaxLaps;
    if (started && !full) {
      snprintf(line, sizeof(line), tr(FMT_LAP), watch_.lapCount + 1);
      ui::text(line, ui::cx(), kLapTitleY, &fonts::DejaVu18, ui::kGrey);
    }
    const int good = stopwatch::best(watch_), bad = stopwatch::worst(watch_);
    for (int row = 0; row < kLapRows && row < watch_.lapCount; ++row) {
      const int i = watch_.lapCount - 1 - row;
      stopwatch::format(watch_.laps[i], time, sizeof(time));
      snprintf(line, sizeof(line), tr(FMT_LAP_TIME), i + 1, time);
      ui::text(line, ui::cx(), kLapY + row * kLapStep, &fonts::DejaVu18,
               i == good ? ui::kGreen : (i == bad ? ui::kRed : TFT_WHITE));
    }
    if (watch_.running) {
      ui::text(full ? tr(NO_MORE_LAPS) : tr(TAP_FOR_LAP), ui::cx(), 388, &fonts::DejaVu18, ui::kGrey);
    } else if (started) {
      ui::text(tr(HOLD_TO_RESET), ui::cx(), 388, &fonts::DejaVu18, ui::kGrey);
    }
    const char* green = t >= stopwatch::kMaxMs ? nullptr : (started ? tr(RESUME) : tr(START));
    ui::hints(ui::kHintsY, tr(BACK), watch_.running ? tr(STOP) : green);
    if (!digits_.getBuffer()) ui::text(tr(OUT_OF_MEMORY), ui::cx(), kDigitsY, &fonts::DejaVu24, ui::kRed);
    shown_[0] = '\0';
    drawDigits(ui::canvas(), now);
  }

  void animate() override {
    if (!watch_.running) return;
    const uint32_t now = millis();
    drawDigits(M5.Display, now);
    const Ring r = ring(now);
    if (r.key != ring_.key) {
      dirty = true;
      return;
    }
    if (r.deg - ring_.deg < kMinStepDeg) return;
    M5.Display.startWrite();
    ui::edgeArc(M5.Display, kTopDeg + std::max(0.0f, ring_.deg - kOverlapDeg), kTopDeg + r.deg, r.fill);
    M5.Display.endWrite();
    ring_.deg = r.deg;
  }

 private:
  // Without laps, the ring fills once in each minute, and its color changes in each minute. With laps, the ring
  // fills in the time of the last lap. A longer lap turns the ring red.
  Ring ring(uint32_t now) const {
    const uint32_t t = stopwatch::elapsed(watch_, now);
    if (watch_.lapCount == 0) {
      const uint32_t minute = t / 60000;
      auto color = [](uint32_t m) { return m % 2 ? kLight : ui::kAmber; };
      return {minute, minute ? color(minute - 1) : ui::kDark, color(minute), t % 60000 * 360.0f / 60000};
    }
    const uint32_t last = watch_.laps[watch_.lapCount - 1], lap = t - watch_.lapStartMs;
    const uint32_t key = 0x80000000u | static_cast<uint32_t>(watch_.lapCount) << 1;
    if (lap > last || last == 0) return {key | 1, ui::kRed, ui::kRed, 0};
    return {key, ui::kDark, ui::kGreen, lap * 360.0f / last};
  }

  void drawDigits(LovyanGFX& dst, uint32_t now) {
    char s[sizeof(shown_)];
    stopwatch::format(stopwatch::elapsed(watch_, now), s, sizeof(s));
    if (!digits_.getBuffer() || strcmp(s, shown_) == 0) return;
    strcpy(shown_, s);
    digits_.fillScreen(TFT_BLACK);
    digits_.setFont(&fonts::DejaVu72);
    digits_.setTextDatum(middle_center);
    digits_.setTextColor(TFT_WHITE);
    digits_.drawString(s, kDigitsW / 2, kDigitsH / 2);
    digits_.pushSprite(&dst, ui::cx() - kDigitsW / 2, kDigitsY - kDigitsH / 2);
  }

  stopwatch::Watch watch_;
  M5Canvas digits_{&M5.Display};
  Ring ring_{};
  char shown_[16] = {};
};

}  // namespace

App& stopwatchApp() {
  static StopwatchApp a;
  return a;
}
