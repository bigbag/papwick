#include "Audio.h"

#include <Logging.h>
#include <M5Unified.h>

#include <algorithm>
#include <atomic>
#include <cstdlib>
#include <cstring>

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
// Tones use channel 0, so stopAudio() does not cut the voice.
constexpr int kVoiceChannel = 1;
// The walkie-talkie tones are near the voice level.
constexpr int kCueChannel = 2;
constexpr uint8_t kCueVolume = 128;
constexpr uint32_t kVoiceChunkMs = kVoiceChunk * 1000 / kVoiceRate;
constexpr int kVoiceQueue = 8;
// The board default 16 clips a voice near the microphone.
constexpr uint8_t kVoiceGain = 8;

struct Tone {
  uint16_t hz, ms, gapMs;
};

Tone queue[kQueueSize];
int head = 0, size = 0;
uint32_t nextToneAt = 0, speakerOffAt = 0;
bool muted = false, vibrationOn = true, micOn = false;
int16_t micBuf[256];

QueueHandle_t voiceQueue = nullptr;
int16_t captureBuf[2][kVoiceFrame];
// The speaker holds 2 requests and reads from the buffers. The third buffer takes the next chunk.
int16_t playBuf[3][kVoiceChunk];
int playNext = 0;
uint8_t micGain = 0;
std::atomic<uint32_t> voiceDrops{0};

uint8_t pulsesLeft = 0;
bool motorOn = false;
uint16_t pulseOnMs = 0, pulseOffMs = 0;
uint32_t pulseUntil = 0;

bool due(uint32_t at, uint32_t now) { return static_cast<int32_t>(now - at) >= 0; }

// Runs in the microphone task. It must not block.
void onCapture(void*, void* data, size_t) {
  if (xQueueSend(voiceQueue, data, 0) != pdTRUE) ++voiceDrops;
  if (!M5.Mic.record(static_cast<int16_t*>(data), kVoiceFrame, kVoiceRate)) ++voiceDrops;
}

bool speakerOn() {
  if (M5.Speaker.isRunning() || M5.Speaker.begin()) return true;
  LOG_ERR(TAG, "Speaker start failed");
  return false;
}

}  // namespace

void setVolume(uint8_t volume) {
  muted = volume == 0;
  // The output level is master^2 * channel^2. Only the tone channel gets the volume, so the voice keeps its own.
  M5.Speaker.setVolume(255);
  M5.Speaker.setChannelVolume(kChannel, volume);
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
    if (!speakerOn()) {
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

bool voiceCaptureBegin() {
  if (!voiceQueue) voiceQueue = xQueueCreate(kVoiceQueue, sizeof(captureBuf[0]));
  if (!voiceQueue) {
    LOG_ERR(TAG, "Voice queue failed");
    return false;
  }
  xQueueReset(voiceQueue);
  voiceDrops = 0;
  // The gain and the callback must be set before the first record().
  auto cfg = M5.Mic.config();
  micGain = cfg.magnification;
  cfg.magnification = kVoiceGain;
  M5.Mic.config(cfg);
  M5.Mic.setBufferReleaseCallback(nullptr, onCapture);
  if (!micBegin()) {
    voiceCaptureEnd();
    return false;
  }
  for (auto& buf : captureBuf) {
    if (!M5.Mic.record(buf, kVoiceFrame, kVoiceRate)) {
      LOG_ERR(TAG, "Voice record failed");
      voiceCaptureEnd();
      return false;
    }
  }
  return true;
}

bool voiceRead(int16_t* frame) { return voiceQueue && xQueueReceive(voiceQueue, frame, 0) == pdTRUE; }

void voiceCaptureEnd() {
  // The last record() fails while end() stops the task. That is not a drop.
  const uint32_t drops = voiceDrops;
  micEnd();
  // The callback and the gain can change only after end() returns.
  M5.Mic.setBufferReleaseCallback(nullptr, nullptr);
  if (micGain) {
    auto cfg = M5.Mic.config();
    cfg.magnification = micGain;
    M5.Mic.config(cfg);
    micGain = 0;
  }
  if (voiceQueue) xQueueReset(voiceQueue);
  if (drops > 0) LOG_WRN(TAG, "Voice: %u frames dropped", static_cast<unsigned>(drops));
  voiceDrops = 0;
}

bool voiceCanPlay() { return !micOn && M5.Speaker.isPlaying(kVoiceChannel) < 2; }

bool voicePlay(const int16_t* chunk) {
  if (!voiceCanPlay() || !speakerOn()) return false;
  int16_t* buf = playBuf[playNext];
  playNext = (playNext + 1) % 3;
  std::memcpy(buf, chunk, sizeof(playBuf[0]));
  if (!M5.Speaker.playRaw(buf, kVoiceChunk, kVoiceRate, false, 1, kVoiceChannel)) return false;
  speakerOffAt = millis() + 2 * kVoiceChunkMs + kSpeakerIdleMs;
  return true;
}

void voiceStop() {
  voiceCaptureEnd();
  M5.Speaker.stop(kVoiceChannel);
  M5.Speaker.stop(kCueChannel);
}

bool voiceIdle() { return M5.Speaker.isPlaying(kVoiceChannel) == 0; }

void voiceTone(uint16_t hz, uint16_t hz2, uint16_t ms) {
  if (micOn || !speakerOn()) return;
  M5.Speaker.setChannelVolume(kCueChannel, kCueVolume);
  M5.Speaker.tone(hz, ms, kCueChannel);
  if (hz2) M5.Speaker.tone(hz2, ms, kCueChannel, false);
  speakerOffAt = millis() + 2 * ms + kSpeakerIdleMs;
}

}  // namespace board
