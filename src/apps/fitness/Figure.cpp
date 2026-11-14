#include "Figure.h"

#include <Exercise.h>

#include <cmath>
#include <cstring>

#include "Ui.h"

namespace figure {

namespace {

constexpr int kMaxPoses = 6;
constexpr float kTorso = 36, kNeckToHead = 12, kHeadR = 9, kUpperArm = 20, kForearm = 20, kThigh = 29, kShin = 29;
constexpr float kLineR = 4;
constexpr int kGroundY = kH - 4;

// Angles in degrees: 0 points down, 90 points forward (right), 180 points up. head 0 means "same as torso".
struct Pose {
  int16_t hipX, torso, armFarU, armFarL, armNearU, armNearL, legFarU, legFarL, legNearU, legNearL, head;
};

enum class Prop : uint8_t { None, Wall, Chair };

struct Anim {
  uint16_t periodMs;
  bool front;
  Prop prop;
  int16_t propX, propY;
  uint8_t count;
  Pose poses[kMaxPoses];
};

// One entry per fit::Ex, in the same order. scripts/preview_figures.py parses this table.
const Anim kAnims[] = {
    // MarchInPlace
    {1000,
     false,
     Prop::None,
     0,
     0,
     2,
     {{90, 180, 35, 110, -30, -15, 0, 0, 80, 0, 0}, {90, 180, -30, -15, 35, 110, 80, 0, 0, 0, 0}}},
    // ArmCircles
    {800,
     true,
     Prop::None,
     0,
     0,
     2,
     {{90, 180, -102, -102, 102, 102, -8, -8, 8, 8, 0}, {90, 180, -78, -78, 78, 78, -8, -8, 8, 8, 0}}},
    // JumpingJacks
    {700,
     true,
     Prop::None,
     0,
     0,
     2,
     {{90, 180, -8, -5, 8, 5, -4, -4, 4, 4, 0}, {90, 180, -150, -170, 150, 170, -22, -22, 22, 22, 0}}},
    // StepJacks
    {1600,
     true,
     Prop::None,
     0,
     0,
     4,
     {{90, 180, -8, -5, 8, 5, -4, -4, 4, 4, 0},
      {90, 180, -150, -170, 150, 170, -4, -4, 25, 25, 0},
      {90, 180, -8, -5, 8, 5, -4, -4, 4, 4, 0},
      {90, 180, -150, -170, 150, 170, -25, -25, 4, 4, 0}}},
    // Squats
    {1800,
     false,
     Prop::None,
     0,
     0,
     2,
     {{95, 180, 5, 10, 10, 15, -3, 0, 3, 0, 0}, {72, 145, 75, 80, 78, 85, 80, -15, 82, -15, 0}}},
    // KneePushUps
    {1600,
     false,
     Prop::None,
     0,
     0,
     2,
     {{90, 128, 0, 0, 0, 0, -52, -110, -52, -110, 0}, {93, 106, -50, 74, -50, 74, -74, -110, -74, -110, 0}}},
    // PushUps
    {1600,
     false,
     Prop::None,
     0,
     0,
     2,
     {{88, 115, 0, 0, 0, 0, -65, -65, -65, -65, 0}, {93, 101, -50, 74, -50, 74, -79, -79, -79, -79, 0}}},
    // Lunges
    {2400,
     false,
     Prop::None,
     0,
     0,
     4,
     {{90, 180, 0, 0, 0, 0, -3, 0, 3, 0, 0},
      {90, 180, 0, 0, 0, 0, -20, -80, 85, 0, 0},
      {90, 180, 0, 0, 0, 0, -3, 0, 3, 0, 0},
      {90, 180, 0, 0, 0, 0, 85, 0, -20, -80, 0}}},
    // Plank
    {2000, false, Prop::None, 0, 0, 1, {{90, 102, 0, 90, 0, 90, -78, -78, -78, -78, 0}}},
    // Crunches
    {1600,
     false,
     Prop::None,
     0,
     0,
     2,
     {{85, 90, -90, -90, -90, -90, -140, -40, -140, -40, 0},
      {85, 120, -100, -100, -100, -100, -140, -40, -140, -40, 0}}},
    // MountainClimbers
    {600,
     false,
     Prop::None,
     0,
     0,
     2,
     {{88, 115, 0, 0, 0, 0, -65, -65, 60, -100, 0}, {88, 115, 0, 0, 0, 0, 60, -100, -65, -65, 0}}},
    // KneeDrives
    {1000,
     false,
     Prop::None,
     0,
     0,
     2,
     {{90, 180, 165, 175, 165, 175, -3, 0, 3, 0, 0}, {90, 180, 30, 60, 30, 60, -3, 0, 110, 0, 0}}},
    // HighKnees
    {500,
     false,
     Prop::None,
     0,
     0,
     2,
     {{90, 180, 50, 130, -40, -10, 5, -5, 100, -5, 0}, {90, 180, -40, -10, 50, 130, 100, -5, 5, -5, 0}}},
    // HighMarch
    {900,
     false,
     Prop::None,
     0,
     0,
     2,
     {{90, 180, 40, 120, -35, -15, 0, 0, 95, 0, 0}, {90, 180, -35, -15, 40, 120, 95, 0, 0, 0, 0}}},
    // Burpees
    {3000,
     false,
     Prop::None,
     0,
     0,
     5,
     {{95, 180, 5, 10, 10, 15, -3, 0, 3, 0, 0},
      {80, 120, 0, 0, 0, 0, 100, -30, 100, -30, 0},
      {78, 115, 0, 0, 0, 0, -65, -65, -65, -65, 0},
      {80, 120, 0, 0, 0, 0, 100, -30, 100, -30, 0},
      {95, 180, 165, 175, 165, 175, -3, 0, 3, 0, 0}}},
    // StepBackBurpees
    {4200,
     false,
     Prop::None,
     0,
     0,
     6,
     {{95, 180, 5, 10, 10, 15, -3, 0, 3, 0, 0},
      {80, 120, 0, 0, 0, 0, 100, -30, 100, -30, 0},
      {79, 118, 0, 0, 0, 0, 100, -30, -70, -70, 0},
      {78, 115, 0, 0, 0, 0, -65, -65, -65, -65, 0},
      {79, 118, 0, 0, 0, 0, 100, -30, -70, -70, 0},
      {80, 120, 0, 0, 0, 0, 100, -30, 100, -30, 0}}},
    // GluteBridge
    {1600,
     false,
     Prop::None,
     0,
     0,
     2,
     {{85, 90, -90, -90, -90, -90, -140, -40, -140, -40, 0}, {89, 63, -90, -90, -90, -90, -117, -10, -117, -10, 90}}},
    // WallSit
    {2000, false, Prop::Wall, 58, 0, 1, {{70, 180, 90, 90, 90, 90, 90, 0, 90, 0, 0}}},
    // LegRaises
    {1800,
     false,
     Prop::None,
     0,
     0,
     2,
     {{80, 90, -90, -90, -90, -90, -95, -95, -95, -95, 0}, {80, 90, -90, -90, -90, -90, -180, -180, -180, -180, 0}}},
    // Superman
    {2000,
     false,
     Prop::None,
     0,
     0,
     2,
     {{80, 90, 88, 88, 88, 88, -88, -88, -88, -88, 0}, {80, 100, 110, 110, 110, 110, -108, -108, -108, -108, 0}}},
    // TricepsDips
    {1600,
     false,
     Prop::Chair,
     36,
     113,
     2,
     {{80, 175, -15, -15, -15, -15, 75, 15, 75, 15, 0}, {82, 175, -70, 12, -70, 12, 85, 35, 85, 35, 0}}},
    // ShoulderTaps
    {1600,
     false,
     Prop::None,
     0,
     0,
     4,
     {{88, 115, 0, 0, 0, 0, -65, -65, -65, -65, 0},
      {88, 115, 0, 0, 30, -150, -65, -65, -65, -65, 0},
      {88, 115, 0, 0, 0, 0, -65, -65, -65, -65, 0},
      {88, 115, 30, -150, 0, 0, -65, -65, -65, -65, 0}}},
    // QuadStretch
    {2000, false, Prop::None, 0, 0, 1, {{90, 180, 80, 85, -10, -20, 0, 0, -5, -165, 0}}},
    // HamstringStretch
    {2000, false, Prop::None, 0, 0, 1, {{55, 115, 60, 60, 60, 60, 90, 90, 90, 90, 0}}},
    // CobraStretch
    {2000, false, Prop::None, 0, 0, 1, {{80, 115, -35, 35, -35, 35, -90, -90, -90, -90, 160}}},
};
static_assert(sizeof(kAnims) / sizeof(kAnims[0]) == fit::kExerciseCount, "one animation per exercise");

struct Vec {
  float x, y;
};

Vec step(Vec from, float deg, float len) {
  const float r = deg * static_cast<float>(M_PI) / 180.0f;
  return {from.x + len * std::sin(r), from.y + len * std::cos(r)};
}

float mixAngle(float a, float b, float f) {
  float d = std::fmod(b - a, 360.0f);
  if (d > 180.0f) d -= 360.0f;
  if (d < -180.0f) d += 360.0f;
  return a + d * f;
}

}  // namespace

void draw(LovyanGFX& g, int x, int y, uint8_t exercise, uint32_t ms) {
  if (exercise >= fit::kExerciseCount) return;
  const Anim& a = kAnims[exercise];
  const float t = static_cast<float>(ms % a.periodMs) / a.periodMs * a.count;
  const int i = static_cast<int>(t) % a.count;
  float f = t - static_cast<int>(t);
  f = f * f * (3.0f - 2.0f * f);
  static_assert(sizeof(Pose) == 11 * sizeof(int16_t), "Pose holds 11 values");
  int16_t p0[11], p1[11];
  memcpy(p0, &a.poses[i], sizeof(p0));
  memcpy(p1, &a.poses[(i + 1) % a.count], sizeof(p1));
  float v[11];
  v[0] = p0[0] + (p1[0] - p0[0]) * f;
  for (int k = 1; k < 11; ++k) {
    const float h0 = (k == 10 && p0[k] == 0) ? p0[1] : p0[k];
    const float h1 = (k == 10 && p1[k] == 0) ? p1[1] : p1[k];
    v[k] = mixAngle(h0, h1, f);
  }

  const Vec hip{v[0], 0};
  const Vec neck = step(hip, v[1], kTorso);
  const Vec head = step(neck, v[10], kNeckToHead);
  const Vec elbowF = step(neck, v[2], kUpperArm), handF = step(elbowF, v[3], kForearm);
  const Vec elbowN = step(neck, v[4], kUpperArm), handN = step(elbowN, v[5], kForearm);
  const Vec kneeF = step(hip, v[6], kThigh), footF = step(kneeF, v[7], kShin);
  const Vec kneeN = step(hip, v[8], kThigh), footN = step(kneeN, v[9], kShin);

  float low = head.y + kHeadR;
  for (const Vec& j : {hip, neck, elbowF, handF, elbowN, handN, kneeF, footF, kneeN, footN}) {
    low = std::fmax(low, j.y + kLineR);
  }
  const float dy = y + kGroundY - low;
  auto line = [&](Vec p, Vec q, uint16_t c) { g.drawWideLine(x + p.x, dy + p.y, x + q.x, dy + q.y, kLineR, c); };

  const uint16_t near = TFT_WHITE, far = a.front ? TFT_WHITE : ui::kGrey;
  g.fillRect(x + 6, y + kGroundY, kW - 12, 3, ui::kDark);
  if (a.prop == Prop::Wall) g.fillRect(x + a.propX - 4, y + 4, 4, kGroundY - 4, ui::kDark);
  if (a.prop == Prop::Chair) {
    const int sx = x + a.propX, sy = y + a.propY;
    g.fillRect(sx, sy, 40, 5, ui::kDark);
    g.fillRect(sx + 2, sy, 4, y + kGroundY - sy, ui::kDark);
    g.fillRect(sx + 34, sy, 4, y + kGroundY - sy, ui::kDark);
  }
  line(neck, elbowF, far);
  line(elbowF, handF, far);
  line(hip, kneeF, far);
  line(kneeF, footF, far);
  line(hip, neck, near);
  line(hip, kneeN, near);
  line(kneeN, footN, near);
  line(neck, elbowN, near);
  line(elbowN, handN, near);
  g.fillSmoothCircle(x + head.x, dy + head.y, kHeadR, near);
}

}  // namespace figure
