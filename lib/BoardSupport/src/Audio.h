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

// The speaker starts for a tone and ends after 3 s of silence. Volume 0 keeps it off.
// The speaker and the microphone share the ES8311 codec and the I2S clocks. micBegin() stops the speaker.
// Tones stay muted while the microphone runs.
bool micBegin();
// Records 16 ms of sound and returns the peak level 0..32767, or -1 when the microphone is off.
int micLevel();
void micEnd();

}  // namespace board
