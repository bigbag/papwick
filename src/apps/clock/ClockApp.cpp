#include <BoardSupport.h>
#include <I18n.h>
#include <Logging.h>

#include <cmath>
#include <cstdio>
#include <ctime>

#include "App.h"

namespace {

constexpr const char* TAG = "CLOCK";
constexpr StrId kDays[] = {StrId::STR_SUN, StrId::STR_MON, StrId::STR_TUE, StrId::STR_WED,
                           StrId::STR_THU, StrId::STR_FRI, StrId::STR_SAT};
constexpr StrId kMonths[] = {StrId::STR_JAN, StrId::STR_FEB, StrId::STR_MAR, StrId::STR_APR,
                             StrId::STR_MAY, StrId::STR_JUN, StrId::STR_JUL, StrId::STR_AUG,
                             StrId::STR_SEP, StrId::STR_OCT, StrId::STR_NOV, StrId::STR_DEC};
constexpr float kPi = 3.14159265f;

struct ClockSettings {
  uint8_t version = 1;
  uint8_t face = 0;
};

// The face is mostly black, so the AMOLED uses little power. It changes once a minute and follows the sleep setting.
class ClockApp : public App {
 public:
  const AppInfo& info() const override {
    static const AppInfo i = {"clock", tr(CLOCK), lgfx::color565(200, 200, 205), icon::clock};
    return i;
  }

  void boot() override {
    if (!prefs_.begin(info().id, false)) LOG_ERR(TAG, "NVS open failed");
    ClockSettings s;
    if (shell::load(prefs_, "settings", s) && s.version == ClockSettings{}.version && s.face <= 1) settings_ = s;
  }

  void update(const ui::Input& in) override {
    if (in.blue) {
      shell::home();
      return;
    }
    if (in.green || in.tap) {
      settings_.face ^= 1;
      settingsChanged();
      dirty = true;
      return;
    }
    if (time(nullptr) / 60 != shownMinute_) dirty = true;
  }

  void draw() override {
    ui::clear();
    const time_t now = time(nullptr);
    shownMinute_ = now / 60;
    tm local;
    localtime_r(&now, &local);
    char date[24];
    snprintf(date, sizeof(date), "%s %d %s", i18n::get(kDays[local.tm_wday]), local.tm_mday,
             i18n::get(kMonths[local.tm_mon]));
    if (settings_.face == 0) {
      drawAnalog(local, date);
    } else {
      drawDigital(local, date);
    }
  }

  ui::Row* settings(int& count) override {
    static const char* const faces[] = {tr(ANALOG), tr(DIGITAL)};
    static ui::Row rows[] = {{tr(FACE), faces, 2, &settings_.face}};
    count = 1;
    return rows;
  }

  void settingsChanged() override { shell::save(prefs_, "settings", settings_); }

  void clearData() override {
    App::clearData();
    settings_ = ClockSettings{};
  }

 private:
  // drawWideLine() takes the half width.
  static void hand(float turns, int length, float halfWidth, uint16_t color) {
    const float a = turns * 2 * kPi - kPi / 2;
    ui::canvas().drawWideLine(ui::cx(), ui::cy(), ui::cx() + length * cosf(a), ui::cy() + length * sinf(a), halfWidth,
                              color);
  }

  static void drawAnalog(const tm& t, const char* date) {
    auto& c = ui::canvas();
    for (int i = 0; i < 60; ++i) {
      const float a = i * 2 * kPi / 60;
      const float x0 = ui::cx() + 220 * cosf(a), y0 = ui::cy() + 220 * sinf(a);
      if (i % 5 == 0) {
        c.drawWideLine(x0, y0, ui::cx() + 196 * cosf(a), ui::cy() + 196 * sinf(a), 2.5f, TFT_WHITE);
      } else {
        c.drawLine(x0, y0, ui::cx() + 210 * cosf(a), ui::cy() + 210 * sinf(a), ui::kGrey);
      }
    }
    ui::text(date, ui::cx(), ui::cy() + 80, &fonts::DejaVu18, ui::kGrey);
    ui::battery(ui::cx(), ui::cy() + 120);
    hand((t.tm_hour % 12 + t.tm_min / 60.0f) / 12, 110, 5, TFT_WHITE);
    hand(t.tm_min / 60.0f, 175, 3, TFT_WHITE);
    c.fillCircle(ui::cx(), ui::cy(), 8, ui::kAmber);
  }

  static void drawDigital(const tm& t, const char* date) {
    char line[8];
    strftime(line, sizeof(line), "%H:%M", &t);
    ui::text(line, ui::cx(), ui::cy() - 10, &fonts::DejaVu72);
    ui::text(date, ui::cx(), ui::cy() + 60, &fonts::DejaVu24, ui::kGrey);
    ui::battery(ui::cx(), ui::cy() + 120);
  }

  Preferences prefs_;
  ClockSettings settings_;
  time_t shownMinute_ = 0;
};

}  // namespace

App& clockApp() {
  static ClockApp a;
  return a;
}
