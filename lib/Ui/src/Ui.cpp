#include "Ui.h"

#include <I18n.h>
#include <Logging.h>
#include <Power.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <tuple>

#include "OpenSans16.h"

namespace ui {

namespace {

constexpr const char* TAG = "UI";
constexpr int kRowTop = 118, kRowPitch = 52;

M5Canvas sprite(&M5.Display);

constexpr int kWheelPitch = 56;
constexpr uint16_t kFaint = 0x4208;

void format(char* buf, size_t n, float v, int decimals) { snprintf(buf, n, "%.*f", decimals, v); }

}  // namespace

// Taps on this device land up to about 40 px below the target near the bottom edge, so the area extends down.
// Buttons side by side have gaps of 6 px, so the area extends only 3 px to the sides. Then one tap hits one button.
bool Rect::hit(int px, int py) const { return px >= x - 3 && px < x + w + 3 && py >= y - 4 && py < y + h + 24; }

bool begin() {
  sprite.setColorDepth(16);
  if (!sprite.createSprite(M5.Display.width(), M5.Display.height())) {
    LOG_ERR(TAG, "Screen sprite alloc failed");
    return false;
  }
  return true;
}

M5Canvas& canvas() { return sprite; }
int cx() { return M5.Display.width() / 2; }
int cy() { return M5.Display.height() / 2; }
void clear() { sprite.fillScreen(TFT_BLACK); }
void push() { sprite.pushSprite(0, 0); }

void text(const char* s, int x, int y, const lgfx::IFont* font, uint16_t color, textdatum_t datum) {
  sprite.setFont(font);
  sprite.setTextColor(color);
  sprite.setTextDatum(datum);
  sprite.drawString(s, x, y);
}

void button(const Rect& r, const char* label, uint16_t bg, uint16_t fg) {
  sprite.fillRoundRect(r.x, r.y, r.w, r.h, r.h / 2, bg);
  text(label, r.x + r.w / 2, r.y + r.h / 2, &fonts::DejaVu24, fg);
}

void edgeArc(float from, float to, uint16_t color) { edgeArc(sprite, from, to, color); }

void edgeArc(LovyanGFX& dst, float from, float to, uint16_t color) {
  if (to <= from) return;
  const float span = std::fmin(to - from, 360.0f);
  from = std::fmod(from, 360.0f);
  to = from + span;
  if (to > 360.0f) {
    dst.fillArc(cx(), cy(), kRingInner, kRingOuter, from, 360.0f, color);
    dst.fillArc(cx(), cy(), kRingInner, kRingOuter, 0.0f, to - 360.0f, color);
  } else {
    dst.fillArc(cx(), cy(), kRingInner, kRingOuter, from, to, color);
  }
}

void progressRing(float fraction, uint16_t color) {
  fraction = std::fmax(0.0f, std::fmin(1.0f, fraction));
  edgeArc(270.0f + 360.0f * fraction, 630.0f, kDark);
  edgeArc(270.0f, 270.0f + 360.0f * fraction, color);
}

void pageDots(int page, int count) {
  constexpr float kStep = 6.0f;
  const float first = 90.0f + (count - 1) * kStep / 2;
  for (int p = 0; p < count; ++p) {
    const float at = first - p * kStep;
    edgeArc(at - 1.5f, at + 1.5f, p == page ? TFT_WHITE : kDark);
  }
}

const char* const* onOffLabels() {
  static const char* const l[] = {tr(OFF), tr(ON)};
  return l;
}

void battery(int x, int y) {
  constexpr int kW = 30, kH = 14, kTipW = 3, kTipH = 6, kGap = 6;
  const int level = std::clamp(board::batteryLevel(), 0, 100);
  const uint16_t fill = level <= 10 ? kRed : (level <= 20 ? kAmber : TFT_WHITE);
  char line[8];
  snprintf(line, sizeof(line), "%d%%", level);
  sprite.setFont(&fonts::DejaVu12);
  const int left = x - (kW + kTipW + kGap + sprite.textWidth(line)) / 2, top = y - kH / 2;
  sprite.drawRoundRect(left, top, kW, kH, 3, kGrey);
  sprite.fillRect(left + kW, y - kTipH / 2, kTipW, kTipH, kGrey);
  sprite.fillRect(left + 2, top + 2, (kW - 4) * level / 100, kH - 4, fill);
  if (board::onUsb()) {
    const int bx = left + kW / 2;
    sprite.fillTriangle(bx + 2, top - 1, bx - 5, y + 1, bx, y + 1, kDark);
    sprite.fillTriangle(bx - 2, top + kH + 1, bx + 5, y - 1, bx, y - 1, kDark);
    sprite.fillTriangle(bx + 1, top + 1, bx - 3, y, bx, y, kAmber);
    sprite.fillTriangle(bx - 1, top + kH - 1, bx + 3, y, bx, y, kAmber);
  }
  text(line, left + kW + kTipW + kGap, y, &fonts::DejaVu12, kGrey, middle_left);
}

void banner(const char* s, int y, uint16_t color) {
  constexpr int kPadX = 24, kPadY = 12, kRadius = 14;
  sprite.setFont(&fonts::DejaVu40);
  const int w = sprite.textWidth(s) + 2 * kPadX, h = sprite.fontHeight() + 2 * kPadY;
  const int x = cx() - w / 2, top = y - h / 2;
  sprite.fillRoundRect(x, top, w, h, kRadius, color);
  sprite.fillRoundRect(x + 3, top + 3, w - 6, h - 6, kRadius - 3, TFT_BLACK);
  text(s, cx(), y, &fonts::DejaVu40, color);
}

void hints(int y, const char* blue, const char* green, uint16_t color) {
  constexpr int kDotR = 6, kDotGap = 6, kGap = 24;
  sprite.setFont(&OpenSans16);
  auto width = [](const char* s) { return s ? 2 * kDotR + kDotGap + sprite.textWidth(s) : 0; };
  const int wb = width(blue), wg = width(green);
  int x = cx() - (wb + wg + (blue && green ? kGap : 0)) / 2;
  for (const auto& [label, dot, w] : {std::make_tuple(blue, kBlue, wb), std::make_tuple(green, kGreen, wg)}) {
    if (!label) continue;
    sprite.fillCircle(x + kDotR, y, kDotR, dot);
    text(label, x + 2 * kDotR + kDotGap, y, &OpenSans16, color, middle_left);
    x += w + kGap;
  }
}

bool pickerUpdate(Picker& p, const Input& in) {
  const float before = p.value;
  auto apply = [&p](int steps) {
    const float v = std::round((p.value + steps * p.step) / p.step) * p.step;
    p.value = std::fmax(p.min, std::fmin(p.max, v));
  };
  static uint32_t holdStartMs = 0;
  if (in.hold) holdStartMs = millis();
  if (in.swipeUp || in.swipeDown) {
    const int steps = std::max(1, in.distance / kWheelPitch);
    apply(in.swipeUp ? steps : -steps);
  }
  if ((in.tap || in.repeat) && std::abs(in.x - cx()) < 170) {
    const int row = std::clamp(static_cast<int>(std::lround(static_cast<float>(in.y - cy()) / kWheelPitch)), -2, 2);
    const int sign = (row > 0) - (row < 0);
    apply(in.tap ? row : sign * static_cast<int>(1 + (millis() - holdStartMs) / 1500));
  }
  return p.value != before;
}

void pickerDraw(const Picker& p, const char* green) {
  text(p.label, cx(), 62, &fonts::DejaVu24, kGrey);
  if (p.unit) text(p.unit, cx(), 92, &fonts::DejaVu18, kGreen);
  sprite.fillRoundRect(cx() - 150, cy() - kWheelPitch / 2, 300, kWheelPitch, kWheelPitch / 2, kDark);
  for (int k = -2; k <= 2; ++k) {
    const float v = p.value + k * p.step;
    if (v < p.min - p.step / 2 || v > p.max + p.step / 2) continue;
    char buf[16];
    format(buf, sizeof(buf), v, p.decimals);
    const int far = std::abs(k);
    text(p.text ? p.text(std::lround(v)) : buf, cx(), cy() + k * kWheelPitch,
         far == 0 ? &fonts::DejaVu40 : (far == 1 ? &fonts::DejaVu24 : &fonts::DejaVu18),
         far == 0 ? TFT_WHITE : (far == 1 ? kGrey : kFaint));
  }
  hints(kHintsY, tr(BACK), green ? green : tr(NEXT));
}

int rowsUpdate(Row* rows, int count, const Input& in) {
  const int n = std::min(count, kMaxRows);
  // Each row owns the band down to the next row. The last row gets 24 px below its drawn bottom, because taps near
  // the bottom edge land lower than the drawn rows.
  if (!in.tap || std::abs(in.x - cx()) > 170 || in.y < kRowTop - 4 || in.y >= kRowTop + n * kRowPitch + 18) return -1;
  const int i = std::min((in.y - kRowTop + 4) / kRowPitch, n - 1);
  Row& r = rows[i];
  if (r.value && r.count) *r.value = (*r.value + 1) % r.count;
  return i;
}

void rowsDraw(const char* title, const Row* rows, int count) {
  text(title, cx(), 80, &fonts::DejaVu24, kGreen);
  for (int i = 0; i < std::min(count, kMaxRows); ++i) {
    const Rect r = {cx() - 130, kRowTop + i * kRowPitch, 260, kRowPitch - 6};
    const Row& row = rows[i];
    sprite.fillRoundRect(r.x, r.y, r.w, r.h, r.h / 2, kDark);
    text(row.label, r.x + 24, r.y + r.h / 2, &fonts::DejaVu18, TFT_WHITE, middle_left);
    const char* value = row.value && row.count ? row.options[*row.value] : row.detail;
    if (value) text(value, r.x + r.w - 24, r.y + r.h / 2, &fonts::DejaVu18, kAmber, middle_right);
  }
}

void confirmDraw(const char* question, const char* yes) {
  clear();
  text(question, cx(), cy() - 20, &fonts::DejaVu24);
  hints(kHintsY, tr(NO), yes);
}

}  // namespace ui
