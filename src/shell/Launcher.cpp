#include <BoardSupport.h>
#include <I18n.h>

#include <cstdio>
#include <ctime>

#include "Shell.h"

namespace {

constexpr int kIconR = 80;
// In the move mode, the icon is smaller and has an arrow at each side.
constexpr int kLiftR = 62, kArrowX = 135;
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

  void start() override { moving_ = false; }

  void update(const ui::Input& in) override {
    if (time(nullptr) / 60 != shownMinute_) dirty = true;
    if (moving_) {
      updateMove(in);
      return;
    }
    const int n = entryCount();
    if (in.swipeLeft || in.swipeRight) {
      selected_ = (selected_ + (in.swipeLeft ? 1 : n - 1)) % n;
      dirty = true;
    }
    if (in.green || (in.tap && onIcon(in))) {
      shell::open(entry(selected_));
    } else if (in.hold && onIcon(in) && &entry(selected_) != &settingsApp()) {
      moving_ = true;
      board::vibrate(20);
      dirty = true;
    }
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
    if (moving_) {
      ui::text(tr(SWIPE_TO_MOVE), ui::cx(), 110, &fonts::DejaVu18, ui::kAmber);
    } else {
      snprintf(line, sizeof(line), "%s %d %s", i18n::get(kDays[local.tm_wday]), local.tm_mday,
               i18n::get(kMonths[local.tm_mon]));
      ui::text(line, ui::cx(), 110, &fonts::DejaVu18, ui::kGrey);
    }

    const AppInfo& a = entry(selected_).info();
    auto& c = ui::canvas();
    c.fillCircle(ui::cx(), kIconY, moving_ ? kLiftR : kIconR, a.color);
    if (a.icon) a.icon(ui::cx(), kIconY, TFT_BLACK, a.color);
    if (moving_) {
      for (int dir : {-1, 1}) {
        if (!canMove(dir)) continue;
        const int x = ui::cx() + dir * kArrowX;
        c.fillTriangle(x + dir * 14, kIconY, x - dir * 10, kIconY - 22, x - dir * 10, kIconY + 22, TFT_WHITE);
      }
      ui::text(tr(TAP_FOR_SETTINGS), ui::cx(), 300, &fonts::DejaVu18, ui::kGrey);
    }
    ui::text(a.name, ui::cx(), 335, &fonts::DejaVu24);
    ui::hints(380, moving_ ? tr(CANCEL) : nullptr, moving_ ? tr(DONE) : tr(OPEN));
    ui::battery(ui::cx(), 420);
    ui::pageDots(selected_, entryCount());
  }

 private:
  static constexpr int kIconY = 215;

  static bool onIcon(const ui::Input& in) {
    return ui::Rect{ui::cx() - kIconR, kIconY - kIconR, 2 * kIconR, 2 * kIconR}.hit(in.x, in.y);
  }

  // The touch area of an arrow is 88 x 120 px. Wear OS asks for 48 dp, that is 80 px on this display.
  static bool onArrow(const ui::Input& in, int dir) {
    return ui::Rect{ui::cx() + dir * kArrowX - 44, kIconY - 60, 88, 120}.hit(in.x, in.y);
  }

  // Settings stays the last entry.
  bool canMove(int dir) const {
    const int to = selected_ + dir;
    return to >= 0 && to < entryCount() - 1;
  }

  // A swipe or an arrow moves the app one place. Green keeps the order, and blue restores it.
  void updateMove(const ui::Input& in) {
    const int dir =
        in.swipeLeft || (in.tap && onArrow(in, -1)) ? -1 : (in.swipeRight || (in.tap && onArrow(in, 1)) ? 1 : 0);
    if (dir != 0) {
      if (!canMove(dir)) return;
      shell::swapApps(selected_, selected_ + dir);
      selected_ += dir;
    } else if (in.green) {
      shell::saveAppOrder();
      moving_ = false;
    } else if (in.blue) {
      const App& app = entry(selected_);
      shell::loadAppOrder();
      while (&entry(selected_) != &app) selected_ = (selected_ + 1) % entryCount();
      moving_ = false;
    } else if (in.tap && onIcon(in)) {
      shell::saveAppOrder();
      shell::openAppPage(entry(selected_));
      return;
    } else {
      return;
    }
    dirty = true;
  }

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
  bool moving_ = false;
  time_t shownMinute_ = 0;
};

}  // namespace

App& launcherApp() {
  static Launcher a;
  return a;
}
