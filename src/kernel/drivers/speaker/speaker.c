#include "speaker.h"
#include "../../arch/x86/sync/spinlock.h"
#include "../ports/IO.h"
#include <stdint.h>

static spinlock_t speaker_lock = SPINLOCK_INIT;

void play_sound(uint32_t nFreq) {
  if (nFreq == 0) return;

  uint32_t divisor = 1193180 / nFreq;
  uint32_t flags = spinlock_acquire_irq_save(&speaker_lock);

  outb(0x43, 0xB6);
  outb(0x42, (uint8_t)(divisor & 0xFF));
  outb(0x42, (uint8_t)((divisor >> 8) & 0xFF));

  uint8_t tmp = inb(0x61);
  if (tmp != (tmp | 3)) {
    outb(0x61, tmp | 3);
  }

  spinlock_release_irq_restore(&speaker_lock, flags);
}

void no_sound() {
  uint32_t flags = spinlock_acquire_irq_save(&speaker_lock);
  uint8_t tmp = inb(0x61) & 0xFC;
  outb(0x61, tmp);
  spinlock_release_irq_restore(&speaker_lock, flags);
}