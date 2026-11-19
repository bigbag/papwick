#include "Stopwatch.h"

#include <algorithm>
#include <cstdio>

namespace stopwatch {

namespace {

bool differ(const Watch& w) {
  const auto first = w.laps.begin(), last = first + w.lapCount;
  return w.lapCount >= 2 && *std::min_element(first, last) != *std::max_element(first, last);
}

}  // namespace

uint32_t elapsed(const Watch& w, uint32_t nowMs) {
  const uint32_t run = w.running ? nowMs - w.startMs : 0;
  return run < kMaxMs - w.storedMs ? w.storedMs + run : kMaxMs;
}

void start(Watch& w, uint32_t nowMs) {
  if (w.running) return;
  w.running = true;
  w.startMs = nowMs;
}

void stop(Watch& w, uint32_t nowMs) {
  if (!w.running) return;
  w.storedMs = elapsed(w, nowMs);
  w.running = false;
}

bool lap(Watch& w, uint32_t nowMs) {
  const uint32_t t = elapsed(w, nowMs);
  if (!w.running || w.lapCount == kMaxLaps || t >= kMaxMs) return false;
  w.laps[w.lapCount++] = t - w.lapStartMs;
  w.lapStartMs = t;
  return true;
}

void reset(Watch& w) { w = Watch{}; }

int best(const Watch& w) {
  if (!differ(w)) return -1;
  return static_cast<int>(std::min_element(w.laps.begin(), w.laps.begin() + w.lapCount) - w.laps.begin());
}

int worst(const Watch& w) {
  if (!differ(w)) return -1;
  return static_cast<int>(std::max_element(w.laps.begin(), w.laps.begin() + w.lapCount) - w.laps.begin());
}

void format(uint32_t ms, char* out, size_t size) {
  const unsigned long h = ms / 3600000, m = ms / 60000 % 60, s = ms / 1000 % 60, cs = ms / 10 % 100;
  if (h) {
    snprintf(out, size, "%lu:%02lu:%02lu", h, m, s);
  } else {
    snprintf(out, size, "%02lu:%02lu.%02lu", m, s, cs);
  }
}

}  // namespace stopwatch
