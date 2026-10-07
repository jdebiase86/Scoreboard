// The board's little speaker: ES8311 sound chip (I2C 0x18, same bus as the
// wheel) fed over I2S, into an NS4150 amplifier switched on by IO3.
#pragma once

bool audioBegin();     // call once in setup(), after the wheel has opened the I2C bus
bool audioReady();     // false if the sound chip didn't answer
void audioChime();     // plays a short test chime at full volume (returns at once)
