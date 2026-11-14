#pragma once

#include <cstdint>

namespace fit {

enum class Units : uint8_t { Metric, Imperial };

constexpr uint8_t kSettingsVersion = 4;
// Short, medium and long exercise and rest times.
constexpr int8_t kWorkOffsetSec[] = {-10, 0, 10};
constexpr uint8_t kRestPercent[] = {50, 100, 200};
// Main exercises per workout. 0 uses the level default.
constexpr uint8_t kExerciseCounts[] = {0, 10, 15, 20};

// The option fields are indices into the tables above.
struct Settings {
  uint8_t version = kSettingsVersion;
  uint8_t workTime = 1;
  uint8_t restTime = 1;
  uint8_t exercises = 0;
};

struct Range {
  float min, max, step;
  int decimals;
};

bool isValid(const Settings& s);

// The ranges stay inside the limits of isValid(Profile) after conversion to kg and cm.
Range weightRange(Units u);
Range heightRange(Units u);
float toWeightUnits(float kg, Units u);
float fromWeightUnits(float value, Units u);
float toHeightUnits(int cm, Units u);
int fromHeightUnits(float value, Units u);
const char* weightUnit(Units u);
const char* heightUnit(Units u);

}  // namespace fit
