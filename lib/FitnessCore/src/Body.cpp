#include "Body.h"

#include <cmath>

namespace fit {

bool isValid(const Profile& p) {
  return p.version == kProfileVersion && p.age >= 16 && p.age <= 90 && p.heightCm >= 140 && p.heightCm <= 210 &&
         p.weightKg >= 40.0f && p.weightKg <= 200.0f && p.targetKg >= 40.0f && p.targetKg <= 200.0f &&
         static_cast<uint8_t>(p.level) <= static_cast<uint8_t>(Level::Advanced) &&
         static_cast<uint8_t>(p.sex) <= static_cast<uint8_t>(Sex::Female);
}

bool isValid(const WeightLog& log) {
  if (log.version != kWeightLogVersion || log.count > kMaxWeights) return false;
  for (int i = 0; i < log.count; ++i) {
    const float kg = log.entries[i].kg;
    if (!std::isfinite(kg) || kg < 40.0f || kg > 200.0f) return false;
  }
  return true;
}

void addWeight(WeightLog& log, uint32_t time, float kg) {
  if (log.count == kMaxWeights) {
    for (int i = 1; i < kMaxWeights; ++i) log.entries[i - 1] = log.entries[i];
    --log.count;
  }
  log.entries[log.count++] = {time, kg};
}

float bmi(float kg, int heightCm) {
  const float m = heightCm / 100.0f;
  return m > 0.0f ? kg / (m * m) : 0.0f;
}

BmiClass bmiClass(float bmiValue) {
  if (bmiValue < 18.5f) return BmiClass::Underweight;
  if (bmiValue < 25.0f) return BmiClass::Healthy;
  if (bmiValue < 30.0f) return BmiClass::Overweight;
  return BmiClass::Obese;
}

const char* bmiClassName(BmiClass c) {
  static const char* const names[] = {"Underweight", "Healthy", "Overweight", "Obese"};
  return names[static_cast<int>(c)];
}

const char* levelName(Level l) {
  static const char* const names[] = {"Beginner", "Intermediate", "Advanced"};
  return names[static_cast<int>(l)];
}

int bmrKcal(const Profile& p) {
  const float base = 10.0f * p.weightKg + 6.25f * p.heightCm - 5.0f * p.age;
  return static_cast<int>(std::lround(base + (p.sex == Sex::Male ? 5.0f : -161.0f)));
}

int weeksToTarget(const Profile& p) {
  const float diff = std::fabs(p.weightKg - p.targetKg);
  return static_cast<int>(std::ceil(diff / kSafeLossKgPerWeek - 1e-4f));
}

bool suggestLowImpact(const Profile& p) { return bmi(p.weightKg, p.heightCm) >= 30.0f; }

float kcal(float met, float kg, float seconds) { return met * 3.5f * kg / 200.0f * seconds / 60.0f; }

}  // namespace fit
