#pragma once

#include <cstdint>

namespace fit {

enum class Sex : uint8_t { Male, Female };
enum class Level : uint8_t { Beginner, Intermediate, Advanced };
enum class BmiClass : uint8_t { Underweight, Healthy, Overweight, Obese };

constexpr uint8_t kProfileVersion = 1;
constexpr float kSafeLossKgPerWeek = 0.5f;

struct Profile {
  uint8_t version = kProfileVersion;
  Sex sex = Sex::Male;
  uint8_t age = 30;
  uint8_t heightCm = 175;
  float weightKg = 85.0f;
  float targetKg = 80.0f;
  Level level = Level::Beginner;
  bool lowImpact = false;
};

constexpr uint8_t kWeightLogVersion = 1;
constexpr int kMaxWeights = 30;

struct WeightEntry {
  uint32_t time;
  float kg;
};

struct WeightLog {
  uint8_t version = kWeightLogVersion;
  uint8_t count = 0;
  WeightEntry entries[kMaxWeights] = {};
};

bool isValid(const Profile& p);
bool isValid(const WeightLog& log);
// Appends an entry. When the log is full, the oldest entry is removed.
void addWeight(WeightLog& log, uint32_t time, float kg);
float bmi(float kg, int heightCm);
BmiClass bmiClass(float bmiValue);
const char* bmiClassName(BmiClass c);
const char* levelName(Level l);
// Mifflin-St Jeor resting energy, kcal per day.
int bmrKcal(const Profile& p);
// Weeks to reach the target weight at kSafeLossKgPerWeek, rounded up.
int weeksToTarget(const Profile& p);
bool suggestLowImpact(const Profile& p);
// Calories for an activity: MET x 3.5 x kg / 200 per minute.
float kcal(float met, float kg, float seconds);

}  // namespace fit
