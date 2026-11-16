#include "Golf.h"

#include <algorithm>
#include <cmath>

namespace golf {

namespace {

constexpr float kPi = 3.14159265f;
constexpr uint32_t kStepMs = 4;
constexpr float kDt = kStepMs / 1000.0f;
// Accelerations in px/s^2: for 1 g of tilt and for a slope of 1.
constexpr float kTiltGain = 900, kSlopeGain = 900;
// The rolling friction in px/s^2 and the air drag in 1/s.
constexpr float kRollGreen = 40, kRollSand = 260, kDrag = 0.4f;
constexpr float kBounce = 0.55f, kMaxSpeed = 600;
// A slow ball near the hole center falls in. A fast ball rolls over the cup with a pull to the center.
constexpr float kSinkSpeed = 170, kPull = 1300;

float segment(float x, float y, float x0, float y0, float x1, float y1, float& t) {
  const float dx = x1 - x0, dy = y1 - y0;
  const float len2 = dx * dx + dy * dy;
  t = len2 > 0 ? std::clamp(((x - x0) * dx + (y - y0) * dy) / len2, 0.0f, 1.0f) : 0;
  return std::hypot(x - x0 - t * dx, y - y0 - t * dy);
}

float spiral(const Shape& s, float x, float y) {
  const float dx = x - s.x0, dy = y - s.y0;
  const float rho = std::hypot(dx, dy);
  float turn = std::atan2(dy, dx) / (2 * kPi);
  if (turn < 0) turn += 1;
  // Both ends of the arm are on the +x axis. The distance to the end points gives them round caps.
  float best = std::min(std::hypot(dx - s.x1, dy), std::hypot(dx - s.x1 - s.y1 * s.s, dy));
  for (int k = 0; k < static_cast<int>(s.s); ++k) {
    best = std::min(best, std::fabs(rho - (s.x1 + s.y1 * (turn + k))));
  }
  return best - s.r;
}

float bumpHeight(const Bump& b, float x, float y) {
  float t;
  switch (b.kind) {
    case Bump::Hill: {
      const float d2 = (x - b.x0) * (x - b.x0) + (y - b.y0) * (y - b.y0);
      return b.h * std::exp(-d2 / (b.r * b.r));
    }
    case Bump::Ridge: {
      const float d = segment(x, y, b.x0, b.y0, b.x1, b.y1, t);
      return b.h * std::exp(-d * d / (b.r * b.r));
    }
    case Bump::Ring: {
      const float d = std::hypot(x - b.x0, y - b.y0) - b.x1;
      return b.h * std::exp(-d * d / (b.r * b.r));
    }
    case Bump::Ripples: {
      const float d = segment(x, y, b.x0, b.y0, b.x1, b.y1, t);
      const float along = t * std::hypot(b.x1 - b.x0, b.y1 - b.y0);
      // The waves fade at both ends and to the sides, so they join the flat ground.
      const float fade = std::sin(t * kPi) * std::exp(-d * d / (90.0f * 90.0f));
      return b.h * fade * std::sin(2 * kPi * along / b.r);
    }
    case Bump::Slope:
      segment(x, y, b.x0, b.y0, b.x1, b.y1, t);
      return b.h * t * t * (3 - 2 * t);
  }
  return 0;
}

Shape bar(const Mill& m, float angle) {
  const float ux = std::cos(angle) * m.length, uy = std::sin(angle) * m.length;
  return {Shape::Capsule, m.x - ux, m.y - uy, m.x + ux, m.y + uy, 4, 0};
}

bool inAny(const Shape* s, int n, float x, float y) {
  for (int i = 0; i < n; ++i) {
    if (distance(s[i], x, y) < 0) return true;
  }
  return false;
}

}  // namespace

float distance(const Shape& s, float x, float y) {
  float t;
  switch (s.kind) {
    case Shape::Circle:
      return std::hypot(x - s.x0, y - s.y0) - s.r;
    case Shape::Capsule:
      return segment(x, y, s.x0, s.y0, s.x1, s.y1, t) - s.r;
    case Shape::Box: {
      const float hx = (s.x1 - s.x0) / 2 - s.r, hy = (s.y1 - s.y0) / 2 - s.r;
      const float qx = std::fabs(x - (s.x0 + s.x1) / 2) - hx, qy = std::fabs(y - (s.y0 + s.y1) / 2) - hy;
      return std::hypot(std::max(qx, 0.0f), std::max(qy, 0.0f)) + std::min(std::max(qx, qy), 0.0f) - s.r;
    }
    case Shape::Spiral:
      return spiral(s, x, y);
  }
  return 1e9f;
}

float height(const Course& c, float x, float y) {
  float h = 0;
  for (int i = 0; i < c.bumps.count; ++i) h += bumpHeight(c.bumps.items[i], x, y);
  return h;
}

void slope(const Course& c, float x, float y, float& gx, float& gy) {
  constexpr float e = 1;
  gx = (height(c, x + e, y) - height(c, x - e, y)) / (2 * e);
  gy = (height(c, x, y + e) - height(c, x, y - e)) / (2 * e);
}

float wall(const Course& c, float x, float y, float angle) {
  float d = 1e9f;
  for (int i = 0; i < c.green.count; ++i) d = std::min(d, distance(c.green.items[i], x, y));
  for (int i = 0; i < c.blocks.count; ++i) d = std::max(d, -distance(c.blocks.items[i], x, y));
  if (c.mill.speed != 0) d = std::max(d, -distance(bar(c.mill, angle), x, y));
  return d;
}

Ground ground(const Course& c, float x, float y) {
  const bool bridge = inAny(c.bridges.items, c.bridges.count, x, y);
  if (!bridge && inAny(c.water.items, c.water.count, x, y)) return Ground::Water;
  return inAny(c.sand.items, c.sand.count, x, y) ? Ground::Sand : Ground::Green;
}

void Game::start(const Course& c) {
  c_ = &c;
  x_ = c.teeX;
  y_ = c.teeY;
  vx_ = vy_ = angle_ = 0;
  timeMs_ = penaltyMs_ = restMs_ = 0;
  sunk_ = false;
}

float Game::speed() const { return std::hypot(vx_, vy_); }

Event Game::step(float ax, float ay, uint32_t ms) {
  if (!c_ || sunk_) return Event::None;
  // A long pause, for example a slow frame, must not throw the ball through a wall.
  restMs_ += std::min<uint32_t>(ms, 100);
  Event last = Event::None;
  while (restMs_ >= kStepMs && !sunk_) {
    restMs_ -= kStepMs;
    timeMs_ += kStepMs;
    const Event e = tick(ax, ay);
    if (e != Event::None) last = e;
    // The frame ends with the ball on the tee, so the app can show it there.
    if (e == Event::Water) {
      restMs_ = 0;
      break;
    }
  }
  return last;
}

Event Game::tick(float ax, float ay) {
  const Course& c = *c_;
  angle_ += c.mill.speed * kDt;
  if (angle_ > 2 * kPi) angle_ -= 2 * kPi;
  if (angle_ < 0) angle_ += 2 * kPi;

  float gx, gy;
  slope(c, x_, y_, gx, gy);
  float fx = kTiltGain * ax - kSlopeGain * gx, fy = kTiltGain * ay - kSlopeGain * gy;
  const float hx = c.holeX - x_, hy = c.holeY - y_;
  const float toHole = std::hypot(hx, hy);
  if (toHole < kHoleR && toHole > 0.01f) {
    if (toHole < kHoleR - 3 && speed() < kSinkSpeed) {
      sunk_ = true;
      x_ = c.holeX;
      y_ = c.holeY;
      vx_ = vy_ = 0;
      return Event::Sunk;
    }
    fx += kPull * hx / toHole;
    fy += kPull * hy / toHole;
  }
  vx_ += fx * kDt;
  vy_ += fy * kDt;

  const float v = speed();
  const float roll = ground(c, x_, y_) == Ground::Sand ? kRollSand : kRollGreen;
  const float loss = (roll + kDrag * v) * kDt;
  const float keep = v > loss ? std::min(v - loss, kMaxSpeed) / v : 0;
  vx_ *= keep;
  vy_ *= keep;
  x_ += vx_ * kDt;
  y_ += vy_ * kDt;

  Event event = Event::None;
  const float d = wall(c, x_, y_, angle_);
  if (d > -kBallR) {
    // The bar is the wall in contact when it gives the wall distance. The margin allows for rounding.
    const bool onBar = c.mill.speed != 0 && -distance(bar(c.mill, angle_), x_, y_) >= d - 0.01f;
    constexpr float e = 0.5f;
    float nx = wall(c, x_ + e, y_, angle_) - wall(c, x_ - e, y_, angle_);
    float ny = wall(c, x_, y_ + e, angle_) - wall(c, x_, y_ - e, angle_);
    const float n = std::hypot(nx, ny);
    if (n > 0) {
      nx /= n;
      ny /= n;
      x_ -= nx * (d + kBallR);
      y_ -= ny * (d + kBallR);
      // The windmill bar moves. The bounce uses the speed of the ball relative to the bar.
      float bx = 0, by = 0;
      if (onBar) {
        bx = -c.mill.speed * (y_ - c.mill.y);
        by = c.mill.speed * (x_ - c.mill.x);
      }
      const float rx = vx_ - bx, ry = vy_ - by;
      const float vn = rx * nx + ry * ny;
      if (vn > 0) {
        vx_ = rx - (1 + kBounce) * vn * nx + bx;
        vy_ = ry - (1 + kBounce) * vn * ny + by;
        if (vn > 40) event = Event::Bounce;
      }
    }
  }

  if (ground(c, x_, y_) == Ground::Water) {
    x_ = c.teeX;
    y_ = c.teeY;
    vx_ = vy_ = 0;
    penaltyMs_ += kWaterPenaltyMs;
    return Event::Water;
  }
  return event;
}

int stars(uint32_t timeMs, uint32_t parMs) {
  if (timeMs <= parMs) return 3;
  return timeMs <= parMs * 8 / 5 ? 2 : 1;
}

bool Progress::valid() const {
  if (version != Progress{}.version || unlocked < 1 || unlocked > kHoles + 1) return false;
  for (int i = 0; i < kMaxHoles; ++i) {
    if (best[i] != 0 && i >= unlocked) return false;
  }
  return true;
}

bool Progress::record(int hole, uint32_t ms) {
  if (hole < 0 || hole >= kHoles) return false;
  unlocked = static_cast<uint8_t>(std::max<int>(unlocked, std::min(hole + 2, kMaxHoles)));
  if (best[hole] != 0 && best[hole] <= ms) return false;
  best[hole] = ms;
  return true;
}

uint16_t pixel(Rgb c) {
  const uint16_t v = static_cast<uint16_t>((c.r >> 3) << 11 | (c.g >> 2) << 5 | c.b >> 3);
  return static_cast<uint16_t>(v >> 8 | v << 8);
}

}  // namespace golf
