#include <cmath>
#include <string>
#include <vector>

#include "Noise.h"
#include "test_utils.h"

namespace noise {

struct MeterProbe {
  static void seed(Meter& m, uint64_t count, double sum) {
    m.count_ = count;
    m.sum_ = sum;
  }
};

}  // namespace noise

using namespace noise;

namespace {

constexpr float kPi = 3.14159265f;

std::vector<int16_t> tone(float hz, float amplitude, float seconds) {
  std::vector<int16_t> s(static_cast<size_t>(seconds * kRate));
  for (size_t i = 0; i < s.size(); ++i) s[i] = static_cast<int16_t>(amplitude * std::sin(2 * kPi * hz * i / kRate));
  return s;
}

// The gain of the A filter in dB for a tone, after the filter settles.
float gain(float hz) {
  AFilter f;
  const std::vector<int16_t> s = tone(hz, 10000, 1);
  double in = 0, out = 0;
  for (size_t i = 0; i < s.size(); ++i) {
    const float y = f.process(s[i]);
    if (i < s.size() / 2) continue;
    in += double(s[i]) * s[i];
    out += double(y) * y;
  }
  return 10 * static_cast<float>(std::log10(out / in));
}

}  // namespace

int main() {
  TestUtils::TestRunner runner("Noise");

  // The IEC 61672 values from the analog formula.
  runner.expectNear(-19.145, gain(100), 0.1, "A weighting at 100 Hz");
  runner.expectNear(-3.248, gain(500), 0.1, "A weighting at 500 Hz");
  runner.expectNear(0.0, gain(1000), 0.1, "A weighting at 1 kHz");
  runner.expectNear(1.202, gain(2000), 0.1, "A weighting at 2 kHz");

  Meter m;
  runner.expectTrue(m.empty(), "a new meter has no average");
  const std::vector<int16_t> loud = tone(1000, 32767, 1);
  runner.expectTrue(m.add(loud.data(), static_cast<int>(loud.size())), "a full-scale tone is an overload");
  runner.expectTrue(!m.empty(), "the meter has an average after samples");
  runner.expectNear(kFullScaleDb - 3.01, m.level(), 0.1, "a full-scale sine is 3 dB below a full-scale square");
  runner.expectNear(m.level(), m.average(), 0.1, "the average of a steady tone is its level");

  Meter half;
  const std::vector<int16_t> soft = tone(1000, 16384, 1);
  runner.expectTrue(!half.add(soft.data(), static_cast<int>(soft.size())), "half the amplitude is no overload");
  runner.expectNear(m.level() - 6.02, half.level(), 0.1, "half the amplitude is 6 dB less");

  // The fast weighting reaches 1 - 1/e of the power, which is 2 dB less, after 125 ms.
  Meter step;
  step.add(loud.data(), kRate / 8);
  runner.expectNear(m.level() - 2.0, step.level(), 0.3, "fast weighting after 125 ms");

  // The fast level falls by 34.7 dB in each second.
  const std::vector<int16_t> quiet(3 * kRate, 0);
  m.add(quiet.data(), static_cast<int>(quiet.size()));
  runner.expectTrue(m.level() < kMinDb, "the level falls below the display range in 3 s of silence");
  runner.expectNear(kFullScaleDb - 3.01, m.max(), 0.1, "the maximum stays after the tone");
  runner.expectNear(kFullScaleDb - 9.03, m.average(), 0.1, "a tone for a quarter of the time is 6 dB less on average");

  m.reset();
  runner.expectTrue(m.max() < kMinDb, "reset clears the maximum");
  runner.expectTrue(m.empty(), "reset clears the average");
  m.add(soft.data(), static_cast<int>(soft.size()));
  runner.expectNear(half.level(), m.average(), 0.1, "the average starts again after the reset");

  Meter during;
  during.add(soft.data(), static_cast<int>(soft.size()));
  during.reset();
  runner.expectNear(half.level(), during.level(), 0.01, "reset keeps the level of a tone");
  runner.expectNear(half.level(), during.max(), 0.01, "the maximum starts at the level");
  during.add(soft.data(), kRate / 10);
  runner.expectNear(half.level(), during.level(), 0.1, "the level continues after the reset");

  Meter capture;
  capture.add(soft.data(), static_cast<int>(soft.size()));
  capture.clearLevel();
  runner.expectTrue(capture.level() < kMinDb, "clearLevel clears the level");
  runner.expectNear(half.max(), capture.max(), 0.01, "clearLevel keeps the maximum");
  runner.expectNear(half.average(), capture.average(), 0.01, "clearLevel keeps the average");

  Meter longRun;
  const uint64_t count = 4294967040ULL;
  MeterProbe::seed(longRun, count, static_cast<double>(count) * 32768.0 * 32768.0);
  const int16_t frame[320] = {};
  longRun.add(frame, 320);
  runner.expectNear(kFullScaleDb, longRun.average(), 0.01, "the average stays stable past 2^32 samples");
  longRun.reset();
  longRun.add(soft.data(), static_cast<int>(soft.size()));
  runner.expectNear(half.average(), longRun.average(), 0.1, "reset clears a long session");

  return runner.exitCode();
}
