#ifndef AC97_H
#define AC97_H

#include <stdint.h>
#include <stdbool.h>

#define AC97_MAX_BDL_ENTRIES 8

typedef struct {
    uint32_t buffer_phys;
    uint16_t samples; // Number of 16-bit samples (words)
    uint16_t flags;   // Bit 15: IOC, Bit 14: BUP
} __attribute__((packed)) ac97_bdl_entry_t;

void ac97_initialize(void);
bool ac97_is_present(void);
int ac97_play_pcm(const uint8_t *pcm_data, uint32_t length, uint32_t sample_rate, uint8_t channels, uint8_t bits_per_sample);
bool ac97_isr(void);

#endif