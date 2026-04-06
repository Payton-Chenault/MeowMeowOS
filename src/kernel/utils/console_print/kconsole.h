#ifndef KCONSOLE_H
#define KCONSOLE_H

#include "../../intf/vga_display/vga.h"
#include "../integer_ascii_converters/itoa.h"

void kscreen_initialize(void);
void kput_char(char c);
void kprint(const char* c);
void kprintln(const char* c);
void kclear_screen(void);
void kbackspace(void);
size_t kconsole_read_line(char* buffer, size_t size);
#endif