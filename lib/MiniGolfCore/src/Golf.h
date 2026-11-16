#pragma once

#include <algorithm>
#include <cstdint>

// Mini golf without hardware: the holes, the ground, the ball physics, the score and the background picture.
// A course unit is one screen pixel. The screen is round: kSize x kSize pixels with the center at kSize / 2.
namespace golf {

constexpr int kSize = 466;
constexpr int kHoles = 27;
// The save holds this many holes. Then more holes later keep the saved progress.
constexpr int kMaxHoles = 40;
static_assert(kHoles <= kMaxHoles, "the save holds all holes");
constexpr float kBallR = 7, kHoleR = 10;
// The green with its curb stays this near the screen center, so the edge fade does not cover it.
constexpr float kFieldR = 208;
// The band above this line holds the hole number and the time.
constexpr float kFieldTop = 50;
constexpr uint32_t kWaterPenaltyMs = 5000;

// Circle: center (x0, y0), radius r.
// Capsule: the segment from (x0, y0) to (x1, y1), radius r.
// Box: the corners (x0, y0) and (x1, y1), corner radius r.
// Spiral: center (x0, y0). The arm starts at radius x1 and moves out by y1 in each turn, for s turns. Half width r.
struct Shape {
  enum Kind : uint8_t { Circle, Capsule, Box, Spiral };
  Kind kind;
  float x0, y0, x1, y1, r, s;
};

// The signed distance in pixels. It is negative inside the shape.
float distance(const Shape& s, float x, float y);

// Hill: a round hill at (x0, y0) with radius r and height h. A negative h gives a dip.
// Ridge: a long hill along the segment from (x0, y0) to (x1, y1), width r.
// Ring: a ring hill around (x0, y0) with radius x1, width r.
// Ripples: waves along the segment from (x0, y0) to (x1, y1), wavelength r, height h. They fade at the ends.
// Slope: the ground rises by h from (x0, y0) to (x1, y1). It is flat before and after the segment.
struct Bump {
  enum Kind : uint8_t { Hill, Ridge, Ring, Ripples, Slope };
  Kind kind;
  float x0, y0, x1, y1, r, h;
};

// Pictures outside the course. They do not touch the ball.
struct Deco {
  enum Kind : uint8_t { Tree, Flowers, Stone, Lantern };
  Kind kind;
  float x, y, r;
};

// A bar that turns around (x, y). length is the half length. speed in rad/s. Speed 0: the hole has no windmill.
struct Mill {
  float x = 0, y = 0, length = 0, speed = 0;
};

enum class Theme : uint8_t { Meadow, Coast, Evening, Autumn, Winter };
enum class Ground : uint8_t { Green, Sand, Water };

template <typename T, int N>
struct List {
  T items[N] = {};
  int count = 0;
  // The courses are constant data, so an item over the capacity is a build error.
  constexpr void add(const T& v) { items[count++] = v; }
};

struct Course {
  // The area inside the outer walls: the union of the shapes.
  List<Shape, 8> green;
  // Walls in the green.
  List<Shape, 4> blocks;
  List<Shape, 4> sand;
  List<Shape, 4> water;
  // Ground over the water.
  List<Shape, 2> bridges;
  List<Bump, 8> bumps;
  List<Deco, 28> decos;
  Mill mill;
  float teeX = 0, teeY = 0, holeX = 0, holeY = 0;
  uint32_t parMs = 0;
  Theme theme = Theme::Meadow;
};

const Course& course(int index);
float height(const Course& c, float x, float y);
// The slope of the ground: the height change for each pixel along x and y.
void slope(const Course& c, float x, float y, float& gx, float& gy);
// The signed distance to the walls: negative in the play area. angle: the windmill angle.
float wall(const Course& c, float x, float y, float angle);
Ground ground(const Course& c, float x, float y);

enum class Event : uint8_t { None, Sunk, Water, Bounce };

class Game {
 public:
  void start(const Course& c);
  // ax, ay: the tilt in g along the screen axes, x to the right and y down. ms: the time since the last call.
  // Returns the last event of the period.
  Event step(float ax, float ay, uint32_t ms);
  float x() const { return x_; }
  float y() const { return y_; }
  float speed() const;
  float millAngle() const { return angle_; }
  // The play time with the penalties.
  uint32_t timeMs() const { return timeMs_ + penaltyMs_; }
  bool sunk() const { return sunk_; }

 private:
  Event tick(float ax, float ay);

  const Course* c_ = nullptr;
  float x_ = 0, y_ = 0, vx_ = 0, vy_ = 0, angle_ = 0;
  uint32_t timeMs_ = 0, penaltyMs_ = 0, restMs_ = 0;
  bool sunk_ = false;
};

// A time at par or faster gets 3 stars. A time up to 1.6 x par gets 2 stars. A slower time gets 1 star.
int stars(uint32_t timeMs, uint32_t parMs);

// The app saves this struct in NVS.
struct Progress {
  uint8_t version = 1;
  // The count of open holes. After the last hole it is kHoles + 1, so a hole that a later version adds is open.
  uint8_t unlocked = 1;
  // The best time of each hole in ms. 0: not played.
  uint32_t best[kMaxHoles] = {};

  bool valid() const;
  // The count of holes that the player can select.
  int selectable() const { return std::min<int>(unlocked, kHoles); }
  // Saves the time and unlocks the next hole. Returns true for a new best time.
  bool record(int hole, uint32_t ms);
};

struct Rgb {
  uint8_t r, g, b;
};

struct Palette {
  Rgb grassA, grassB, rough, curb, sand, waterDeep, waterShallow, wood, canopy, stone, light;
  Rgb flowers[3];
};

const Palette& palette(Theme t);
// RGB565 with the high byte first, as the display sprites hold it.
uint16_t pixel(Rgb c);

// Writes the background of the hole without the ball and the windmill bar: kSize x kSize pixels in the format of
// pixel(). Returns false when it cannot get memory.
bool paint(const Course& c, uint16_t* out);

}  // namespace golf
