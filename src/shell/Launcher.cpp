#include <BoardSupport.h>
#include <I18n.h>

#include <cstdio>
#include <ctime>

#include "Shell.h"

namespace {

constexpr int kIconR = 80;
constexpr StrId kDays[] = {StrId::STR_SUN, StrId::STR_MON, StrId::STR_TUE, StrId::STR_WED,
                           StrId::STR_THU, StrId::STR_FRI, StrId::STR_SAT};
constexpr StrId kMonths[] = {StrId::STR_JAN, StrId::STR_FEB, StrId::STR_MAR, StrId::STR_APR,
                             StrId::STR_MAY, StrId::STR_JUN, StrId::STR_JUL, StrId::STR_AUG,
                             StrId::STR_SEP, StrId::STR_OCT, StrId::STR_NOV, StrId::STR_DEC};

class Launcher : public App {
 public:
  const AppInfo& info() const override {
    static const AppInfo i = {"launcher", tr(HOME), ui::kGrey, nullptr};
    return i;
  }

  void update(const ui::Input& in) override {
    const int n = entryCount();
    if (in.swipeLeft || in.swipeRight) {
      selected_ = (selected_ + (in.swipeLeft ? 1 : n - 1)) % n;
      dirty = true;
    }
    const bool onIcon =
        (in.tap || in.hold) && ui::Rect{ui::cx() - kIconR, kIconY - kIconR, 2 * kIconR, 2 * kIconR}.hit(in.x, in.y);
    if (in.green || (in.tap && onIcon)) {
      shell::open(entry(selected_));
      return;
    }
    if (in.hold && onIcon && &entry(selected_) != &settingsApp()) {
      shell::openAppPage(entry(selected_));
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
    char line[32];
    strftime(line, sizeof(line), "%H:%M", &local);
    ui::text(line, ui::cx(), 70, &fonts::DejaVu40);
    snprintf(line, sizeof(line), "%s %d %s", i18n::get(kDays[local.tm_wday]), local.tm_mday,
             i18n::get(kMonths[local.tm_mon]));
    ui::text(line, ui::cx(), 110, &fonts::DejaVu18, ui::kGrey);

    const AppInfo& a = entry(selected_).info();
    ui::canvas().fillCircle(ui::cx(), kIconY, kIconR, a.color);
    if (a.icon) a.icon(ui::cx(), kIconY, TFT_BLACK, a.color);
    ui::text(a.name, ui::cx(), 335, &fonts::DejaVu24);
    ui::hints(380, nullptr, tr(OPEN));
    ui::battery(ui::cx(), 420);
    ui::pageDots(selected_, entryCount());
  }

 private:
  static constexpr int kIconY = 215;

  static int entryCount() {
    int n;
    shell::apps(n);
    return n + 1;
  }

  // The settings come after the apps.
  static App& entry(int i) {
    int n;
    App* const* list = shell::apps(n);
    return i < n ? *list[i] : settingsApp();
  }

  int selected_ = 0;
  time_t shownMinute_ = 0;
};

}  // namespace

App& launcherApp() {
  static Launcher a;
  return a;
}
