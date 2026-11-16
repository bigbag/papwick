#include <algorithm>
#include <cmath>
#include <new>

#include "Golf.h"

namespace golf {

namespace {

// Heights and shades are on a grid of 4 px. Each pixel interpolates them. Edges use the exact distance per pixel.
constexpr int kGrid = 4;
constexpr int kCells = kSize / kGrid + 2;
constexpr float kCurb = 6;
// The height step between two contour lines.
constexpr float kContour = 5;
// The shading shows the slopes this much steeper than the physics, so the player can see them.
constexpr float kRelief = 2.2f;
// The light comes from the upper left.
constexpr float kLx = -0.55f, kLy = -0.55f, kLz = 0.63f;
// Black beyond the glass edge. A fade before it.
constexpr float kEdge = 229, kFade = 16;

const Palette kPalettes[] = {
    // Meadow
    {{78, 164, 84},
     {68, 150, 74},
     {12, 30, 16},
     {232, 226, 206},
     {226, 200, 142},
     {22, 74, 140},
     {70, 168, 196},
     {150, 104, 62},
     {42, 112, 52},
     {132, 130, 124},
     {255, 250, 230},
     {{238, 92, 122}, {250, 214, 82}, {244, 244, 240}}},
    // Coast
    {{104, 176, 92},
     {94, 164, 82},
     {34, 30, 20},
     {242, 238, 226},
     {238, 214, 160},
     {12, 92, 152},
     {64, 196, 206},
     {162, 118, 74},
     {52, 128, 70},
     {176, 164, 146},
     {255, 252, 236},
     {{244, 196, 176}, {250, 236, 210}, {236, 140, 120}}},
    // Evening
    {{52, 124, 84},
     {46, 112, 76},
     {6, 10, 22},
     {186, 186, 206},
     {196, 174, 132},
     {14, 40, 92},
     {44, 96, 148},
     {120, 84, 56},
     {30, 76, 56},
     {98, 100, 112},
     {255, 206, 120},
     {{200, 120, 220}, {120, 170, 240}, {240, 240, 250}}},
    // Autumn: the flower patches are fallen leaves.
    {{128, 152, 70},
     {116, 140, 62},
     {30, 18, 8},
     {234, 222, 200},
     {222, 190, 128},
     {26, 62, 96},
     {84, 144, 156},
     {140, 92, 52},
     {206, 104, 40},
     {140, 128, 116},
     {255, 240, 210},
     {{226, 72, 40}, {240, 180, 60}, {196, 120, 56}}},
    // Winter night: the rough is snow in the moonlight, the sand is snowdrift and the flower patches are snow.
    {{64, 120, 104},
     {56, 108, 94},
     {44, 52, 72},
     {214, 224, 238},
     {222, 230, 240},
     {18, 46, 88},
     {110, 164, 200},
     {118, 88, 68},
     {26, 70, 62},
     {108, 114, 130},
     {214, 232, 255},
     {{236, 242, 255}, {176, 206, 240}, {255, 255, 255}}},
};
static_assert(sizeof(kPalettes) / sizeof(*kPalettes) == static_cast<int>(Theme::Winter) + 1, "one palette per theme");

struct Color {
  float r, g, b;
};

Color rgb(Rgb c) { return {static_cast<float>(c.r), static_cast<float>(c.g), static_cast<float>(c.b)}; }
Color scale(Color c, float k) { return {c.r * k, c.g * k, c.b * k}; }
Color mix(Color a, Color b, float t) { return {a.r + (b.r - a.r) * t, a.g + (b.g - a.g) * t, a.b + (b.b - a.b) * t}; }

Rgb clamp8(Color c) {
  auto v = [](float x) { return static_cast<uint8_t>(std::clamp(x, 0.0f, 255.0f)); };
  return {v(c.r), v(c.g), v(c.b)};
}

uint32_t hash(int x, int y) {
  uint32_t h = static_cast<uint32_t>(x) * 374761393u + static_cast<uint32_t>(y) * 668265263u;
  h = (h ^ (h >> 13)) * 1274126177u;
  return h ^ (h >> 16);
}

float noise(int x, int y) { return static_cast<float>(hash(x, y) & 0xFFFF) / 65535.0f; }

// Noise with soft transitions: the corners of cells of the given size, mixed.
float smoothNoise(float x, float y, float cell) {
  const float fx = x / cell, fy = y / cell;
  const int i = static_cast<int>(std::floor(fx)), j = static_cast<int>(std::floor(fy));
  const float tx = fx - i, ty = fy - j;
  const float a = noise(i, j) + (noise(i + 1, j) - noise(i, j)) * tx;
  const float b = noise(i, j + 1) + (noise(i + 1, j + 1) - noise(i, j + 1)) * tx;
  return a + (b - a) * ty;
}

float shadeOf(float gx, float gy) {
  const float nx = -gx * kRelief, ny = -gy * kRelief;
  const float len = std::sqrt(nx * nx + ny * ny + 1);
  return std::clamp((nx * kLx + ny * kLy + kLz) / (len * kLz), 0.45f, 1.45f);
}

float minDistance(const Shape* s, int n, float x, float y) {
  float d = 1e9f;
  for (int i = 0; i < n; ++i) d = std::min(d, distance(s[i], x, y));
  return d;
}

// A ball of light from the upper left on a round thing: 1 at the center, more on the lit side.
float dome(float lx, float ly) { return 0.74f + 0.42f * -(lx * 0.7f + ly * 0.7f); }

Color decoration(const Course& c, const Palette& p, Color col, float x, float y) {
  for (int i = 0; i < c.decos.count; ++i) {
    const Deco& d = c.decos.items[i];
    const float reach = d.kind == Deco::Lantern ? d.r * 6 : d.r + 8;
    if (std::fabs(x - d.x) > reach || std::fabs(y - d.y) > reach) continue;
    const float dist = std::hypot(x - d.x, y - d.y);
    switch (d.kind) {
      case Deco::Tree:
      case Deco::Stone: {
        const float off = d.kind == Deco::Tree ? 7 : 3;
        if (std::hypot(x - d.x - off, y - d.y - off) < d.r) col = scale(col, 0.5f);
        if (dist < d.r) {
          const float lx = (x - d.x) / d.r, ly = (y - d.y) / d.r;
          float k = dome(lx, ly);
          if (d.kind == Deco::Tree) {
            // Leaf clusters: a coarse noise gives the canopy a texture.
            k += 0.3f * (smoothNoise(x, y, 5) - 0.5f);
            if (dist > d.r * 0.86f) k *= 0.72f;
          }
          col = scale(rgb(d.kind == Deco::Tree ? p.canopy : p.stone), k);
        }
        break;
      }
      case Deco::Flowers: {
        if (dist >= d.r) break;
        // A patch of tall grass with flowers in it.
        col = mix(col, scale(rgb(p.grassB), 0.55f + 0.2f * smoothNoise(x, y, 3)), std::min(1.0f, 2 * (1 - dist / d.r)));
        const int cx = static_cast<int>(x / 5), cy = static_cast<int>(y / 5);
        const uint32_t h = hash(cx, cy);
        const float fx = cx * 5 + 1 + (h >> 8) % 3, fy = cy * 5 + 1 + (h >> 12) % 3;
        if (h % 3 == 0 && dist < d.r - 2 && std::fabs(x - fx) < 1.2f && std::fabs(y - fy) < 1.2f) {
          col = rgb(p.flowers[(h >> 4) % 3]);
        }
        break;
      }
      case Deco::Lantern: {
        const float glow = std::max(0.0f, 1 - dist / reach);
        col = mix(col, rgb(p.light), 0.55f * glow * glow);
        if (dist < d.r * 0.6f) col = mix(rgb(p.light), {255, 255, 255}, 0.4f);
        break;
      }
    }
  }
  return col;
}

Color water(const Course& c, const Palette& p, float x, float y, int px, int py) {
  const float depth = -minDistance(c.water.items, c.water.count, x, y);
  Color col = mix(rgb(p.waterShallow), rgb(p.waterDeep), std::clamp(depth / 26, 0.0f, 1.0f));
  if (depth < 2.5f) return mix(col, rgb(p.light), 0.45f);
  // Short light strokes look like small waves.
  if (py % 7 == 0 && hash(px / 9, py / 7) % 4 == 0) col = mix(col, rgb(p.light), 0.3f);
  return col;
}

Color plank(const Course& c, const Palette& p, float x, float y, int px, int py) {
  int i = 0;
  while (i + 1 < c.bridges.count && distance(c.bridges.items[i], x, y) >= 0) ++i;
  const Shape& b = c.bridges.items[i];
  const float ux = b.x1 - b.x0, uy = b.y1 - b.y0, len = std::hypot(ux, uy);
  const float along = ((x - b.x0) * ux + (y - b.y0) * uy) / len;
  Color col = scale(rgb(p.wood), 0.92f + 0.16f * noise(px / 2, py));
  if (static_cast<int>(along) % 6 == 0) col = scale(col, 0.7f);
  if (distance(b, x, y) > -2) col = scale(col, 0.75f);
  return col;
}

Color cup(Color col, const Course& c, float x, float y) {
  const float dx = x - c.holeX, dy = y - c.holeY, dist = std::hypot(dx, dy);
  if (dist > kHoleR - 1.2f) return mix(col, {250, 250, 250}, 0.55f);
  // The light falls on the inner wall at the lower right.
  const float lit = std::max(0.0f, (dx + dy) / (dist * 1.414f + 0.01f)) * dist / kHoleR;
  return scale({70, 70, 64}, 0.15f + 0.85f * lit);
}

// The pole, the pennant and the shadow of the pole. Returns false for a pixel that does not show the flag.
bool flag(const Course& c, float x, float y, Color& col) {
  const float fx = x - c.holeX, fy = y - c.holeY;
  if (fx >= 1 && fx < 3 && fy > -38 && fy < -1) {
    col = {236, 236, 230};
    return true;
  }
  const float mid = -32, half = 7;
  if (fx >= 3 && fy > mid - half && fy < mid + half && fx < 3 + 17 * (1 - std::fabs(fy - mid) / half)) {
    col = fy < mid ? Color{232, 64, 60} : Color{196, 44, 44};
    return true;
  }
  return false;
}

bool poleShadow(const Course& c, float x, float y) {
  const float fx = x - c.holeX - 2, fy = y - c.holeY;
  return fx > 0 && fx < 26 && std::fabs(fx - fy) < 1.2f;
}

}  // namespace

const Palette& palette(Theme t) { return kPalettes[static_cast<int>(t)]; }

bool paint(const Course& course, uint16_t* out) {
  float* grid = new (std::nothrow) float[2 * kCells * kCells];
  int16_t* levels = new (std::nothrow) int16_t[kSize];
  if (!grid || !levels) {
    delete[] grid;
    delete[] levels;
    return false;
  }
  // The background shows the walls without the windmill bar. The app draws the bar.
  Course c = course;
  c.mill.speed = 0;
  const Palette& p = palette(c.theme);

  float* heights = grid;
  float* shades = grid + kCells * kCells;
  for (int j = 0; j < kCells; ++j) {
    for (int i = 0; i < kCells; ++i) heights[j * kCells + i] = height(c, i * kGrid, j * kGrid);
  }
  for (int j = 0; j < kCells; ++j) {
    for (int i = 0; i < kCells; ++i) {
      const int l = std::max(i - 1, 0), r = std::min(i + 1, kCells - 1);
      const int u = std::max(j - 1, 0), d = std::min(j + 1, kCells - 1);
      const float gx = (heights[j * kCells + r] - heights[j * kCells + l]) / ((r - l) * kGrid);
      const float gy = (heights[d * kCells + i] - heights[u * kCells + i]) / ((d - u) * kGrid);
      shades[j * kCells + i] = shadeOf(gx, gy);
    }
  }
  auto sample = [&](const float* g, float x, float y) {
    const float fx = x / kGrid, fy = y / kGrid;
    const int i = std::min(static_cast<int>(fx), kCells - 2), j = std::min(static_cast<int>(fy), kCells - 2);
    const float tx = fx - i, ty = fy - j;
    const float a = g[j * kCells + i] + (g[j * kCells + i + 1] - g[j * kCells + i]) * tx;
    const float b = g[(j + 1) * kCells + i] + (g[(j + 1) * kCells + i + 1] - g[(j + 1) * kCells + i]) * tx;
    return a + (b - a) * ty;
  };

  for (int py = 0; py < kSize; ++py) {
    int16_t left = INT16_MIN;
    for (int px = 0; px < kSize; ++px) {
      const float x = px + 0.5f, y = py + 0.5f;
      const float edge = std::hypot(x - kSize / 2.0f, y - kSize / 2.0f);
      uint16_t* o = out + py * kSize + px;
      if (edge > kEdge) {
        *o = 0;
        levels[px] = INT16_MIN;
        left = INT16_MIN;
        continue;
      }
      const float d = wall(c, x, y, 0);
      Color col;
      int16_t level = INT16_MIN;
      if (flag(c, x, y, col)) {
      } else if (d < 0) {
        const bool bridge = minDistance(c.bridges.items, c.bridges.count, x, y) < 0;
        const Ground g = ground(c, x, y);
        if (g == Ground::Water) {
          col = water(c, p, x, y, px, py);
        } else {
          if (bridge) {
            col = plank(c, p, x, y, px, py);
          } else if (g == Ground::Sand) {
            col = scale(rgb(p.sand), 0.9f + 0.2f * noise(px, py));
          } else {
            // Mown stripes.
            col = ((px + py) / 24) % 2 ? rgb(p.grassA) : rgb(p.grassB);
            col = scale(col, 0.97f + 0.06f * noise(px, py));
            const float h = sample(heights, x, y);
            level = static_cast<int16_t>(std::lround(h / kContour));
            if ((left != INT16_MIN && level != left) || (levels[px] != INT16_MIN && level != levels[px])) {
              col = scale(col, 0.84f);
            }
          }
          // Bright sand loses its grain in full light. It gets half the shading.
          const float shade = sample(shades, x, y);
          col = scale(col, g == Ground::Sand ? 1 + (shade - 1) / 2 : shade);
          if (std::fabs(x - c.teeX) < 11 && std::fabs(y - c.teeY) < 11) {
            const bool border = std::fabs(x - c.teeX) > 9.5f || std::fabs(y - c.teeY) > 9.5f;
            col = mix(col, rgb(p.light), border ? 0.7f : 0.18f);
          }
        }
        // The curb throws a shadow to the lower right.
        if (wall(c, x - 5, y - 5, 0) > 0) col = scale(col, 0.7f);
        if (poleShadow(c, x, y)) col = scale(col, 0.6f);
        if (std::hypot(x - c.holeX, y - c.holeY) < kHoleR) col = cup(col, c, x, y);
      } else if (d < kCurb) {
        // A round curb: across its width the surface turns from the green to the outside.
        constexpr float e = 0.5f;
        float nx = wall(c, x + e, y, 0) - wall(c, x - e, y, 0), ny = wall(c, x, y + e, 0) - wall(c, x, y - e, 0);
        const float n = std::hypot(nx, ny) + 1e-6f;
        const float s = d / kCurb * 2 - 1, nz = std::sqrt(std::max(0.0f, 1 - s * s));
        const float lit = (nx / n * s * kLx + ny / n * s * kLy + nz * kLz) / kLz;
        col = scale(rgb(p.curb), std::clamp(lit, 0.55f, 1.25f));
        if (d > kCurb - 1) col = scale(col, 0.6f);
      } else {
        col = scale(rgb(p.rough), 0.8f + 0.4f * noise(px / 2, py / 2));
        if (wall(c, x - 5, y - 5, 0) < kCurb) col = scale(col, 0.55f);
        col = decoration(c, p, col, x, y);
      }
      if (edge > kEdge - kFade) col = scale(col, (kEdge - edge) / kFade);
      *o = pixel(clamp8(col));
      levels[px] = level;
      left = level;
    }
  }
  delete[] grid;
  delete[] levels;
  return true;
}

}  // namespace golf
