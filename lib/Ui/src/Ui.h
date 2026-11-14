#pragma once

#include <Input.h>
#include <M5Unified.h>

namespace ui {

using board::Input;

struct Rect {
  int x, y, w, h;
  bool hit(int px, int py) const;
};

// A vertical wheel of values. The selected value is in the center, two neighbors show above and below.
// text, when set, gives the name of each integer value, for example a time zone.
struct Picker {
  const char* label;
  const char* unit;
  float value, min, max, step;
  int decimals;
  const char* (*text)(int value) = nullptr;
};

// A settings row. A row with options shows the selected option, and a tap selects the next one.
// A row without options shows detail, for example ">" for a sub-screen or "UTC+3".
struct Row {
  const char* label;
  const char* const* options = nullptr;
  uint8_t count = 0;
  uint8_t* value = nullptr;
  const char* detail = nullptr;
};

constexpr uint16_t kGreen = 0x2FE8;
constexpr uint16_t kBlue = 0x3D9F;
constexpr uint16_t kAmber = 0xFD20;
constexpr uint16_t kGrey = 0x8410;
constexpr uint16_t kDark = 0x2104;
constexpr uint16_t kRed = 0xE8E4;
constexpr int kMaxRows = 5;
constexpr int kHintsY = 415;

bool begin();
M5Canvas& canvas();
int cx();
int cy();
void clear();
void push();

void text(const char* s, int x, int y, const lgfx::IFont* font, uint16_t color = TFT_WHITE,
          textdatum_t datum = middle_center);
void button(const Rect& r, const char* label, uint16_t bg, uint16_t fg = TFT_WHITE);
// Draws an arc band near the round edge. Angles in degrees: 0 is 3 o'clock, 270 is 12 o'clock.
void edgeArc(float from, float to, uint16_t color);
void progressRing(float fraction, uint16_t color);
// Draws one dot for each page at the bottom of the edge ring. The current page is white.
void pageDots(int page, int count);
// The "Off" and "On" option labels.
const char* const* onOffLabels();

// Draws the battery icon and level centered at x, y. A bolt shows that USB power is present.
void battery(int x, int y);

// Draws a blue dot with the blue label and a green dot with the green label, centered. A label can be nullptr.
// The dot colors match the left (blue) and right (green) hardware buttons.
void hints(int y, const char* blue, const char* green, uint16_t color = kGrey);

// A swipe up or down moves the wheel by one step for each 56 px. A tap on a neighbor selects it.
// A held touch above or below the center moves the wheel, faster after each 1.5 s. Returns true on a change.
bool pickerUpdate(Picker& p, const Input& in);
// green nullptr shows "next".
void pickerDraw(const Picker& p, const char* green = nullptr);

// A tap on a row with options selects the next option. Returns the index of the tapped row, or -1.
int rowsUpdate(Row* rows, int count, const Input& in);
// Draws the title and at most kMaxRows rows.
void rowsDraw(const char* title, const Row* rows, int count);

// Draws a short message on a black panel with a border of the text color, centered at y. Use it over a game board.
void banner(const char* s, int y, uint16_t color);

// Draws a question. Green answers yes and blue answers no.
void confirmDraw(const char* question, const char* yes);

}  // namespace ui
