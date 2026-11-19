#pragma once

#include <cstdint>

// The sound level meter without hardware: the A frequency weighting, the time weighting and the levels in dB.
namespace noise {

constexpr int kRate = 16000;
// The level of a full-scale square wave in dB SPL. It comes from the codec gain of +27 dB, the full scale of the
// differential codec input of 2 Vrms (2 x AVDD / 3.3) and a typical MEMS microphone sensitivity of -38 dBV/Pa.
// A reference meter gives a better value.
constexpr float kFullScaleDb = 114;
// The range of the display.
constexpr float kMinDb = 30, kMaxDb = 110;

// The A weighting of IEC 61672 at 16 kHz: three biquad sections from the bilinear transform. It agrees with the
// standard within 0.1 dB up to 2 kHz. Above 4 kHz it falls faster than the standard.
class AFilter {
 public:
  float process(float x);

 private:
  float state_[3][2] = {};
};

// All levels are in dB(A). The level uses the fast time weighting of 125 ms.
class Meter {
 public:
  // Returns true when a sample is at full scale. Then the input is overloaded, and the level is too low.
  bool add(const int16_t* samples, int count);
  // Starts the maximum and the average again. The maximum starts at the level.
  void reset();
  // Clears the level and the filter for a new capture. The maximum and the average stay.
  void clearLevel();
  // True when no samples came after the reset.
  bool empty() const { return count_ == 0; }
  float level() const;
  float max() const;
  // The equivalent continuous level since the reset.
  float average() const;

 private:
  // A host test sets a long session without three days of audio.
  friend struct MeterProbe;
  AFilter filter_;
  float fast_ = 0, max_ = 0;
  double sum_ = 0;
  uint64_t count_ = 0;
};

// Converts a mean square of 16-bit samples to dB.
float decibels(double meanSquare);

}  // namespace noise
