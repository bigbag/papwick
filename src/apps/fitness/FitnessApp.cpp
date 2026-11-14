#include <Logging.h>
#include <Preferences.h>

#include <algorithm>
#include <cstdio>

#include "App.h"

namespace {

constexpr const char* TAG = "FITNESS";

Preferences prefs;
Screen* screen = nullptr;
ScreenId screenId = ScreenId::Onboarding;
bool hasProfile = false;

constexpr const char* kCount[] = {"Auto", "10", "15", "20"};
constexpr const char* kLength[] = {"Short", "Med", "Long"};
static_assert(sizeof(kCount) / sizeof(*kCount) == sizeof(fit::kExerciseCounts), "exercise count labels");
static_assert(sizeof(kLength) / sizeof(*kLength) == sizeof(fit::kWorkOffsetSec), "exercise time labels");
static_assert(sizeof(kLength) / sizeof(*kLength) == sizeof(fit::kRestPercent), "rest time labels");

const char* screenName(ScreenId id) {
  static const char* const names[] = {"onboarding", "home", "workout", "summary", "weight", "stats", "settings"};
  return names[static_cast<int>(id)];
}

template <typename T>
bool load(const char* key, T& value) {
  T tmp;
  if (!prefs.isKey(key) || prefs.getBytes(key, &tmp, sizeof(tmp)) != sizeof(tmp) || !fit::isValid(tmp)) {
    LOG_INF(TAG, "No valid '%s' in NVS", key);
    return false;
  }
  value = tmp;
  return true;
}

template <typename T>
void store(const char* key, const T& value) {
  if (prefs.putBytes(key, &value, sizeof(value)) != sizeof(value)) LOG_ERR(TAG, "Save '%s' failed", key);
}

// Version 3 kept the device settings in the fitness settings. Move them to the platform once.
void migrateSettings() {
  fit::SettingsV3 old;
  if (!prefs.isKey("settings") || prefs.getBytes("settings", &old, sizeof(old)) != sizeof(old)) return;
  if (!fit::migrate(old, app::settings, shell::device())) {
    LOG_WRN(TAG, "Invalid version 3 settings, use defaults");
    return;
  }
  // The device settings go first: the version 3 record is their only copy until then.
  if (!shell::saveDevice()) return;
  app::saveSettings();
  LOG_INF(TAG, "Settings moved from version 3");
}

Screen& screenFor(ScreenId id) {
  switch (id) {
    case ScreenId::Home:
      return app::homeScreen();
    case ScreenId::Workout:
      return app::workoutScreen();
    case ScreenId::Summary:
      return app::summaryScreen();
    case ScreenId::Weight:
      return app::weightScreen();
    case ScreenId::Stats:
      return app::statsScreen();
    case ScreenId::Settings:
      return app::settingsScreen();
    default:
      return app::onboardingScreen();
  }
}

class Fitness : public App {
 public:
  const AppInfo& info() const override {
    static const AppInfo i = {"fit", "Workout", lgfx::color565(149, 117, 205), icon::dumbbell};
    return i;
  }

  void boot() override {
    if (!prefs.begin(info().id, false)) LOG_ERR(TAG, "NVS open failed");
    if (!load("settings", app::settings)) migrateSettings();
    hasProfile = load("profile", app::profile);
    load("progress", app::progress);
    load("weights", app::weights);
    LOG_INF(TAG, "Profile %s, day %d of %d, %u workouts", hasProfile ? "loaded" : "missing",
            fit::currentDay(app::progress) + 1, fit::kPlanDays, app::progress.workouts);
  }

  // A paused workout stays open until the user ends it.
  void start() override {
    if (!hasProfile) {
      app::go(ScreenId::Onboarding);
    } else if (screenId != ScreenId::Workout) {
      app::go(ScreenId::Home);
    }
  }

  void stop() override { screen->leave(); }

  void update(const ui::Input& in) override {
    screen->update(in);
    if (screen->dirty) dirty = true;
  }

  void draw() override {
    screen->dirty = false;
    screen->draw();
  }

  void animate() override { screen->animate(); }

  bool keepAwake() const override { return screenId == ScreenId::Workout; }

  ui::Row* settings(int& count) override { return app::settingsRows(count); }

  void settingsChanged() override { app::saveSettings(); }

  void clearData() override {
    App::clearData();
    app::settings = fit::Settings{};
    app::profile = fit::Profile{};
    app::progress = fit::Progress{};
    app::weights = fit::WeightLog{};
    hasProfile = false;
    screenId = ScreenId::Onboarding;
    screen = &app::onboardingScreen();
  }
};

}  // namespace

namespace app {

fit::Profile profile;
fit::Progress progress;
fit::WeightLog weights;
LastWorkout last;
fit::Settings settings;

void go(ScreenId id) {
  LOG_INF(TAG, "Screen -> %s", screenName(id));
  if (id == ScreenId::Home) hasProfile = true;
  screenId = id;
  screen = &screenFor(id);
  screen->enter();
  screen->dirty = true;
  fitnessApp().dirty = true;
}

void saveProfile() { store("profile", profile); }
void saveProgress() { store("progress", progress); }
void saveWeights() { store("weights", weights); }
void saveSettings() { store("settings", settings); }

fit::Units units() { return static_cast<fit::Units>(shell::device().units); }

fit::Workout buildWorkout(int day) {
  return fit::buildWorkout(day, profile.level, profile.lowImpact,
                           progress.adjustSec + fit::kWorkOffsetSec[settings.workTime],
                           fit::kRestPercent[settings.restTime], fit::kExerciseCounts[settings.exercises]);
}

ui::Picker weightPicker(const char* label, float kg) {
  const fit::Units u = units();
  const fit::Range r = fit::weightRange(u);
  // 40.0 kg shows as 88 lb, below the 89 lb minimum. A value out of range would give an invalid profile.
  const float value = std::clamp(fit::toWeightUnits(kg, u), r.min, r.max);
  return {label, fit::weightUnit(u), value, r.min, r.max, r.step, r.decimals};
}

float pickedKg(const ui::Picker& p) { return fit::fromWeightUnits(p.value, units()); }

void formatWeight(char* buf, size_t n, float kg) {
  const fit::Units u = units();
  snprintf(buf, n, "%.*f %s", fit::weightRange(u).decimals, fit::toWeightUnits(kg, u), fit::weightUnit(u));
}

ui::Row* settingsRows(int& count) {
  static ui::Row rows[] = {{"Exercises", kCount, 4, &settings.exercises},
                           {"Exercise time", kLength, 3, &settings.workTime},
                           {"Rest time", kLength, 3, &settings.restTime}};
  count = sizeof(rows) / sizeof(*rows);
  return rows;
}

uint32_t now() { return static_cast<uint32_t>(time(nullptr)); }

}  // namespace app

App& fitnessApp() {
  static Fitness a;
  return a;
}
