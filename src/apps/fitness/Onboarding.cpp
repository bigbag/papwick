#include <Logging.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <initializer_list>

#include "App.h"

namespace {

constexpr const char* TAG = "ONBOARD";

enum Step : uint8_t { Welcome, Safety, SexStep, Age, Height, Weight, Target, PushUps, LowImpact, Result, kStepCount };

ui::Rect choiceRect(int i, int n) {
  const int h = 62, gap = 18;
  const int top = ui::cy() - (n * h + (n - 1) * gap) / 2 + 25;
  return {ui::cx() - 140, top + i * (h + gap), 280, h};
}

class Onboarding : public Screen {
 public:
  void enter() override {
    step_ = Welcome;
    profile_ = fit::Profile{};
    targetSet_ = false;
    setupPickers();
    dirty = true;
  }

  void update(const ui::Input& in) override {
    if (in.blue) {
      if (step_ == Welcome) {
        shell::home();
        return;
      }
      step_ = static_cast<Step>(step_ - 1);
      dirty = true;
      return;
    }
    if (ui::Picker* p = picker()) {
      if (ui::pickerUpdate(*p, in)) dirty = true;
      if (in.green) next();
      return;
    }
    const int n = choiceCount();
    const int chosen = in.tap ? hitChoice(in.x, in.y, n) : -1;
    switch (step_) {
      case Welcome:
      case Safety:
        if (in.green) next();
        break;
      case SexStep:
        if (chosen >= 0) {
          profile_.sex = static_cast<fit::Sex>(chosen);
          next();
        }
        break;
      case PushUps:
        if (chosen >= 0) {
          profile_.level = static_cast<fit::Level>(chosen);
          next();
        }
        break;
      case LowImpact:
        if (chosen >= 0) {
          profile_.lowImpact = chosen == 0;
          next();
        }
        break;
      case Result:
        if (in.green) finish();
        break;
      default:
        break;
    }
  }

  void draw() override {
    ui::clear();
    if (const ui::Picker* p = picker()) {
      ui::pickerDraw(*p);
      return;
    }
    switch (step_) {
      case Welcome:
        ui::text("Home Workout", ui::cx(), ui::cy() - 60, &fonts::DejaVu40, ui::kGreen);
        ui::text("28-day plan, no equipment", ui::cx(), ui::cy(), &fonts::DejaVu18);
        ui::hints(ui::kHintsY, "back", "start");
        break;
      case Safety:
        ui::text("Safety first", ui::cx(), 95, &fonts::DejaVu24, ui::kAmber);
        ui::text("Talk to a doctor first if you", ui::cx(), 150, &fonts::DejaVu18);
        ui::text("have heart, joint or blood", ui::cx(), 178, &fonts::DejaVu18);
        ui::text("pressure problems.", ui::cx(), 206, &fonts::DejaVu18);
        ui::text("Stop at once if you feel chest", ui::cx(), 250, &fonts::DejaVu18);
        ui::text("pain, dizziness or unusual", ui::cx(), 278, &fonts::DejaVu18);
        ui::text("shortness of breath.", ui::cx(), 306, &fonts::DejaVu18);
        ui::hints(ui::kHintsY, "back", "I understand");
        break;
      case SexStep:
        drawChoices("Your sex", {"Male", "Female"});
        break;
      case PushUps:
        drawChoices("Push-ups in a row?", {"0 - 10", "11 - 25", "More than 25"});
        break;
      case LowImpact:
        drawChoices("Avoid jumping?", {"Yes, low impact", "No, jumps are OK"});
        if (fit::suggestLowImpact(profile_)) {
          ui::text("Low impact is better for your joints", ui::cx(), 375, &fonts::DejaVu12, ui::kAmber);
        }
        break;
      case Result:
        drawResult();
        break;
      default:
        break;
    }
  }

 private:
  ui::Picker* picker() {
    if (step_ >= Age && step_ <= Target) return &pickers_[step_ - Age];
    return nullptr;
  }

  int choiceCount() const {
    switch (step_) {
      case SexStep:
      case LowImpact:
        return 2;
      case PushUps:
        return 3;
      default:
        return 0;
    }
  }

  int hitChoice(int x, int y, int n) const {
    // The hit areas overlap. The lower choice wins, because its drawn area starts in the overlap.
    for (int i = n - 1; i >= 0; --i) {
      if (choiceRect(i, n).hit(x, y)) return i;
    }
    return -1;
  }

  void drawChoices(const char* title, std::initializer_list<const char*> labels) {
    ui::text(title, ui::cx(), 110, &fonts::DejaVu24, ui::kGrey);
    int i = 0;
    for (const char* label : labels) ui::button(choiceRect(i++, labels.size()), label, ui::kDark);
    ui::hints(ui::kHintsY, "back", nullptr);
  }

  void drawResult() {
    char line[48];
    const float b = fit::bmi(profile_.weightKg, profile_.heightCm);
    ui::text("Your plan", ui::cx(), 90, &fonts::DejaVu24, ui::kGreen);
    snprintf(line, sizeof(line), "BMI %.1f  %s", b, fit::bmiClassName(fit::bmiClass(b)));
    ui::text(line, ui::cx(), 145, &fonts::DejaVu18);
    snprintf(line, sizeof(line), "Resting burn %d kcal/day", fit::bmrKcal(profile_));
    ui::text(line, ui::cx(), 180, &fonts::DejaVu18);
    const int weeks = fit::weeksToTarget(profile_);
    char target[16];
    app::formatWeight(target, sizeof(target), profile_.targetKg);
    if (weeks > 0) {
      snprintf(line, sizeof(line), "Goal %s in ~%d weeks", target, weeks);
    } else {
      snprintf(line, sizeof(line), "Goal: keep %s", target);
    }
    ui::text(line, ui::cx(), 215, &fonts::DejaVu18);
    snprintf(line, sizeof(line), "%s%s", fit::levelName(profile_.level), profile_.lowImpact ? ", low impact" : "");
    ui::text(line, ui::cx(), 250, &fonts::DejaVu18, ui::kAmber);
    ui::text("Estimates only, not medical advice", ui::cx(), 290, &fonts::DejaVu12, ui::kGrey);
    ui::hints(ui::kHintsY, "back", "start plan");
  }

  void setupPickers() {
    const fit::Units u = app::units();
    const fit::Range h = fit::heightRange(u);
    pickers_[0] = {"Age", "years", static_cast<float>(profile_.age), 16, 90, 1, 0};
    pickers_[1] = {
        "Height", fit::heightUnit(u), std::clamp(fit::toHeightUnits(profile_.heightCm, u), h.min, h.max), h.min, h.max,
        h.step,   h.decimals};
    pickers_[2] = app::weightPicker("Weight", profile_.weightKg);
    pickers_[3] = app::weightPicker("Target weight", profile_.targetKg);
  }

  void next() {
    if (step_ == Age) profile_.age = static_cast<uint8_t>(pickers_[0].value);
    if (step_ == Height) profile_.heightCm = fit::fromHeightUnits(pickers_[1].value, app::units());
    if (step_ == Weight) {
      profile_.weightKg = app::pickedKg(pickers_[2]);
      if (!targetSet_) pickers_[3] = app::weightPicker("Target weight", std::fmax(40.0f, profile_.weightKg - 5.0f));
    }
    if (step_ == Target) {
      profile_.targetKg = app::pickedKg(pickers_[3]);
      targetSet_ = true;
    }
    step_ = static_cast<Step>(step_ + 1);
    dirty = true;
    LOG_DBG(TAG, "Step %d", step_);
  }

  void finish() {
    app::profile = profile_;
    app::progress = fit::Progress{};
    app::weights = fit::WeightLog{};
    fit::addWeight(app::weights, app::now(), profile_.weightKg);
    app::saveProfile();
    app::saveProgress();
    app::saveWeights();
    LOG_INF(TAG, "Profile: sex=%d age=%u height=%u cm weight=%.1f kg target=%.1f kg level=%s lowImpact=%d",
            static_cast<int>(profile_.sex), profile_.age, profile_.heightCm, profile_.weightKg, profile_.targetKg,
            fit::levelName(profile_.level), profile_.lowImpact);
    app::go(ScreenId::Home);
  }

  Step step_ = Welcome;
  ui::Picker pickers_[4];
  fit::Profile profile_;
  bool targetSet_ = false;
};

}  // namespace

Screen& app::onboardingScreen() {
  static Onboarding s;
  return s;
}
