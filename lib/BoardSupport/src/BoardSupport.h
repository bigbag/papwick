#pragma once

#include <ctime>

#include "Audio.h"
#include "Input.h"
#include "Power.h"
#include "Wifi.h"

namespace board {

// Starts M5Unified with the TriviaPOD audio enabled and the StopWatch as the fallback board.
void begin();

// Waits for a USB serial host for at most timeoutMs, so that start-up logs are not lost.
void waitForSerial(unsigned long timeoutMs);

// Sets the local time zone as a POSIX TZ string, for example "UTC-3" for UTC+3. Call it before syncClock().
void setTimeZone(const char* tz);

// Sets the system clock from the RTC. When the RTC holds no valid time, sets both from the build time.
// The RTC holds UTC. Returns the current Unix time.
time_t syncClock();

// Sets the system clock and the RTC.
void setClock(time_t t);

// Starts an NTP request. The Wi-Fi must be connected.
void startNtp();
// Returns true after NTP sets the system clock. Then it writes the RTC and stops NTP.
bool ntpDone();
void stopNtp();

}  // namespace board
