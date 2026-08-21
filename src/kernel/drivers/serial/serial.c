#include "serial.h"
#include "../../arch/x86/sync/spinlock.h"
#include "../ports/IO.h"
#include <stdint.h>

#define MODULE "SERIAL"

static spinlock_t serial_lock = SPINLOCK_INIT;

void serial_initialize() {
  outb(COM1 + 1, 0x00);
  outb(COM1 + 3, 0x80);
  outb(COM1 + 0, 0x03);
  outb(COM1 + 1, 0x00);
  outb(COM1 + 3, 0x03);
  outb(COM1 + 2, 0xC7);
  outb(COM1 + 4, 0x0B);
}

int serial_received() { return inb(COM1 + 5) & 1; }

int is_transmit_empty() { return inb(COM1 + 5) & 0x20; }

void serial_put_char(char c) {
  uint32_t flags = spinlock_acquire_irq_save(&serial_lock);

  for (volatile uint32_t i = 0; i < 100000; i++) {
    if (is_transmit_empty()) {
      outb(COM1, c);
      spinlock_release_irq_restore(&serial_lock, flags);
      return;
    }
  }

  spinlock_release_irq_restore(&serial_lock, flags);
}

void serial_write(const char *c) {
  if (c == NULL) return;
  for (int i = 0; c[i] != '\0'; i++) {
    serial_put_char(c[i]);
  }
}