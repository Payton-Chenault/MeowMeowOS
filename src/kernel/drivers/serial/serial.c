#include "serial.h"

#include "../ports/IO.h"
#include <stdint.h>

#define MODULE "SERIAL"

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
  while (is_transmit_empty() == 0)
    ;
  outb(COM1, c);
}

void serial_write(const char *c) {
  for (int i = 0; c[i] != '\0'; i++) {
    serial_put_char(c[i]);
  }
}