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
void fb_draw_bmp_file(const char *filename);

uint32_t kconsole_get_width(void);
uint32_t kconsole_get_height(void);
void kconsole_draw_mouse_cursor(int32_t x, int32_t y);
void kconsole_restore_mouse_cursor(void);

#endif