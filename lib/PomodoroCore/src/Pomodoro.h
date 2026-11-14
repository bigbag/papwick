#pragma once

#include <cstdint>

namespace pomodoro {

enum class Period : uint8_t { Focus, ShortBreak, LongBreak };
enum class RunState : uint8_t { Idle, Running, Paused, Waiting };
enum class StartMode : uint8_t { Manual, Auto };

inline constexpr uint32_t kFocusMs = 25u * 60u * 1000u;
inline constexpr uint32_t kShortBreakMs = 5u * 60u * 1000u;
inline constexpr uint32_t kLongBreakMs = 15u * 60u * 1000u;
inline constexpr uint8_t kFocusPerCycle = 4;

struct Timer {
  Period period = Period::Focus;
  RunState run = RunState::Idle;
  StartMode startMode = StartMode::Auto;
  uint8_t completedFocus = 0;
  uint32_t remainingMs = kFocusMs;
  uint32_t deadlineMs = 0;
};

inline uint32_t durationMs(Period period) {
  if (period == Period::ShortBreak) return kShortBreakMs;
  if (period == Period::LongBreak) return kLongBreakMs;
  return kFocusMs;
}

inline uint32_t displayedMinutes(uint32_t remainingMs) {
  if (remainingMs == 0) return 0;
  return (remainingMs + 59999u) / 60000u;
}

inline void expire(Timer& timer, uint32_t nowMs) {
  if (timer.period == Period::Focus) {
    if (timer.completedFocus < kFocusPerCycle) ++timer.completedFocus;
    timer.period = timer.completedFocus == kFocusPerCycle ? Period::LongBreak : Period::ShortBreak;
  } else {
    if (timer.period == Period::LongBreak) timer.completedFocus = 0;
    timer.period = Period::Focus;
  }
  timer.remainingMs = durationMs(timer.period);
  if (timer.startMode == StartMode::Auto) {
    timer.run = RunState::Running;
    timer.deadlineMs = nowMs + timer.remainingMs;
    return;
  }
  timer.run = RunState::Waiting;
  timer.deadlineMs = 0;
}

inline bool deadlinePassed(const Timer& timer, uint32_t nowMs) {
  const uint32_t until = timer.deadlineMs - nowMs;
  return until == 0 || until > durationMs(timer.period);
}

inline void start(Timer& timer, uint32_t nowMs) {
  if (timer.run != RunState::Idle && timer.run != RunState::Waiting) return;
  if (timer.run == RunState::Idle) timer.period = Period::Focus;
  timer.remainingMs = durationMs(timer.period);
  timer.deadlineMs = nowMs + timer.remainingMs;
  timer.run = RunState::Running;
}

inline void pause(Timer& timer, uint32_t nowMs) {
  if (timer.run != RunState::Running) return;
  if (deadlinePassed(timer, nowMs)) {
    expire(timer, nowMs);
    return;
  }
  timer.remainingMs = timer.deadlineMs - nowMs;
  timer.run = RunState::Paused;
}

inline void resume(Timer& timer, uint32_t nowMs) {
  if (timer.run != RunState::Paused) return;
  timer.deadlineMs = nowMs + timer.remainingMs;
  timer.run = RunState::Running;
}

inline void reset(Timer& timer) {
  const StartMode mode = timer.startMode;
  timer = Timer{};
  timer.startMode = mode;
}

inline void skipBreak(Timer& timer, uint32_t nowMs) {
  if (timer.run != RunState::Running || timer.period == Period::Focus) return;
  expire(timer, nowMs);
}

inline void tick(Timer& timer, uint32_t nowMs) {
  if (timer.run != RunState::Running) return;
  if (!deadlinePassed(timer, nowMs)) {
    timer.remainingMs = timer.deadlineMs - nowMs;
    return;
  }
  expire(timer, nowMs);
}

}  // namespace pomodoro
