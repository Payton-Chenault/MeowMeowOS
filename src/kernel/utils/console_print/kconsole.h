#ifndef KCONSOLE_H
#define KCONSOLE_H

#include <stddef.h>
#include <stdint.h>

void kprint(const char *c);
void kprintln(const char *c);
void kput_char(char c);
void kclear_screen(void);
void kscreen_initialize(void);
void kbackspace(void);
size_t kconsole_read_line(char *buffer, size_t size);
void kscreen_timer_tick(void);
void fb_draw_bmp_file(const char *filename, uint32_t start_x, uint32_t start_y);
#endif