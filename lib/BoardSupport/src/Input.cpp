#include "Input.h"

#include <Logging.h>
#include <M5Unified.h>

#include <algorithm>
#include <cstdlib>

namespace board {

namespace {

constexpr const char* TAG = "INPUT";
constexpr int kSwipeMin = 60;
constexpr uint32_t kRepeatMs = 110;

// The M5PM1 click events miss double clicks, so poll() counts the clicks from the button state register.
constexpr uint32_t kPm1Freq = 100000;
constexpr uint8_t kBtnStatus = 0x48, kBtnPressed = 0x01, kBtnFlag = 0x80;
constexpr uint8_t kBtnCfg2 = 0x4A, kDoubleOffDisable = 0x01;
constexpr uint32_t kClickGapMs = 350;
constexpr uint32_t kClickMaxMs = 800;

// Returns the number of clicks of a finished click series, or 0.
int redClicks() {
  static bool down = false;
  static uint32_t pressMs = 0, releaseMs = 0;
  static int clicks = 0;
  const uint8_t s = M5.In_I2C.readRegister8(m5::M5PM1_Class::DEFAULT_ADDRESS, kBtnStatus, kPm1Freq);
  const uint32_t ms = millis();
  const bool pressed = s & kBtnPressed;
  if (pressed && !down) pressMs = ms;
  // The flag catches a press that starts and ends between two reads.
  if ((!pressed && down && ms - pressMs <= kClickMaxMs) || (!pressed && !down && (s & kBtnFlag))) {
    ++clicks;
    releaseMs = ms;
  }
  down = pressed;
  if (down || clicks == 0 || ms - releaseMs < kClickGapMs) return 0;
  const int n = clicks;
  clicks = 0;
  return n;
}

}  // namespace

void beginInput() {
  // By default the M5PM1 powers the device off on a double click. The shell uses the double click for its menu.
  const uint8_t cfg = M5.In_I2C.readRegister8(m5::M5PM1_Class::DEFAULT_ADDRESS, kBtnCfg2, kPm1Freq);
  if (!M5.In_I2C.writeRegister8(m5::M5PM1_Class::DEFAULT_ADDRESS, kBtnCfg2, cfg | kDoubleOffDisable, kPm1Freq)) {
    LOG_ERR(TAG, "Red button setup failed");
  }
  // Drop the press that powered the device on. Else poll() reads it as a click.
  M5.In_I2C.readRegister8(m5::M5PM1_Class::DEFAULT_ADDRESS, kBtnStatus, kPm1Freq);
}

bool Input::any() const {
  return tap || hold || repeat || swipeLeft || swipeRight || swipeUp || swipeDown || blue || green || red || redDouble;
}

Input poll() {
  static uint32_t lastRepeat = 0;
  Input in;
  const auto t = M5.Touch.getDetail();
  if (t.wasClicked() || t.wasHold() || t.isHolding()) {
    in.x = t.x;
    in.y = t.y;
  }
  in.tap = t.wasClicked();
  in.hold = t.wasHold();
  if (t.isHolding() && millis() - lastRepeat >= kRepeatMs) {
    in.repeat = true;
    lastRepeat = millis();
  }
  if (t.wasFlicked()) {
    const int dx = t.distanceX(), dy = t.distanceY();
    in.distance = std::max(std::abs(dx), std::abs(dy));
    if (std::abs(dx) > std::abs(dy)) {
      in.swipeLeft = dx < -kSwipeMin;
      in.swipeRight = dx > kSwipeMin;
    } else {
      in.swipeUp = dy < -kSwipeMin;
      in.swipeDown = dy > kSwipeMin;
    }
  }
  in.blue = M5.BtnA.wasPressed();
  in.green = M5.BtnB.wasPressed();
  const int clicks = redClicks();
  in.red = clicks == 1;
  in.redDouble = clicks >= 2;

  if (in.tap) LOG_DBG(TAG, "Tap %d,%d", in.x, in.y);
  if (in.hold) LOG_DBG(TAG, "Hold %d,%d", in.x, in.y);
  if (in.swipeLeft || in.swipeRight || in.swipeUp || in.swipeDown) {
    LOG_DBG(TAG, "Swipe %s", in.swipeLeft ? "left" : in.swipeRight ? "right" : in.swipeUp ? "up" : "down");
  }
  if (in.blue || in.green) LOG_DBG(TAG, "Button %s", in.blue ? "blue" : "green");
  if (in.red || in.redDouble) LOG_DBG(TAG, "Power %s", in.redDouble ? "double click" : "click");
  return in;
}

}  // namespace board
