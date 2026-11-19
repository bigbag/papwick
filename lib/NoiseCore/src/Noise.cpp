#include "Noise.h"

#include <algorithm>
#include <cmath>

namespace noise {

namespace {

// b0, b1, b2, a1, a2 of each section. The gain at 1 kHz is 0 dB.
constexpr float kSections[3][5] = {
    {0.529092774f, 1.05818555f, 0.529092774f, 0.821563818f, 0.168741777f},
    {1, -2, 1, -1.70550963f, 0.715987575f},
    {1, -2, 1, -1.98388676f, 0.983951666f},
};
// The weight of a new squared sample for the 125 ms time constant.
const float kFast = 1 - std::exp(-1.0f / (0.125f * kRate));

}  // namespace

float AFilter::process(float x) {
  // Transposed direct form II.
  for (int i = 0; i < 3; ++i) {
    const float* k = kSections[i];
    float* z = state_[i];
    const float y = k[0] * x + z[0];
    z[0] = k[1] * x - k[3] * y + z[1];
    z[1] = k[2] * x - k[4] * y;
    x = y;
  }
  return x;
}

bool Meter::add(const int16_t* samples, int count) {
  bool clipped = false;
  for (int i = 0; i < count; ++i) {
    clipped = clipped || samples[i] == INT16_MAX || samples[i] == INT16_MIN;
    const float y = filter_.process(samples[i]);
    const float square = y * y;
    fast_ += (square - fast_) * kFast;
    max_ = std::max(max_, fast_);
    sum_ += square;
  }
  count_ += count;
  return clipped;
}

void Meter::reset() {
  max_ = fast_;
  sum_ = 0;
  count_ = 0;
}

void Meter::clearLevel() {
  filter_ = {};
  fast_ = 0;
}

float Meter::level() const { return decibels(fast_); }

float Meter::max() const { return decibels(max_); }

float Meter::average() const { return decibels(count_ ? sum_ / count_ : 0); }

float decibels(double meanSquare) {
  constexpr double kFullScale = 32768.0 * 32768.0;
  return kFullScaleDb + 10 * static_cast<float>(std::log10(meanSquare / kFullScale + 1e-12));
}

}  // namespace noise
