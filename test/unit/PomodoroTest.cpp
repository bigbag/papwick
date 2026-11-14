#include "Pomodoro.h"
#include "test_utils.h"

namespace {

using pomodoro::kFocusMs;
using pomodoro::kLongBreakMs;
using pomodoro::kShortBreakMs;
using pomodoro::Period;
using pomodoro::RunState;
using pomodoro::StartMode;
using pomodoro::Timer;

int stateId(RunState state) { return static_cast<int>(state); }
int periodId(Period period) { return static_cast<int>(period); }
int modeId(StartMode mode) { return static_cast<int>(mode); }

void expectIdleFocus(TestUtils::TestRunner& runner, const Timer& timer, const char* name) {
  runner.expectEq(stateId(RunState::Idle), stateId(timer.run), name);
  runner.expectEq(periodId(Period::Focus), periodId(timer.period), name);
  runner.expectEq(0, static_cast<int>(timer.completedFocus), name);
  runner.expectEq(kFocusMs, timer.remainingMs, name);
}

void completeRunningPeriod(Timer& timer) { pomodoro::tick(timer, timer.deadlineMs); }

}  // namespace

int main() {
  TestUtils::TestRunner runner("Pomodoro timer");
  Timer timer;

  expectIdleFocus(runner, timer, "new timer is idle focus");
  runner.expectEq(modeId(StartMode::Auto), modeId(timer.startMode), "new timer is auto");
  runner.expectEq(25u, pomodoro::displayedMinutes(timer.remainingMs), "idle shows 25 minutes");

  pomodoro::start(timer, 1000);
  runner.expectEq(stateId(RunState::Running), stateId(timer.run), "start runs focus");
  runner.expectEq(1000u + kFocusMs, timer.deadlineMs, "start sets the focus deadline");
  pomodoro::pause(timer, 61000);
  const uint32_t frozen = timer.remainingMs;
  runner.expectEq(kFocusMs - 60000u, frozen, "pause stores the remaining time");
  pomodoro::tick(timer, 61000u + 600000u);
  runner.expectEq(frozen, timer.remainingMs, "paused time does not move");
  pomodoro::resume(timer, 7000);
  pomodoro::tick(timer, 67000);
  runner.expectEq(frozen - 60000u, timer.remainingMs, "resume continues from the frozen time");

  timer = Timer{};
  timer.startMode = StartMode::Manual;
  pomodoro::start(timer, 0);
  pomodoro::tick(timer, kFocusMs + kShortBreakMs);
  runner.expectEq(stateId(RunState::Waiting), stateId(timer.run), "a late tick waits once");
  runner.expectEq(periodId(Period::ShortBreak), periodId(timer.period), "a late tick does not finish the break");
  runner.expectEq(kShortBreakMs, timer.remainingMs, "the next period keeps its full duration");

  timer = Timer{};
  timer.startMode = StartMode::Manual;
  for (int focus = 1; focus <= 3; ++focus) {
    pomodoro::start(timer, 10);
    completeRunningPeriod(timer);
    runner.expectEq(stateId(RunState::Waiting), stateId(timer.run), "manual focus waits");
    runner.expectEq(periodId(Period::ShortBreak), periodId(timer.period), "the first breaks are short");
    runner.expectEq(focus, static_cast<int>(timer.completedFocus), "focus completion raises the count");
    pomodoro::start(timer, 20);
    completeRunningPeriod(timer);
    runner.expectEq(periodId(Period::Focus), periodId(timer.period), "a short break returns to focus");
    runner.expectEq(focus, static_cast<int>(timer.completedFocus), "a short break keeps the count");
  }
  pomodoro::start(timer, 30);
  completeRunningPeriod(timer);
  runner.expectEq(periodId(Period::LongBreak), periodId(timer.period), "the fourth focus waits for a long break");
  runner.expectEq(4, static_cast<int>(timer.completedFocus), "the fourth focus sets count 4");
  runner.expectEq(kLongBreakMs, timer.remainingMs, "the long break shows 15 minutes");
  pomodoro::start(timer, 40);
  completeRunningPeriod(timer);
  runner.expectEq(periodId(Period::Focus), periodId(timer.period), "a long break waits for focus");
  runner.expectEq(0, static_cast<int>(timer.completedFocus), "a long break clears the count");

  timer = Timer{};
  pomodoro::start(timer, 0);
  completeRunningPeriod(timer);
  runner.expectEq(stateId(RunState::Running), stateId(timer.run), "auto starts the next period");
  runner.expectEq(periodId(Period::ShortBreak), periodId(timer.period), "auto starts the short break");
  pomodoro::start(timer, 1);
  runner.expectEq(periodId(Period::ShortBreak), periodId(timer.period), "start during running does nothing");

  timer = Timer{};
  timer.startMode = StartMode::Manual;
  pomodoro::start(timer, 0);
  completeRunningPeriod(timer);
  pomodoro::start(timer, 5);
  pomodoro::skipBreak(timer, 6);
  runner.expectEq(stateId(RunState::Waiting), stateId(timer.run), "skip waits in manual mode");
  runner.expectEq(periodId(Period::Focus), periodId(timer.period), "skip ends the break");
  runner.expectEq(1, static_cast<int>(timer.completedFocus), "skip does not change the focus count");

  timer = Timer{};
  pomodoro::start(timer, 50);
  pomodoro::skipBreak(timer, 51);
  runner.expectEq(stateId(RunState::Running), stateId(timer.run), "skip during focus does nothing");
  runner.expectEq(periodId(Period::Focus), periodId(timer.period), "skip during focus keeps focus");

  timer.startMode = StartMode::Auto;
  pomodoro::reset(timer);
  expectIdleFocus(runner, timer, "reset returns to idle focus");
  runner.expectEq(modeId(StartMode::Auto), modeId(timer.startMode), "reset keeps the start mode");
  pomodoro::start(timer, 200);
  pomodoro::pause(timer, 200);
  pomodoro::reset(timer);
  expectIdleFocus(runner, timer, "reset from paused returns to idle focus");
  runner.expectEq(modeId(StartMode::Auto), modeId(timer.startMode), "reset from paused keeps auto");
  timer.startMode = StartMode::Manual;
  pomodoro::start(timer, 210);
  completeRunningPeriod(timer);
  pomodoro::reset(timer);
  expectIdleFocus(runner, timer, "reset from waiting returns to idle focus");
  pomodoro::start(timer, 220);
  completeRunningPeriod(timer);
  pomodoro::start(timer, 230);
  pomodoro::reset(timer);
  expectIdleFocus(runner, timer, "reset from a break returns to idle focus");
  pomodoro::skipBreak(timer, 240);
  expectIdleFocus(runner, timer, "skip during idle does nothing");
  runner.expectEq(25u, pomodoro::displayedMinutes(kFocusMs), "25:00 shows 25");
  runner.expectEq(25u, pomodoro::displayedMinutes(24u * 60u * 1000u + 1u), "24 minutes plus 1 ms shows 25");
  runner.expectEq(24u, pomodoro::displayedMinutes(24u * 60u * 1000u), "24:00 shows 24");
  runner.expectEq(1u, pomodoro::displayedMinutes(1), "1 ms shows 1 minute");

  timer = Timer{};
  timer.startMode = StartMode::Manual;
  pomodoro::start(timer, 0xFFFF0000u);
  pomodoro::tick(timer, timer.deadlineMs - 1u);
  runner.expectEq(stateId(RunState::Running), stateId(timer.run), "wrap still has time before the deadline");
  runner.expectEq(1u, pomodoro::displayedMinutes(timer.remainingMs), "1 ms before the deadline shows 1");
  pomodoro::tick(timer, timer.deadlineMs);
  runner.expectEq(stateId(RunState::Waiting), stateId(timer.run), "the deadline expires across the wrap");

  return runner.exitCode();
}
