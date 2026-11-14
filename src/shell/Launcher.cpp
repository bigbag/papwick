#include <BoardSupport.h>

#include <cstdio>
#include <ctime>

#include "Shell.h"

namespace {

constexpr int kIconR = 80;

class Launcher : public App {
 public:
  const AppInfo& info() const override {
    static const AppInfo i = {"launcher", "Home", ui::kGrey, ""};
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
    char line[16];
    strftime(line, sizeof(line), "%H:%M", &local);
    ui::text(line, ui::cx(), 70, &fonts::DejaVu40);

    const AppInfo& a = entry(selected_).info();
    ui::canvas().fillCircle(ui::cx(), kIconY, kIconR, a.color);
    ui::text(a.glyph, ui::cx(), kIconY, &fonts::DejaVu40, TFT_BLACK);
    ui::text(a.name, ui::cx(), 335, &fonts::DejaVu24);
    ui::hints(380, nullptr, "open");
    snprintf(line, sizeof(line), "%d%%%s", board::batteryLevel(), board::onUsb() ? " USB" : "");
    ui::text(line, ui::cx(), 420, &fonts::DejaVu18, ui::kGrey);
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
