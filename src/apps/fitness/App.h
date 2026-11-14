#pragma once

#include <Body.h>
#include <Plan.h>
#include <Settings.h>

#include <cstddef>

#include "../../shell/Shell.h"

enum class ScreenId : uint8_t { Onboarding, Home, Workout, Summary, Weight, Stats, Settings };

class Screen {
 public:
  virtual ~Screen() = default;
  virtual void enter() {}
  // Called when the user leaves the app.
  virtual void leave() {}
  virtual void update(const ui::Input& in) = 0;
  virtual void draw() = 0;
  // Called each frame period when the screen is on and not dirty. Use it to push small sprites.
  virtual void animate() {}
  bool dirty = true;
};

struct LastWorkout {
  int day = 0;
  uint32_t activeSec = 0;
  uint32_t totalSec = 0;
  float kcal = 0.0f;
};

namespace app {

extern fit::Profile profile;
extern fit::Progress progress;
extern fit::WeightLog weights;
extern LastWorkout last;
extern fit::Settings settings;

void go(ScreenId id);
void saveProfile();
void saveProgress();
void saveWeights();
void saveSettings();
fit::Units units();
// Makes a weight picker in the units of the settings.
ui::Picker weightPicker(const char* label, float kg);
float pickedKg(const ui::Picker& p);
// Builds the workout of the day with the level, the feedback and the time settings.
fit::Workout buildWorkout(int day);
// Writes the weight in the units of the settings, for example "85.0 kg" or "187 lb".
void formatWeight(char* buf, size_t n, float kg);
// The rows of the workout settings.
ui::Row* settingsRows(int& count);
uint32_t now();

Screen& onboardingScreen();
Screen& homeScreen();
Screen& workoutScreen();
Screen& summaryScreen();
Screen& weightScreen();
Screen& statsScreen();
Screen& settingsScreen();

}  // namespace app

App& fitnessApp();
