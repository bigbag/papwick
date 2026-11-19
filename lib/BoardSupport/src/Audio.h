#pragma once

#include <cstdint>

namespace board {

// Volume 0 mutes all tones.
void setVolume(uint8_t volume);
void setVibration(bool on);
// Queues a tone. A pause of gapMs follows it.
void beep(uint16_t hz, uint16_t ms, uint16_t gapMs = 40);
void vibrate(uint16_t onMs, uint16_t offMs = 0, uint8_t pulses = 1);
// Starts the queued tones and ends the vibration pulses. Call it in each loop.
void updateAudio();
void stopAudio();

// The speaker starts for a tone or a voice chunk. It ends after 3 s of silence. Volume 0 mutes only the tones.
// The speaker and the microphone share the ES8311 codec and the I2S clocks. micBegin() stops the speaker.
// Tones stay muted while the microphone runs.
bool micBegin();
// Records 16 ms of sound and returns the peak level 0..32767, or -1 when the microphone is off.
int micLevel();
void micEnd();

// Push-to-talk sound: 16 kHz mono. The microphone gives 20 ms frames. The speaker takes chunks of 3 frames.
constexpr int kVoiceRate = 16000;
constexpr int kVoiceFrame = 320;
constexpr int kVoiceChunk = 3 * kVoiceFrame;
// M5Unified multiplies the samples by gain / 2. kUnityGain keeps the codec level. The board default 16 clips a voice
// near the microphone.
constexpr uint8_t kUnityGain = 2, kVoiceGain = 8;
// Starts the microphone. Its task puts each frame in a queue of 8 frames. Then it records the next frame at once.
bool voiceCaptureBegin(uint8_t gain = kVoiceGain);
// Reads one frame from the queue. Returns false when the queue is empty.
bool voiceRead(int16_t* frame);
void voiceCaptureEnd();
// The volume setting does not change the voice. The voice always plays at full volume.
// Returns true when the speaker can take a chunk now. Always false while the microphone runs.
bool voiceCanPlay();
// Starts the speaker when necessary and queues a copy of the chunk. Returns false when the speaker refuses it.
bool voicePlay(const int16_t* chunk);
// True when the speaker played all voice chunks.
bool voiceIdle();
// Plays hz, then hz2 when it is not 0, each for ms. The volume setting does not change it. No sound while the
// microphone runs.
void voiceTone(uint16_t hz, uint16_t hz2, uint16_t ms);
// Turns the microphone off and stops the voice and its tones. Then only the tones that follow the volume setting stay.
void voiceStop();

}  // namespace board
