#include "vga.h"

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;

uint16_t* terminal_buffer;
size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;

bool is_initialized = false;

/**
 * @brief Helper to combine the foreground and background colors into one byte
 * 
 * @param fg the foreground character
 * @param bg the background character
 * @return uint8_t the combined byte
 */
static inline uint8_t vga_entry_color(enum VGA_COLOR fg, enum VGA_COLOR bg) {
    return fg | (bg << 4);
}

/**
 * @brief Helper to combine the character and color byte into a 16-bit int
 * 
 * @param uc the character to combine
 * @param color the character's color
 * @return uint16_t the combined character and color integer
 */
static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
    return (uint16_t) uc | ((uint16_t) color << 8);
}

/**
 * @brief initializes the terminal to be printed on
 * 
 */
void terminal_initialize(void) {
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_buffer = (uint16_t*) 0xB8000; // Memory Pointer to Video Location
    terminal_clear();

    is_initialized = true;
}

/**
 * @brief Clears the terminal
 * 
 */
void terminal_clear() {
    if(!is_initialized){
        return;
    }

    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            terminal_buffer[index] = vga_entry(' ', terminal_color);
        }
    }
}

void terminal_putchar(char c) {
    if(!is_initialized){
        return;
    }

    if (c == '\n') {
        terminal_column = 0;
        terminal_row++;
        // TODO: Implement Scrolling?
        return;
    }

    const size_t index = terminal_row * VGA_WIDTH + terminal_column;

    terminal_buffer[index] = vga_entry(c, terminal_color);

    terminal_column++;
    if (terminal_column == VGA_WIDTH) {
        terminal_column = 0;
        terminal_row++;
    }
}

void terminal_print(const char* data) {
    if(!is_initialized){
        return;
    }
    for (size_t i = 0; data[i] != '\0'; i++) {
        terminal_putchar(data[i]);
    }
}

void terminal_println(const char *data) {
    if(!is_initialized){
        return;
    }
    terminal_print(data);
    terminal_putchar('\n');
}