#ifndef SPEAKER_H
#define SPEAKER_H

#include "../../utils/pit/pit.h"
#include "../ports/IO.h"

void play_sound(uint32_t nFrequency);
void no_sound(void);
void beep(uint32_t freq, uint32_t duration_ms);
#endif