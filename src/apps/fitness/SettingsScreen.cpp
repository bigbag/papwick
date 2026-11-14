#include <Logging.h>

#include "App.h"

namespace {

constexpr const char* TAG = "SETTINGS";

class SettingsScreen : public Screen {
 public:
  void update(const ui::Input& in) override {
    if (in.blue) {
      app::go(ScreenId::Home);
      return;
    }
    int n;
    ui::Row* rows = app::settingsRows(n);
    const int i = ui::rowsUpdate(rows, n, in);
    if (i < 0) return;
    app::saveSettings();
    LOG_INF(TAG, "%s: %s", rows[i].label, rows[i].options[*rows[i].value]);
    dirty = true;
  }

  void draw() override {
    ui::clear();
    int n;
    const ui::Row* rows = app::settingsRows(n);
    ui::rowsDraw("Workout", rows, n);
    ui::hints(ui::kHintsY, "back", nullptr);
  }
};

}  // namespace

Screen& app::settingsScreen() {
  static SettingsScreen s;
  return s;
}
