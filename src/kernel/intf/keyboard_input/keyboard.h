#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "../ports/IO.h"
#include "../../utils//interrupt_descriptor_table//idt.h"
#include "../../implementations//console_print//kconsole.h"

#define KEYBOARD_BUFFER_SIZE     256
#define KEYBOARD_DATA_PORT       0x60
#define KEYBOARD_STATUS_PORT     0x64   

#define KEY_ENTER                '\n'
#define KEY_BACKSPACE            0x08
#define KEY_TAB                  0x09
#define KEY_ESCAPE               0x1B
#define KEY_DELETE               0x7F

// Extended key codes (non-ASCII)
#define KEY_UP                   0xE000
#define KEY_DOWN                 0xE001
#define KEY_LEFT                 0xE002
#define KEY_RIGHT                0xE003
#define KEY_HOME                 0xE004
#define KEY_END                  0xE005
#define KEY_PAGE_UP              0xE006
#define KEY_PAGE_DOWN            0xE007
#define KEY_INSERT               0xE008
#define KEY_F1                   0xE100
#define KEY_F2                   0xE200
#define KEY_F3                   0xE300
#define KEY_F4                   0xE400
#define KEY_F5                   0xE500
#define KEY_F6                   0xE600
#define KEY_F7                   0xE700
#define KEY_F8                   0xE800
#define KEY_F9                   0xE900
#define KEY_F10                  0xEA00
#define KEY_F11                  0xEB00
#define KEY_F12                  0xEC00

#define MODIFIER_SHIFT           0x01
#define MODIFIER_CTRL            0x02
#define MODIFIER_ALT             0x04
#define MODIFIER_CAPS_LOCK       0x08
#define MODIFIER_NUM_LOCK        0x10
#define MODIFIER_SCROLL_LOCK     0x20

void keyboard_initialize(void);
void keyboard_install_handler(void);

char keyboard_read_char(void);
char keyboard_read_char_nonblocking(void);

uint16_t keyboard_read_keycode(void);

size_t keyboard_read_line(char* buffer, size_t buffer_size);

bool keyboard_has_key(void);
void keyboard_flush_buffer(void);

uint8_t keyboard_get_modifiers(void);

bool keyboard_is_modifier_active(uint8_t modifier);

uint8_t keyboard_get_lock_state(void);

#endif