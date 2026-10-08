// The board's little speaker: ES8311 sound chip (I2C 0x18, same bus as the
// wheel) fed over I2S, into an NS4150 amplifier switched on by IO3.
#pragma once
#include "sb_sounds.h"

bool audioBegin();          // call once in setup(), after the wheel has opened the I2C bus
bool audioReady();          // false if the sound chip didn't answer
void audioChime();          // the settings page's test chime (plays even with sound off)
// One of Joe's sounds (sb_sounds.h). Skipped when sound is off; a new one
// waits for the one playing to finish (at most two waiting).
void audioPlay(SoundId id);
void audioPlayAlways(SoundId id);   // the settings page's sampler: ignores sound off
