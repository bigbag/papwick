#include "Icons.h"

#include <cmath>
#include <initializer_list>

#include "Ui.h"

namespace icon {

namespace {

// The half width of a line. drawWideLine() takes the half width.
constexpr float kStroke = 4;

void line(int x0, int y0, int x1, int y1, uint16_t ink, float half = kStroke) {
  ui::canvas().drawWideLine(x0, y0, x1, y1, half, ink);
}

}  // namespace

void dumbbell(int x, int y, uint16_t ink, uint16_t) {
  auto& c = ui::canvas();
  line(x - 26, y, x + 26, y, ink);
  for (int s : {-1, 1}) {
    c.fillRoundRect(x + s * 27 - 6, y - 22, 12, 44, 4, ink);
    c.fillRoundRect(x + s * 38 - 4, y - 14, 8, 28, 3, ink);
  }
}

void hourglass(int x, int y, uint16_t ink, uint16_t bg) {
  auto& c = ui::canvas();
  line(x - 26, y - 36, x + 26, y - 36, ink);
  line(x - 26, y + 36, x + 26, y + 36, ink);
  c.fillTriangle(x - 22, y - 30, x + 22, y - 30, x, y, ink);
  c.fillTriangle(x - 18, y - 26, x + 18, y - 26, x, y - 4, bg);
  c.fillTriangle(x - 12, y - 16, x + 12, y - 16, x, y - 4, ink);
  c.fillTriangle(x, y, x - 22, y + 30, x + 22, y + 30, ink);
  c.fillTriangle(x, y + 4, x - 18, y + 26, x + 18, y + 26, bg);
  c.fillTriangle(x - 18, y + 26, x + 18, y + 26, x, y + 14, ink);
}

void clock(int x, int y, uint16_t ink, uint16_t bg) {
  auto& c = ui::canvas();
  c.fillCircle(x, y, 38, ink);
  c.fillCircle(x, y, 30, bg);
  line(x, y, x, y - 22, ink);
  line(x, y, x + 16, y, ink);
}

void tiles(int x, int y, uint16_t ink, uint16_t) {
  for (int i = 0; i < 4; ++i) ui::canvas().fillRoundRect(x - 36 + i % 2 * 40, y - 36 + i / 2 * 40, 32, 32, 6, ink);
}

void discs(int x, int y, uint16_t ink, uint16_t bg) {
  auto& c = ui::canvas();
  c.fillRoundRect(x - 42, y - 32, 84, 64, 10, ink);
  for (int i = 0; i < 8; ++i) {
    const int cx = x - 28 + i % 4 * 19, cy = y - 11 + i / 4 * 22;
    c.fillCircle(cx, cy, 8, bg);
    if (i >= 4) c.fillCircle(cx, cy, 5, ink);
  }
}

void lights(int x, int y, uint16_t ink, uint16_t bg) {
  auto& c = ui::canvas();
  for (int i = 0; i < 9; ++i) {
    const int cx = x - 26 + i % 3 * 26, cy = y - 26 + i / 3 * 26;
    c.fillRoundRect(cx - 11, cy - 11, 22, 22, 5, ink);
    if (i % 3 != 1 && i / 3 != 1) c.fillRoundRect(cx - 6, cy - 6, 12, 12, 3, bg);
  }
}

void crate(int x, int y, uint16_t ink, uint16_t bg) {
  auto& c = ui::canvas();
  c.fillRoundRect(x - 34, y - 34, 68, 68, 8, ink);
  c.fillRoundRect(x - 24, y - 24, 48, 48, 4, bg);
  line(x - 22, y + 22, x + 22, y - 22, ink);
}

void gear(int x, int y, uint16_t ink, uint16_t bg) {
  constexpr float kPi = 3.14159265f;
  auto& c = ui::canvas();
  for (int k = 0; k < 8; ++k) {
    const float a = k * kPi / 4;
    line(x, y, x + lroundf(34 * cosf(a)), y + lroundf(34 * sinf(a)), ink, 7);
  }
  c.fillCircle(x, y, 28, ink);
  c.fillCircle(x, y, 12, bg);
}

}  // namespace icon
