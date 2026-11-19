#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

// The stopwatch without hardware: the elapsed time, the laps and the time text.
namespace stopwatch {

constexpr uint8_t kMaxLaps = 99;
// The time stops at 99:59:59. Thus the text fits on the screen, and millis() differences stay below the overflow.
constexpr uint32_t kMaxMs = 100u * 3600000 - 1;

struct Watch {
  bool running = false;
  // millis() at the last start.
  uint32_t startMs = 0;
  // The elapsed time before the last start.
  uint32_t storedMs = 0;
  // The elapsed time at the start of the current lap.
  uint32_t lapStartMs = 0;
  uint8_t lapCount = 0;
  std::array<uint32_t, kMaxLaps> laps{};
};

// The elapsed time, up to kMaxMs.
uint32_t elapsed(const Watch& w, uint32_t nowMs);
// Starts the time, or continues it after a stop.
void start(Watch& w, uint32_t nowMs);
void stop(Watch& w, uint32_t nowMs);
// Ends the current lap. Returns false when the watch is stopped, has kMaxLaps laps or is at kMaxMs.
bool lap(Watch& w, uint32_t nowMs);
void reset(Watch& w);
// The index of the shortest and of the longest lap. -1 when fewer than two laps have different times.
int best(const Watch& w);
int worst(const Watch& w);
// mm:ss.cc below one hour, h:mm:ss from one hour.
void format(uint32_t ms, char* out, size_t size);

}  // namespace stopwatch
