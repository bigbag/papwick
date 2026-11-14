#include <Logging.h>
#include <Session.h>

#include <cstdio>

#include "App.h"

namespace {

constexpr const char* TAG = "HOME";

const ui::Rect kWeight = {62, 290, 110, 46};
const ui::Rect kStats = {178, 290, 110, 46};
const ui::Rect kSettings = {294, 290, 110, 46};

class Home : public Screen {
 public:
  void enter() override { dirty = true; }

  void update(const ui::Input& in) override {
    const int day = fit::currentDay(app::progress);
    if (in.blue) {
      shell::home();
      return;
    }
    if (in.green) {
      if (day >= fit::kPlanDays) {
        app::progress.doneMask = 0;
        app::saveProgress();
        LOG_INF(TAG, "Plan restarted");
        dirty = true;
      } else if (fit::dayType(day) == fit::DayType::Rest) {
        fit::markDone(app::progress, day);
        app::saveProgress();
        LOG_INF(TAG, "Rest day %d done", day + 1);
        dirty = true;
      } else {
        app::go(ScreenId::Workout);
      }
      return;
    }
    if (in.tap && kWeight.hit(in.x, in.y)) app::go(ScreenId::Weight);
    if (in.tap && kStats.hit(in.x, in.y)) app::go(ScreenId::Stats);
    if (in.tap && kSettings.hit(in.x, in.y)) app::go(ScreenId::Settings);
  }

  void draw() override {
    ui::clear();
    const int day = fit::currentDay(app::progress);
    drawDayRing(day);

    char line[40];
    ui::text(fit::levelName(app::profile.level), ui::cx(), 72, &fonts::DejaVu18, ui::kGrey);
    if (day >= fit::kPlanDays) {
      ui::text("Plan complete!", ui::cx(), 140, &fonts::DejaVu40, ui::kGreen);
      ui::text("28 of 28 days", ui::cx(), 195, &fonts::DejaVu18);
      ui::hints(ui::kHintsY, "apps", "restart");
    } else {
      snprintf(line, sizeof(line), "DAY %d", day + 1);
      ui::text(line, ui::cx(), 118, &fonts::DejaVu40);
      const fit::DayType type = fit::dayType(day);
      if (type == fit::DayType::Rest) {
        ui::text("Rest day", ui::cx(), 168, &fonts::DejaVu24, ui::kBlue);
        ui::text("Walk, stretch, drink water", ui::cx(), 202, &fonts::DejaVu12, ui::kGrey);
        ui::hints(ui::kHintsY, "apps", "rest done");
      } else {
        const fit::Workout w = app::buildWorkout(day);
        snprintf(line, sizeof(line), "%s  %lu min", fit::dayTypeName(type),
                 static_cast<unsigned long>((fit::totalSeconds(w) + fit::kGetReadyMs / 1000 + 59) / 60));
        ui::text(line, ui::cx(), 168, &fonts::DejaVu24);
        snprintf(line, sizeof(line), "%d exercises", fit::exerciseTotal(w));
        ui::text(line, ui::cx(), 202, &fonts::DejaVu18, ui::kGrey);
        ui::hints(ui::kHintsY, "apps", "start");
      }
    }
    ui::button(kWeight, "Weight", ui::kDark);
    ui::button(kStats, "Stats", ui::kDark);
    ui::button(kSettings, "Settings", ui::kDark);
  }

 private:
  static void drawDayRing(int current) {
    constexpr float kSeg = 360.0f / fit::kPlanDays;
    for (int d = 0; d < fit::kPlanDays; ++d) {
      uint16_t color = ui::kDark;
      if (fit::isDone(app::progress, d)) {
        color = fit::dayType(d) == fit::DayType::Rest ? ui::kBlue : ui::kGreen;
      } else if (d == current) {
        color = TFT_WHITE;
      }
      const float from = 270.0f + d * kSeg + 1.0f;
      ui::edgeArc(from, from + kSeg - 2.0f, color);
    }
  }
};

}  // namespace

Screen& app::homeScreen() {
  static Home s;
  return s;
}
