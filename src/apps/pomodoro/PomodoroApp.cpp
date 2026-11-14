#include <BoardSupport.h>
#include <I18n.h>
#include <Logging.h>
#include <Pomodoro.h>

#include <cstdio>

#include "App.h"

namespace {

constexpr const char* TAG = "POMODORO";
constexpr uint16_t kFocusColor = ui::kAmber;
constexpr uint16_t kBreakColor = lgfx::color565(190, 160, 220);

struct PomodoroSettings {
  uint8_t version = 1;
  uint8_t manual = 0;
};

class PomodoroApp : public App {
 public:
  const AppInfo& info() const override {
    static const AppInfo i = {"pomodoro", tr(POMODORO), lgfx::color565(200, 170, 205), icon::hourglass};
    return i;
  }

  void boot() override {
    if (!prefs_.begin(info().id, false)) LOG_ERR(TAG, "NVS open failed");
    PomodoroSettings s;
    if (shell::load(prefs_, "settings", s) && s.version == PomodoroSettings{}.version && s.manual <= 1) settings_ = s;
    applyMode();
  }

  // The timer runs on in the background, so the alarm also comes on the home screen or in another app.
  void background() override { tick(millis()); }

  void start() override { askReset_ = false; }

  void update(const ui::Input& in) override {
    const uint32_t now = millis();
    // The user pressed green for the old period, so a period change ignores the press.
    const bool changed = tick(now);
    if (askReset_) {
      if (in.green) {
        pomodoro::reset(timer_);
        LOG_INF(TAG, "Reset");
      }
      if (in.green || in.blue) {
        askReset_ = false;
        dirty = true;
      }
      return;
    }
    if (in.blue) {
      shell::home();
      return;
    }
    if (in.hold && timer_.run != pomodoro::RunState::Idle) {
      askReset_ = true;
      dirty = true;
      return;
    }
    if (!in.green || changed) return;
    switch (timer_.run) {
      case pomodoro::RunState::Idle:
      case pomodoro::RunState::Waiting:
        pomodoro::start(timer_, now);
        break;
      case pomodoro::RunState::Running:
        if (timer_.period == pomodoro::Period::Focus) {
          pomodoro::pause(timer_, now);
        } else {
          pomodoro::skipBreak(timer_, now);
        }
        break;
      case pomodoro::RunState::Paused:
        pomodoro::resume(timer_, now);
        break;
    }
    board::vibrate(30);
    dirty = true;
  }

  void draw() override {
    if (askReset_) {
      ui::confirmDraw(tr(RESET_TIMER_Q), tr(YES));
      return;
    }
    ui::clear();
    const bool focus = timer_.period == pomodoro::Period::Focus;
    const uint16_t color = focus ? kFocusColor : kBreakColor;
    ui::text(periodName(), ui::cx(), 120, &fonts::DejaVu24, color);
    shownSec_ = (timer_.remainingMs + 999) / 1000;
    char line[24];
    snprintf(line, sizeof(line), "%02lu:%02lu", static_cast<unsigned long>(shownSec_ / 60),
             static_cast<unsigned long>(shownSec_ % 60));
    ui::text(line, ui::cx(), ui::cy(), &fonts::DejaVu72);
    // A break belongs to the focus session before it.
    const unsigned session = focus ? timer_.completedFocus + 1 : timer_.completedFocus;
    snprintf(line, sizeof(line), tr(FMT_SESSION), session < 1 ? 1 : (session > 4 ? 4 : session));
    ui::text(line, ui::cx(), 300, &fonts::DejaVu18, ui::kGrey);
    ui::text(stateText(), ui::cx(), 340, &fonts::DejaVu18, ui::kGrey);
    const uint32_t total = pomodoro::durationMs(timer_.period);
    ui::progressRing(1.0f - static_cast<float>(timer_.remainingMs) / total, color);
    ui::hints(ui::kHintsY, tr(BACK), greenHint());
  }

  ui::Row* settings(int& count) override {
    static const char* const modes[] = {tr(AUTO), tr(MANUAL)};
    static ui::Row rows[] = {{tr(START_MODE), modes, 2, &settings_.manual}};
    count = 1;
    return rows;
  }

  void settingsChanged() override {
    shell::save(prefs_, "settings", settings_);
    applyMode();
  }

  void clearData() override {
    App::clearData();
    settings_ = PomodoroSettings{};
    timer_ = pomodoro::Timer{};
    applyMode();
  }

 private:
  void applyMode() { timer_.startMode = settings_.manual ? pomodoro::StartMode::Manual : pomodoro::StartMode::Auto; }

  bool tick(uint32_t now) {
    const pomodoro::Period before = timer_.period;
    const pomodoro::RunState run = timer_.run;
    pomodoro::tick(timer_, now);
    const bool changed = timer_.period != before || timer_.run != run;
    if (changed) alarm();
    if (!askReset_ && (timer_.remainingMs + 999) / 1000 != shownSec_) dirty = true;
    return changed;
  }

  void alarm() {
    LOG_INF(TAG, "Period end, next %d", static_cast<int>(timer_.period));
    board::wake();
    board::vibrate(400, 200, 3);
    board::beep(880, 200, 100);
    board::beep(880, 200, 100);
    board::beep(1320, 400);
    dirty = true;
  }

  const char* periodName() const {
    if (timer_.period == pomodoro::Period::ShortBreak) return tr(SHORT_BREAK);
    if (timer_.period == pomodoro::Period::LongBreak) return tr(LONG_BREAK);
    return tr(FOCUS);
  }

  const char* stateText() const {
    switch (timer_.run) {
      case pomodoro::RunState::Idle:
        return tr(READY);
      case pomodoro::RunState::Paused:
        return tr(PAUSED);
      case pomodoro::RunState::Waiting:
        return timer_.period == pomodoro::Period::Focus ? tr(START_FOCUS) : tr(START_BREAK);
      case pomodoro::RunState::Running:
        break;
    }
    return "";
  }

  const char* greenHint() const {
    if (timer_.run == pomodoro::RunState::Paused) return tr(RESUME);
    if (timer_.run != pomodoro::RunState::Running) return tr(START);
    return timer_.period == pomodoro::Period::Focus ? tr(PAUSE) : tr(SKIP);
  }

  Preferences prefs_;
  PomodoroSettings settings_;
  pomodoro::Timer timer_;
  uint32_t shownSec_ = 0;
  bool askReset_ = false;
};

}  // namespace

App& pomodoroApp() {
  static PomodoroApp a;
  return a;
}
