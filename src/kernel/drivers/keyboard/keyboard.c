#include "keyboard.h"

#include "../../kernel_services/kernel_services.h"
#include "../ports/IO.h"
#include "../../arch/x86/interrupt_descriptor_table/idt.h"
#include "../../utils/logging/logger.h"

#define MODULE "KEYBOARD"

static volatile key_buffer_t* keyboard_buffer;

static uint8_t modifier_state = 0;
static uint8_t lock_state = 0;

static bool extended_scancode = false;

// Normal scancode to ASCII (without shift)
static const char scancode_to_ascii_normal[] = {
    0,    0,    '1',  '2',  '3',  '4',  '5',  '6',  '7',  '8',  '9',  '0',  '-',  '=',  '\b',    0,    
    'q',  'w',  'e',  'r',  't',  'y',  'u',  'i',  'o',  'p',  '[',  ']',  '\n',    0,    'a',  's', 
    'd',  'f',  'g',  'h',  'j',  'k',  'l',  ';',  '\'', '`',  0,    '\\', 'z',  'x',  'c',  'v', 
    'b',  'n',  'm',  ',',  '.',  '/',  0,    '*',  0,    ' ',  0,    0,    0,    0,    0,    0, 
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0 
};

// Shifted scancode to ASCII
static const char scancode_to_ascii_shift[] = {
    0,    0,    '!',  '@',  '#',  '$',  '%',  '^',  '&',  '*',  '(',  ')',  '_',  '+',  'b',    0, 
    'Q',  'W',  'E',  'R',  'T',  'Y',  'U',  'I',  'O',  'P',  '{',  '}',  '\n',    0,    'A',  'S',
    'D',  'F',  'G',  'H',  'J',  'K',  'L',  ':',  '"',  '~',  0,    '|',  'Z',  'X',  'C',  'V',
    'B',  'N',  'M',  '<',  '>',  '?',  0,    '*',  0,    ' ',  0,    0,    0,    0,    0,    0,
    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0,    0 
};

// Extended scancode mappings (for keys like arrows, home, end, etc.)
static const uint16_t extended_scancode_map[] = {
    0,        0,        0,        0,        0,        0,        0,        0,        
    0,        0,        0,        0,        0,        0,        0,        0,  
    0,        0,        0,        0,        0,        0,        0,        0,
    0,        0,        0,        0,        0,        0,        0,        0, 
    0,        0,        0,        0,        0,        0,        0,        0,
    0,        0,        0,        0,        0,        0,        0,        0,
    0,        0,        0,        0,        0,        0,        0,        0,
    0,        0,        0,        0,        0,        0,        0,        0,
    0,        0,        0,        0,        0,        0,        0,        0,  
    0,        0,        0,        0,        0,        0,        0,        0,    
    KEY_UP,   0,        0,        0,        KEY_LEFT, 0,        0,        0,     
    KEY_RIGHT,0,        0,        0,        KEY_DOWN, 0,        0,        0,
    0,        0,        0,        0,        0,        0,        0,        0,
    0,        0,        0,        0,        0,        0,        0,        0,
    0,        0,        0,        0,        0,        0,        0,        0,
    0,        0,        0,        0,        0,        0,        0,        0
};

/**
 * @brief Returns if the buffer is empty
 */
static bool keyboard_is_buffer_empty(void) {
    return keyboard_buffer->head == keyboard_buffer->tail;
}

/**
 * @brief Returns if the buffer is full
 */
static bool keyboard_is_buffer_full(void) {
    if(keyboard_buffer == NULL || keyboard_buffer->size == 0) return true;
    return (keyboard_buffer->head + 1) % keyboard_buffer->size == keyboard_buffer->tail;
}

/**
 * @brief Writes a scancode to the buffer
 */
static bool keyboard_buffer_write(uint8_t scancode) {
    if (keyboard_is_buffer_full()) {
        return false;
    }

    keyboard_buffer->data[keyboard_buffer->head] = scancode;
    keyboard_buffer->head = (keyboard_buffer->head + 1) % keyboard_buffer->size;
    return true;
}

/**
 * @brief Reads a scancode from the buffer
 */
static bool keyboard_buffer_read(uint8_t* scancode) {
    if(keyboard_is_buffer_empty()) {
        return false;
    }

    *scancode = keyboard_buffer->data[keyboard_buffer->tail];
    keyboard_buffer->tail = (keyboard_buffer->tail + 1) % keyboard_buffer->size;
    return true;
}

/**
 * @brief Updates the modifier map based on the provided scancode and if it is pressed
 */
static void update_modifier_state(uint8_t scancode, bool is_pressed) {
    switch (scancode) {
        case 0x2A: // Left Shift make
        case 0x36: // Right Shift make
            if (is_pressed) modifier_state |= MODIFIER_SHIFT;
            else modifier_state &= ~MODIFIER_SHIFT;
            break;
        case 0x1D: // Left Ctrl make
            if (is_pressed) modifier_state |= MODIFIER_CTRL;
            else modifier_state &= ~MODIFIER_CTRL;
            break;
        case 0x38: // Left Alt make
            if (is_pressed) modifier_state |= MODIFIER_ALT;
            else modifier_state &= ~MODIFIER_ALT;
            break;
        case 0x3A: // Caps Lock
            if (is_pressed) lock_state ^= MODIFIER_CAPS_LOCK;
            break;
        case 0x45: // Num Lock
            if (is_pressed) lock_state ^= MODIFIER_NUM_LOCK;
            break;
        case 0x46: // Scroll Lock
            if (is_pressed) lock_state ^= MODIFIER_SCROLL_LOCK;
            break;
    }
}

/**
 * @brief Returns if the scancode is a key release
 */
static bool is_break_code(uint8_t scancode) {
    return (scancode & 0x80) != 0 && (scancode & 0xFF00) == 0;
}

/**
 * @brief Gets the make code (removed break flag)
 */
static uint8_t get_make_code(uint8_t scancode) {
    return scancode & 0x7F;
}

/**
 * @brief Called by the keyboard interrupt handler
 */
bool keyboard_isr(void) {
    uint8_t scancode = inb(KEYBOARD_DATA_PORT);
    outb(0x20, 0x20);

    if (scancode == 0xE0) {
        extended_scancode = true;
        return false;
    }

    if (extended_scancode) {
        keyboard_buffer_write(scancode | 0x80);
        extended_scancode = false;
    } else {
        keyboard_buffer_write(scancode);
    }
    
    return false;
}

/**
 * @brief Initializes the keyboard handle
 */
void keyboard_initialize(void) {
    extended_scancode = false;
    modifier_state = 0;
    lock_state = 0;

    keyboard_buffer = (key_buffer_t*)kmem_zalloc(sizeof(key_buffer_t));
    keyboard_buffer->size = 256;
    keyboard_buffer->data = (uint8_t*)kmem_zalloc(keyboard_buffer->size);
    keyboard_buffer->head = 0;
    keyboard_buffer->tail = 0;

    register_interrupt_handler(KEYBOARD_INTERRUPT_VECTOR, keyboard_isr);
    keyboard_install_handler();

    log_info(MODULE, "Initialized");
}

/**
 * @brief Handles the interrupt enabling
 */
void keyboard_install_handler(void) {
    outb(0x21, inb(0x21) & 0xFD);   // unmask keyboard IRQ (bit 1)
}

/**
 * @brief Returns if the keyboard buffer has a key
 */
bool keyboard_has_key(void) {
    return !keyboard_is_buffer_empty();
}

/**
 * @brief Flushes the keyboard buffer
 */
void keyboard_flush_buffer() {
    keyboard_buffer->head = 0;
    keyboard_buffer->tail = 0;
}

/**
 * @brief Reads a scancode from the buffer (non-blocking)
 */
bool keyboard_read_scancode(uint8_t* scancode) {
    return keyboard_buffer_read(scancode);
}

/**
 * @brief Converts a scancode to a character
 */
char keyboard_scancode_to_char(uint8_t scancode) {
    if ((scancode & 0xFF00) == 0xE000) return 0;

    uint8_t make_code = scancode & 0x7F;
    if (is_break_code(scancode)) return 0;

    bool shift_active = (modifier_state & MODIFIER_SHIFT) != 0;
    bool caps_active = (lock_state & MODIFIER_CAPS_LOCK) != 0; 

    char result;
    if (make_code >= 0x10 && make_code <= 0x2F) {
        if (caps_active) {
            shift_active = !shift_active;
        }
    }

    if (shift_active && make_code < sizeof(scancode_to_ascii_shift)) {
        result = scancode_to_ascii_shift[make_code];
    } else if (make_code < sizeof(scancode_to_ascii_normal)) {
        result = scancode_to_ascii_normal[make_code];
    } else {
        result = 0;
    }

    return result;
}

/**
 * @brief Returns an extended make code
 */
uint16_t keyboard_scancode_to_extended(uint8_t scancode) {
    uint8_t make_code = get_make_code(scancode);

    bool is_extended = (scancode & 0x80) != 0;
    if(!is_extended) {
        return 0;
    }

    if (is_break_code(scancode)) {
        return 0;
    }

    return extended_scancode_map[make_code];
}

/**
 * @brief Reads a character from the buffer (blocking)
 */
char keyboard_read_char(void) {
    uint8_t scancode;
    char result;

    while (1) {
        // Disable interrupts before checking to avoid race
        __asm__ volatile("cli");
        if (keyboard_buffer_read(&scancode)) {
            __asm__ volatile("sti");
            break;
        }
        // No data: enable interrupts and wait for one
        __asm__ volatile("sti; hlt; cli");
    }

    uint8_t make_code = get_make_code(scancode);
    bool is_pressed = !is_break_code(scancode);
    update_modifier_state(make_code, is_pressed);

    result = keyboard_scancode_to_char(scancode);
    if (result != 0) {
        return result;
    }

    uint16_t extended = keyboard_scancode_to_extended(scancode);
    if (extended != 0) {
        // Extended key, ignore for now and continue waiting
        return keyboard_read_char();
    }

    // Unknown key, try again
    return keyboard_read_char();
}

/**
 * @brief Non-blocking character read
 */
char keyboard_read_char_nonblocking(void) {
    uint8_t scancode;

    if (!keyboard_buffer_read(&scancode)) {
        return 0;
    }

    uint8_t make_code = get_make_code(scancode);
    bool is_pressed = !is_break_code(scancode);
    update_modifier_state(make_code, is_pressed);

    return keyboard_scancode_to_char(scancode);
}

/**
 * @brief Returns a keycode (non-blocking)
 */
uint16_t keyboard_read_keycode(void) {
    uint8_t scancode;

    if (!keyboard_buffer_read(&scancode)) {
        return 0;
    }

    uint8_t make_code = get_make_code(scancode);
    bool is_pressed = !is_break_code(scancode);

    update_modifier_state(make_code, is_pressed);

    uint16_t extended = keyboard_scancode_to_extended(scancode); 
    if (extended != 0) {
        return extended; 
    }

    char ascii = keyboard_scancode_to_char(scancode);
    if (ascii != 0) {
        return (uint16_t)ascii;
    }

    return 0;
}

uint8_t keyboard_get_modifiers(void) {
    return modifier_state;
}

bool keyboard_is_modifier_active(uint8_t modifier) {
    return (modifier_state & modifier) != 0;
}

uint8_t keyboard_get_lock_state(void) {
    return lock_state;
}