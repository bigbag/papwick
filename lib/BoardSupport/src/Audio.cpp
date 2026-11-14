#include "Audio.h"

#include <Logging.h>
#include <M5Unified.h>

#include <algorithm>
#include <cstdlib>

namespace board {

namespace {

constexpr const char* TAG = "AUDIO";
constexpr int kChannel = 0;
constexpr int kQueueSize = 8;
constexpr uint8_t kMotorLevel = 160;
// 256 samples at 16 kHz take 16 ms.
constexpr uint32_t kMicWaitMs = 100;
// The speaker powers the PA and the codec DAC. They stay on until the speaker ends.
constexpr uint32_t kSpeakerIdleMs = 3000;

struct Tone {
  uint16_t hz, ms, gapMs;
};

Tone queue[kQueueSize];
int head = 0, size = 0;
uint32_t nextToneAt = 0, speakerOffAt = 0;
bool muted = false, vibrationOn = true, micOn = false;
int16_t micBuf[256];

uint8_t pulsesLeft = 0;
bool motorOn = false;
uint16_t pulseOnMs = 0, pulseOffMs = 0;
uint32_t pulseUntil = 0;

bool due(uint32_t at, uint32_t now) { return static_cast<int32_t>(now - at) >= 0; }

}  // namespace

void setVolume(uint8_t volume) {
  muted = volume == 0;
  M5.Speaker.setVolume(volume);
}

void setVibration(bool on) {
  vibrationOn = on;
  if (!on) {
    pulsesLeft = 0;
    if (motorOn) M5.Power.setVibration(0);
    motorOn = false;
  }
}

void beep(uint16_t hz, uint16_t ms, uint16_t gapMs) {
  if (muted || micOn) return;
  if (size == kQueueSize) {
    LOG_WRN(TAG, "Queue full, drop %u Hz", hz);
    return;
  }
  queue[(head + size++) % kQueueSize] = {hz, ms, gapMs};
}

void vibrate(uint16_t onMs, uint16_t offMs, uint8_t pulses) {
  if (!vibrationOn) return;
  pulseOnMs = onMs;
  pulseOffMs = offMs;
  pulsesLeft = pulses;
  if (motorOn) M5.Power.setVibration(0);
  motorOn = false;
  pulseUntil = 0;
}

void updateAudio() {
  const uint32_t now = millis();
  if (size > 0 && due(nextToneAt, now)) {
    if (!M5.Speaker.isRunning() && !M5.Speaker.begin()) {
      LOG_ERR(TAG, "Speaker start failed");
      size = 0;
    } else {
      const Tone& t = queue[head];
      M5.Speaker.tone(t.hz, t.ms, kChannel);
      nextToneAt = now + t.ms + t.gapMs;
      speakerOffAt = nextToneAt + kSpeakerIdleMs;
      head = (head + 1) % kQueueSize;
      --size;
    }
  } else if (size == 0 && due(nextToneAt, now)) {
    // An old deadline looks like a future one after 24.8 days.
    nextToneAt = now;
    if (M5.Speaker.isRunning() && !M5.Speaker.isPlaying() && due(speakerOffAt, now)) M5.Speaker.end();
  }

  if (pulseUntil != 0 && !due(pulseUntil, now)) return;
  if (motorOn) {
    M5.Power.setVibration(0);
    motorOn = false;
    pulseUntil = pulsesLeft > 0 ? now + pulseOffMs : 0;
  } else if (pulsesLeft > 0) {
    M5.Power.setVibration(kMotorLevel);
    motorOn = true;
    --pulsesLeft;
    pulseUntil = now + pulseOnMs;
  } else {
    pulseUntil = 0;
  }
}

void stopAudio() {
  size = 0;
  nextToneAt = millis();
  M5.Speaker.stop(kChannel);
  pulsesLeft = 0;
  if (motorOn) M5.Power.setVibration(0);
  motorOn = false;
  pulseUntil = 0;
}

bool micBegin() {
  if (micOn) return true;
  stopAudio();
  M5.Speaker.end();
  micOn = M5.Mic.begin();
  if (!micOn) LOG_ERR(TAG, "Mic start failed");
  return micOn;
}

int micLevel() {
  if (!micOn || !M5.Mic.record(micBuf, sizeof(micBuf) / sizeof(*micBuf), 16000)) return -1;
  const uint32_t start = millis();
  while (M5.Mic.isRecording()) {
    if (millis() - start > kMicWaitMs) {
      LOG_ERR(TAG, "Mic record timeout");
      micEnd();
      return -1;
    }
    delay(1);
  }
  int peak = 0;
  for (const int16_t v : micBuf) peak = std::max(peak, std::abs(static_cast<int>(v)));
  return peak;
}

void micEnd() {
  if (!micOn) return;
  M5.Mic.end();
  micOn = false;
}

}  // namespace board
