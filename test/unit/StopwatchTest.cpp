#include <string>

#include "Stopwatch.h"
#include "test_utils.h"

using namespace stopwatch;

namespace {

std::string text(uint32_t ms) {
  char s[16];
  format(ms, s, sizeof(s));
  return s;
}

}  // namespace

int main() {
  TestUtils::TestRunner runner("Stopwatch");

  Watch w;
  runner.expectEq(0u, elapsed(w, 5000), "a new watch shows zero");
  start(w, 1000);
  runner.expectEq(2500u, elapsed(w, 3500), "the time runs after the start");
  stop(w, 4000);
  runner.expectEq(3000u, elapsed(w, 9000), "the time stays after the stop");
  runner.expectFalse(lap(w, 9000), "a stopped watch takes no lap");
  start(w, 10000);
  runner.expectEq(3500u, elapsed(w, 10500), "the start continues the time");

  runner.expectTrue(lap(w, 10500) && lap(w, 12000) && lap(w, 15000), "a running watch takes laps");
  runner.expectEq(3u, static_cast<unsigned>(w.lapCount), "three laps");
  runner.expectTrue(w.laps[0] == 3500 && w.laps[1] == 1500 && w.laps[2] == 3000, "each lap is the time since the last");
  runner.expectEq(1, best(w), "the shortest lap is the best");
  runner.expectEq(0, worst(w), "the longest lap is the worst");

  Watch even;
  start(even, 0);
  lap(even, 1000);
  lap(even, 2000);
  runner.expectTrue(best(even) == -1 && worst(even) == -1, "equal laps have no best and no worst");

  Watch full;
  start(full, 0);
  bool all = true;
  for (uint32_t i = 1; i <= kMaxLaps; ++i) all = lap(full, i * 10) && all;
  runner.expectTrue(all && full.lapCount == kMaxLaps, "the watch takes kMaxLaps laps");
  runner.expectFalse(lap(full, 5000), "the watch takes no lap after kMaxLaps");

  Watch wrap;
  start(wrap, 0xFFFFFF00u);
  runner.expectEq(512u, elapsed(wrap, 0x100u), "the time passes the millis() overflow");

  Watch longRun;
  start(longRun, 0);
  runner.expectEq(kMaxMs, elapsed(longRun, kMaxMs + 5000), "the time stops at kMaxMs");
  stop(longRun, kMaxMs + 5000);
  runner.expectEq(kMaxMs, elapsed(longRun, 0), "a stop at the limit keeps kMaxMs");
  runner.expectEqual("99:59:59", text(kMaxMs), "the longest time");

  Watch late;
  start(late, 0);
  runner.expectFalse(lap(late, kMaxMs), "the watch takes no lap at kMaxMs");
  stop(late, 1000);
  start(late, 2000);
  runner.expectEq(kMaxMs, elapsed(late, 1000), "a long run gives kMaxMs, also when the sum overflows");

  reset(w);
  runner.expectTrue(!w.running && elapsed(w, 20000) == 0 && w.lapCount == 0, "reset clears the time and the laps");

  runner.expectEqual("00:00.00", text(0), "zero");
  runner.expectEqual("01:23.45", text(83456), "minutes, seconds and hundredths");
  runner.expectEqual("59:59.99", text(3599999), "the last time below one hour");
  runner.expectEqual("1:00:00", text(3600000), "one hour");
  runner.expectEqual("12:02:03", text(43323000), "hours, minutes and seconds");

  return runner.exitCode();
}
