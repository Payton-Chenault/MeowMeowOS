#ifndef SERIAL_H
#define SERIAL_H

#include "../ports//IO.h"
#include <stdint.h>

#define COM1 0x3F8

void serial_initialize(void);
int serial_received(void);
int is_transmit_empty(void);
void serial_put_char(char c);
void serial_write(const char* c);

#endif