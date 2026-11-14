#pragma once

#include <cstdint>

#include "Input.h"

namespace board {

enum class Light : uint8_t { On, Dim, Off };

// Sets the brightness of Light::On. Light::Dim uses the lower of this level and 20.
void setBrightness(uint8_t level);
void setLight(Light next);
Light light();
// Any input turns the screen on. Without input, the screen dims after offMs / 2 and goes off after offMs.
// offMs 0 keeps the screen on. Returns true when the input only woke the screen.
bool updateLight(const Input& in, uint32_t offMs);

bool onUsb();
int batteryLevel();
// Shows charging on the LED next to the USB port. Call it in each loop.
void updateChargeLed();
// Cuts the power. Call it only on battery: with USB, the M5PM1 cuts only the display. Restarts if the cut fails.
void powerOff();

}  // namespace board
