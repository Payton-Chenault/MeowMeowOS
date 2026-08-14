#ifndef PIT_H
#define PIT_H

#include <stdint.h>

#define PIT_COMMAND_PORT 0X43
#define PIT_CHANNEL2_PORT 0x42
#define PIT_CHANNEL1_PORT 0x41
#define PIT_CHANNEL0_PORT 0X40

#define SPEAKER_PORT 0x61

void pit_initialize(uint32_t frequency);

uint32_t get_ticks();
uint32_t get_system_freq();

#endif