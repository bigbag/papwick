#include "Settings.h"

#include <cmath>

namespace fit {

namespace {

constexpr float kLbPerKg = 2.20462f;
constexpr float kCmPerIn = 2.54f;

template <typename T, int N>
constexpr int count(const T (&)[N]) {
  return N;
}

}  // namespace

bool isValid(const Settings& s) {
  return s.version == kSettingsVersion && s.workTime < count(kWorkOffsetSec) && s.restTime < count(kRestPercent) &&
         s.exercises < count(kExerciseCounts);
}

bool migrate(const SettingsV3& old, Settings& out, sys::Device& device) {
  const Settings s = {kSettingsVersion, old.workTime, old.restTime, old.exercises};
  sys::Device d = device;
  d.sleep = old.sleep;
  d.units = static_cast<uint8_t>(old.units);
  d.volume = old.volume;
  d.vibration = old.vibration;
  d.brightness = old.brightness;
  if (old.version != 3 || !isValid(s) || !sys::isValid(d)) return false;
  out = s;
  device = d;
  return true;
}

Range weightRange(Units u) { return u == Units::Metric ? Range{40, 200, 0.5f, 1} : Range{89, 440, 1, 0}; }

Range heightRange(Units u) { return u == Units::Metric ? Range{140, 210, 1, 0} : Range{56, 82, 1, 0}; }

float toWeightUnits(float kg, Units u) {
  if (u == Units::Metric) return kg;
  return std::round(kg * kLbPerKg);
}

float fromWeightUnits(float value, Units u) { return u == Units::Metric ? value : value / kLbPerKg; }

float toHeightUnits(int cm, Units u) {
  if (u == Units::Metric) return cm;
  return std::round(cm / kCmPerIn);
}

int fromHeightUnits(float value, Units u) {
  return static_cast<int>(std::lround(u == Units::Metric ? value : value * kCmPerIn));
}

const char* weightUnit(Units u) { return u == Units::Metric ? "kg" : "lb"; }

const char* heightUnit(Units u) { return u == Units::Metric ? "cm" : "in"; }

}  // namespace fit
