#ifndef KPRINT_H
#define KPRINT_H

#include "../../intf/vga_display/vga.h"

void kinit_screen(void);
void kput_char(char c);
void kprint(const char* c);
void kprintln(const char* c);
void kclear_screen(void);
#endif