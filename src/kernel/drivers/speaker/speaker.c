#include "speaker.h"

#include "../ports/IO.h"

void play_sound(uint32_t nFreq) {
    if (nFreq == 0) {
        return;
    }

    uint32_t divisor = 1193180 / nFreq;

    outb(0x43, 0xB6);
    outb(0x42, (uint8_t)(divisor & 0xFF));
    outb(0x42, (uint8_t)((divisor >> 8) & 0xFF));

    uint8_t tmp = inb(0x61);
    if (tmp != (tmp | 3)) {
        outb(0x61, tmp | 3);
    }
}

void no_sound() {
    uint8_t tmp = inb(0x61) & 0xFC;
    outb(0x61, tmp);
}

