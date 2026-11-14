#include <Logging.h>

#include <cmath>
#include <cstdio>

#include "App.h"

namespace {

constexpr const char* TAG = "WEIGHT";

class WeightScreen : public Screen {
 public:
  void enter() override {
    picker_ = app::weightPicker("Today's weight", app::profile.weightKg);
    dirty = true;
  }

  void update(const ui::Input& in) override {
    if (in.blue) {
      app::go(ScreenId::Home);
      return;
    }
    if (ui::pickerUpdate(picker_, in)) dirty = true;
    if (in.green) {
      app::profile.weightKg = app::pickedKg(picker_);
      fit::addWeight(app::weights, app::now(), app::profile.weightKg);
      app::saveProfile();
      app::saveWeights();
      LOG_INF(TAG, "Logged %.1f kg (%u entries)", app::profile.weightKg, app::weights.count);
      app::go(ScreenId::Home);
    }
  }

  void draw() override {
    ui::clear();
    ui::pickerDraw(picker_, "save");
  }

 private:
  ui::Picker picker_{};
};

class Stats : public Screen {
 public:
  void update(const ui::Input& in) override {
    if (in.blue) app::go(ScreenId::Home);
  }

  void draw() override {
    ui::clear();
    const auto& p = app::progress;
    const auto& pr = app::profile;
    char line[48], weight[16], target[16];
    app::formatWeight(weight, sizeof(weight), pr.weightKg);
    app::formatWeight(target, sizeof(target), pr.targetKg);
    ui::text("Stats", ui::cx(), 80, &fonts::DejaVu24, ui::kGreen);
    snprintf(line, sizeof(line), "%u workouts  %lu min", p.workouts, static_cast<unsigned long>(p.activeSec / 60));
    ui::text(line, ui::cx(), 125, &fonts::DejaVu18);
    snprintf(line, sizeof(line), "~%.0f kcal burned", p.kcal);
    ui::text(line, ui::cx(), 155, &fonts::DejaVu18);
    const float b = fit::bmi(pr.weightKg, pr.heightCm);
    snprintf(line, sizeof(line), "%s  BMI %.1f %s", weight, b, fit::bmiClassName(fit::bmiClass(b)));
    ui::text(line, ui::cx(), 185, &fonts::DejaVu18);
    snprintf(line, sizeof(line), "Goal %s  ~%d weeks", target, fit::weeksToTarget(pr));
    ui::text(line, ui::cx(), 215, &fonts::DejaVu18, ui::kAmber);
    drawWeightGraph({ui::cx() - 140, 250, 280, 90});
    ui::hints(ui::kHintsY, "back", nullptr);
  }

 private:
  static void drawWeightGraph(const ui::Rect& r) {
    auto& c = ui::canvas();
    const auto& w = app::weights;
    c.drawRoundRect(r.x, r.y, r.w, r.h, 8, ui::kDark);
    if (w.count < 2) {
      ui::text("Log weight to see a graph", r.x + r.w / 2, r.y + r.h / 2, &fonts::DejaVu12, ui::kGrey);
      return;
    }
    float lo = w.entries[0].kg, hi = lo;
    for (int i = 0; i < w.count; ++i) {
      lo = std::fmin(lo, w.entries[i].kg);
      hi = std::fmax(hi, w.entries[i].kg);
    }
    const float span = std::fmax(hi - lo, 1.0f);
    int px = 0, py = 0;
    for (int i = 0; i < w.count; ++i) {
      const int x = r.x + 10 + (r.w - 20) * i / (w.count - 1);
      const int y = r.y + r.h - 10 - static_cast<int>((r.h - 20) * (w.entries[i].kg - lo) / span);
      if (i > 0) c.drawLine(px, py, x, y, ui::kGreen);
      c.fillCircle(x, y, 3, ui::kGreen);
      px = x;
      py = y;
    }
  }
};

}  // namespace

Screen& app::weightScreen() {
  static WeightScreen s;
  return s;
}

Screen& app::statsScreen() {
  static Stats s;
  return s;
}
