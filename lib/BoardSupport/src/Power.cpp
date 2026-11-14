#include "Power.h"

#include <Logging.h>
#include <M5Unified.h>

#include <algorithm>

namespace board {

namespace {

constexpr const char* TAG = "POWER";
constexpr uint8_t kDimBrightness = 20;
constexpr int kUsbPresentMv = 4000;
constexpr uint32_t kLedCheckMs = 2000;

Light current = Light::On;
uint8_t brightness = 160;
uint32_t lastInputMs = 0;

void applyBrightness() {
  if (current != Light::Off) {
    M5.Display.setBrightness(current == Light::Dim ? std::min(brightness, kDimBrightness) : brightness);
  }
}

}  // namespace

void setBrightness(uint8_t level) {
  brightness = level;
  applyBrightness();
}

void setLight(Light next) {
  if (next == current) return;
  if (next == Light::Off) {
    M5.Display.sleep();
  } else if (current == Light::Off) {
    M5.Display.wakeup();
  }
  current = next;
  applyBrightness();
  LOG_INF(TAG, "Screen %s", next == Light::On ? "on" : (next == Light::Dim ? "dim" : "off"));
}

Light light() { return current; }

bool updateLight(const Input& in, uint32_t offMs) {
  const uint32_t now = millis();
  if (in.any()) {
    lastInputMs = now;
    const bool wasOff = current == Light::Off;
    setLight(Light::On);
    return wasOff;
  }
  // Without input the light only goes down, so a screen that the user turned off stays off.
  if (offMs == 0) {
    lastInputMs = now;
  } else if (now - lastInputMs >= offMs) {
    setLight(Light::Off);
  } else if (now - lastInputMs >= offMs / 2 && current == Light::On) {
    setLight(Light::Dim);
  }
  return false;
}

bool onUsb() { return M5.Power.getVBUSVoltage() > kUsbPresentMv; }

int batteryLevel() { return M5.Power.getBatteryLevel(); }

void updateChargeLed() {
  static uint32_t lastCheck = 0;
  static int ledOn = -1;
  const uint32_t ms = millis();
  if (lastCheck && ms - lastCheck < kLedCheckMs) return;
  lastCheck = ms;
  const bool charging = onUsb() && batteryLevel() < 100;
  if (ledOn == static_cast<int>(charging)) return;
  if (!M5.Power.M5pm1.setLedEnLevel(charging)) {
    LOG_WRN(TAG, "Charge LED write failed");
    return;
  }
  ledOn = charging;
  LOG_INF(TAG, "Charge LED %s", charging ? "on" : "off");
}

void powerOff() {
  LOG_INF(TAG, "Power off");
  M5.Power.powerOff();
  delay(1000);
  LOG_WRN(TAG, "Power off failed, restart");
  ESP.restart();
}

}  // namespace board
