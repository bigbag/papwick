#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

#include "Golf.h"
#include "test_utils.h"

using namespace golf;

namespace {

constexpr uint32_t kFrameMs = 25;

Course flat() {
  Course c;
  c.green.add({Shape::Box, 100, 100, 366, 366, 20, 0});
  c.teeX = c.teeY = 233;
  c.holeX = c.holeY = 340;
  c.parMs = 10000;
  return c;
}

void roll(Game& g, float ax, float ay, uint32_t ms) {
  for (uint32_t t = 0; t < ms; t += kFrameMs) g.step(ax, ay, kFrameMs);
}

struct Point {
  float x, y;
};

// The way of the bot from the tee to the hole: a breadth-first search on a grid of 4 px. A cell is free when the ball
// fits with a margin of 5 px and no water is near. The windmill bar does not count, so the bot must pass it.
std::vector<Point> route(const Course& course) {
  constexpr int kCell = 4, kN = kSize / kCell;
  Course c = course;
  c.mill.speed = 0;
  auto center = [](int i) { return i * kCell + kCell / 2.0f; };
  auto open = [&](float x, float y) {
    if (wall(c, x, y, 0) > -kBallR - 5) return false;
    for (const Point& d : {Point{0, 0}, Point{5, 0}, Point{-5, 0}, Point{0, 5}, Point{0, -5}}) {
      if (ground(c, x + d.x, y + d.y) == Ground::Water) return false;
    }
    return true;
  };
  auto free = [&](int i, int j) { return open(center(i), center(j)); };
  auto cell = [](float v) { return static_cast<int>(v) / kCell; };
  const int start = cell(c.teeY) * kN + cell(c.teeX), goal = cell(c.holeY) * kN + cell(c.holeX);
  std::vector<int> from(kN * kN, -1);
  std::vector<int> queue = {start};
  from[start] = start;
  for (size_t q = 0; q < queue.size() && from[goal] < 0; ++q) {
    const int i = queue[q] % kN, j = queue[q] / kN;
    for (const auto& [di, dj] : {std::pair{1, 0}, std::pair{-1, 0}, std::pair{0, 1}, std::pair{0, -1}}) {
      const int ni = i + di, nj = j + dj, n = nj * kN + ni;
      if (ni < 0 || nj < 0 || ni >= kN || nj >= kN || from[n] >= 0 || (n != goal && !free(ni, nj))) continue;
      from[n] = queue[q];
      queue.push_back(n);
    }
  }
  std::vector<Point> path;
  if (from[goal] < 0) return path;
  for (int n = goal; n != start; n = from[n]) path.insert(path.begin(), {center(n % kN), center(n / kN)});
  // From each point, go straight to the farthest point of the path that a free line reaches.
  auto clear = [&](Point a, Point b) {
    const int steps = static_cast<int>(std::hypot(b.x - a.x, b.y - a.y) / 2) + 1;
    for (int k = 1; k < steps; ++k) {
      if (!open(a.x + (b.x - a.x) * k / steps, a.y + (b.y - a.y) * k / steps)) return false;
    }
    return true;
  };
  std::vector<Point> points;
  Point at = {c.teeX, c.teeY};
  for (size_t k = 0; k + 1 < path.size();) {
    size_t far = k;
    for (size_t n = path.size() - 1; n > k; --n) {
      if (clear(at, path[n])) {
        far = n;
        break;
      }
    }
    if (far == k) far = k + 1;
    at = path[far];
    points.push_back(at);
    k = far;
  }
  return points;
}

// Returns the time of the bot on the hole, or 0 when it does not sink the ball before the limit.
uint32_t play(const Course& c, std::vector<Point> path, uint32_t limitMs) {
  if (path.empty()) return 0;
  path.push_back({c.holeX, c.holeY});
  Game g;
  g.start(c);
  size_t next = 0;
  float lastX = g.x(), lastY = g.y();
  while (!g.sunk() && g.timeMs() < limitMs) {
    const float vx = (g.x() - lastX) * 1000 / kFrameMs, vy = (g.y() - lastY) * 1000 / kFrameMs;
    lastX = g.x();
    lastY = g.y();
    while (next + 1 < path.size() && std::hypot(path[next].x - g.x(), path[next].y - g.y()) < 8) ++next;
    const float dx = path[next].x - g.x(), dy = path[next].y - g.y(), d = std::hypot(dx, dy) + 0.001f;
    const float want = std::clamp(2.5f * d, 60.0f, 200.0f);
    // The tilt gain and the slope gain are equal, so a tilt of the slope value holds the ball on the slope.
    float gx, gy;
    slope(c, g.x(), g.y(), gx, gy);
    float ax = 6 * (want * dx / d - vx) / 900 + gx, ay = 6 * (want * dy / d - vy) / 900 + gy;
    const float a = std::hypot(ax, ay);
    if (a > 0.6f) {
      ax *= 0.6f / a;
      ay *= 0.6f / a;
    }
    g.step(ax, ay, kFrameMs);
  }
  return g.sunk() ? g.timeMs() : 0;
}

}  // namespace

int main() {
  TestUtils::TestRunner runner("Mini golf");

  const Course plain = flat();
  Game g;
  g.start(plain);
  roll(g, 0, 0, 1000);
  runner.expectTrue(g.x() == 233 && g.y() == 233 && g.timeMs() == 1000, "ball at rest stays on level ground");
  roll(g, 0.3f, 0, 500);
  runner.expectTrue(g.x() > 260 && std::fabs(g.y() - 233) < 0.01f, "tilt to the right rolls the ball right");
  g.start(plain);
  roll(g, 0, -0.3f, 500);
  runner.expectTrue(g.y() < 215 && std::fabs(g.x() - 233) < 0.01f, "tilt up rolls the ball up");

  g.start(plain);
  bool bounced = false, inside = true;
  for (int i = 0; i < 120; ++i) {
    bounced |= g.step(0.8f, 0.2f, kFrameMs) == Event::Bounce;
    inside &= wall(plain, g.x(), g.y(), 0) <= -kBallR + 0.5f;
  }
  runner.expectTrue(bounced, "fast ball bounces off the wall");
  runner.expectTrue(inside, "ball stays inside the walls");
  runner.expectTrue(g.x() > 350 && g.y() > 340, "steady tilt holds the ball in the corner");

  Course sandy = plain;
  sandy.sand.add({Shape::Circle, 233, 233, 0, 0, 100, 0});
  g.start(plain);
  roll(g, 0.3f, 0, 400);
  Game s;
  s.start(sandy);
  roll(s, 0.3f, 0, 400);
  runner.expectTrue(s.x() - 233 < (g.x() - 233) * 0.8f, "sand slows the ball");
  g.start(sandy);
  roll(g, 0.4f, 0, 400);
  roll(g, 0, 0, 800);
  const float stopped = g.x();
  roll(g, 0, 0, 200);
  runner.expectEq(stopped, g.x(), "sand stops a free ball");

  Course slanted = plain;
  slanted.bumps.add({Bump::Slope, 100, 233, 366, 233, 0, -20});
  g.start(slanted);
  roll(g, 0, 0, 500);
  runner.expectTrue(g.x() > 238, "ball rolls down a slope without tilt");
  runner.expectTrue(height(slanted, 120, 233) > height(slanted, 340, 233), "slope falls to the right");

  Course pond = plain;
  pond.water.add({Shape::Circle, 320, 233, 0, 0, 30, 0});
  g.start(pond);
  Event e = Event::None;
  for (int i = 0; i < 80 && e != Event::Water; ++i) e = g.step(0.4f, 0, kFrameMs);
  runner.expectTrue(e == Event::Water, "water event");
  runner.expectTrue(g.x() == 233 && g.y() == 233 && g.speed() == 0, "water puts the ball back on the tee");
  runner.expectTrue(g.timeMs() >= kWaterPenaltyMs, "water adds the penalty");
  Course bridged = pond;
  bridged.bridges.add({Shape::Capsule, 233, 233, 366, 233, 12, 0});
  runner.expectTrue(ground(bridged, 320, 233) == Ground::Green, "bridge covers the water");
  runner.expectTrue(ground(bridged, 320, 250) == Ground::Water, "water beside the bridge");

  Course cup = plain;
  cup.holeX = 238;
  cup.holeY = 233;
  g.start(cup);
  runner.expectTrue(g.step(0, 0, kFrameMs) == Event::Sunk && g.sunk(), "slow ball sinks");
  runner.expectTrue(g.x() == 238 && g.y() == 233, "sunk ball is in the hole");
  const uint32_t at = g.timeMs();
  runner.expectTrue(g.step(1, 0, 500) == Event::None && g.timeMs() == at, "sunk ball stops the clock");
  cup.teeX = 110;
  cup.holeX = 233;
  g.start(cup);
  bool sunk = false;
  for (int i = 0; i < 40 && g.x() < 280; ++i) sunk |= g.step(1, 0, kFrameMs) == Event::Sunk;
  runner.expectTrue(!sunk && g.x() >= 280, "fast ball rolls over the hole");
  cup.teeX = 233 - 2 * kHoleR;
  g.start(cup);
  roll(g, 0.08f, 0, 3000);
  runner.expectTrue(g.sunk(), "slow ball rolls into the hole");

  g.start(plain);
  g.step(0.3f, 0, 60000);
  runner.expectEq(uint32_t{100}, g.timeMs(), "long pause advances at most 100 ms");

  Course mill = course(7);
  mill.teeX = 263;
  mill.teeY = 256;
  g.start(mill);
  bool clear = true;
  for (int i = 0; i < 160; ++i) {
    g.step(0, 0, kFrameMs);
    clear &= wall(mill, g.x(), g.y(), g.millAngle()) <= -kBallR + 1;
  }
  runner.expectTrue(clear, "windmill bar does not go through the ball");
  runner.expectTrue(std::hypot(g.x() - 263, g.y() - 256) > 10, "windmill bar pushes the ball");
  mill.teeX = 277.9f;
  mill.teeY = 280;
  Course still = mill;
  still.mill.speed = 0;
  g.start(mill);
  s.start(still);
  for (int i = 0; i < 10; ++i) {
    g.step(0.2f, 0, 4);
    s.step(0.2f, 0, 4);
  }
  runner.expectNear(s.speed(), g.speed(), 0.5, "fixed wall near the windmill bounces as without the windmill");

  runner.expectEq(3, stars(9000, 9000), "par gives 3 stars");
  runner.expectEq(2, stars(14400, 9000), "1.6 x par gives 2 stars");
  runner.expectEq(1, stars(14401, 9000), "slower gives 1 star");

  Progress p;
  runner.expectTrue(p.valid() && p.unlocked == 1, "new progress is valid");
  runner.expectTrue(p.record(0, 8000) && p.unlocked == 2 && p.best[0] == 8000, "first time unlocks the next hole");
  runner.expectFalse(p.record(0, 9000), "slower time is not a record");
  runner.expectEq(uint32_t{8000}, p.best[0], "slower time keeps the best");
  runner.expectTrue(p.record(0, 7000) && p.best[0] == 7000, "faster time is a record");
  runner.expectFalse(p.record(kHoles, 1000) || p.record(-1, 1000), "bad hole is ignored");
  p.unlocked = kHoles;
  runner.expectTrue(p.record(kHoles - 1, 1000) && p.unlocked == kHoles + 1 && p.selectable() == kHoles,
                    "last hole opens a hole that a later version adds");
  runner.expectTrue(p.valid(), "full progress is valid");
  Progress bad;
  bad.version = 2;
  runner.expectFalse(bad.valid(), "unknown version is invalid");
  bad = {};
  bad.unlocked = 0;
  runner.expectFalse(bad.valid(), "no unlocked hole is invalid");
  bad.unlocked = kHoles + 2;
  runner.expectFalse(bad.valid(), "too many unlocked holes is invalid");
  bad = {};
  bad.best[3] = 5000;
  runner.expectFalse(bad.valid(), "time of a locked hole is invalid");

  runner.expectTrue(&course(-1) == &course(0) && &course(99) == &course(kHoles - 1), "course index is clamped");
  runner.expectEq(uint16_t{0x00F8}, pixel({255, 0, 0}), "pixel is RGB565 with the high byte first");

  for (int i = 0; i < kHoles; ++i) {
    const Course& c = course(i);
    const std::string name = "hole " + std::to_string(i + 1) + ": ";
    float reach = 0, top = kSize;
    for (int y = 0; y < kSize; ++y) {
      for (int x = 0; x < kSize; ++x) {
        if (wall(c, x, y, 0) >= 0) continue;
        reach = std::max(reach, std::hypot(x - kSize / 2.0f, y - kSize / 2.0f));
        top = std::min<float>(top, y);
      }
    }
    runner.expectTrue(reach <= kFieldR && top >= kFieldTop,
                      name + "green fits the screen, reach " + std::to_string(reach) + ", top " + std::to_string(top));
    runner.expectTrue(wall(c, c.teeX, c.teeY, 0) < -kBallR && ground(c, c.teeX, c.teeY) == Ground::Green,
                      name + "tee is on the green");
    runner.expectTrue(wall(c, c.holeX, c.holeY, 0) < -kHoleR && ground(c, c.holeX, c.holeY) != Ground::Water,
                      name + "hole is on the green");
    for (int k = 0; k < c.decos.count; ++k) {
      const Deco& d = c.decos.items[k];
      runner.expectTrue(wall(c, d.x, d.y, 0) >= d.r + 6 && std::hypot(d.x - kSize / 2.0f, d.y - kSize / 2.0f) < 229,
                        name + "decoration " + std::to_string(k) + " is on the screen and off the course");
    }
    runner.expectTrue(c.parMs > 0, name + "par is set");
    Game still;
    still.start(c);
    bool dry = true;
    for (uint32_t ms = 0; ms < 60000; ms += kFrameMs) dry = dry && still.step(0, 0, kFrameMs) != Event::Water;
    runner.expectTrue(dry, name + "a flat device does not roll the ball into the water");
    if (std::getenv("GOLF_BOT")) std::cout << name << "bot " << play(c, route(c), 60000) << " ms\n";
    const uint32_t ms = play(c, route(c), c.parMs);
    runner.expectTrue(ms > 0, name + "bot sinks the ball within par");
  }

  std::vector<uint16_t> picture(kSize * kSize, 0xFFFF);
  runner.expectTrue(paint(course(0), picture.data()), "paint succeeds");
  runner.expectTrue(picture[0] == 0 && picture[kSize * kSize - 1] == 0, "paint leaves the corners black");
  runner.expectTrue(picture[233 * kSize + 233] != 0, "paint draws the center");
  return runner.exitCode();
}
