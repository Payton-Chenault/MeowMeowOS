#ifndef KCONSOLE_H
#define KCONSOLE_H

#include "../../intf/vga_display/vga.h"

void kscreen_initialize(void);
void kput_char(char c);
void kprint(const char* c);
void kprintln(const char* c);
void kclear_screen(void);
void kbackspace(void);
#endif