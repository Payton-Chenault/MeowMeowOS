#include "vga.h"


#define MODULE "VGA"
static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;

volatile uint16_t* terminal_buffer;
size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;

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
 * @note clears the terminal after initialization
 * 
 */
void terminal_initialize(void) {
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_buffer = (uint16_t*) 0xB8000; // Memory Pointer to Video Location

    terminal_clear();

    log_info(MODULE, "Initialized");
}

/**
 * @brief Clears the terminal
 * 
 */
void terminal_clear() {
    terminal_row = 0;
    terminal_column = 0;

    uint16_t blank = vga_entry(' ', terminal_color);
    
    for(size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        terminal_buffer[i] = blank;
    }

    update_cursor();
}

/**
 * @brief Scrolls the terminal upwards once the buffer is full
 * 
 */
void terminal_scroll(void) {
    for (size_t i = VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++) {
        terminal_buffer[i-VGA_WIDTH] = terminal_buffer[i];
    }

    size_t last_row_start = VGA_WIDTH * (VGA_HEIGHT - 1);
    for (size_t i = 0; i < VGA_WIDTH; i++) {
        terminal_buffer[last_row_start + i] = vga_entry(' ', terminal_color);
    }
}

/**
 * @brief Puts a char on the terminal
 * 
 * @param c the character to print on the terminal
 */
void terminal_putchar(char c) {
    if (c == '\r') {
        terminal_column = 0;
        update_cursor();
        return;
    }
    
    if (c == '\n') {
        terminal_column = 0;
        terminal_row++;
        if (terminal_row >= VGA_HEIGHT) {
            terminal_scroll();
            terminal_row = VGA_HEIGHT -1;
        }

        return;
    }

    if (c == '\t') {
        terminal_column = (terminal_column + 8) & ~7;
        if (terminal_column >= VGA_WIDTH) {
            terminal_putchar('\n');
        }
        return;
    }

    if (terminal_column == VGA_WIDTH - 1) {
        terminal_column = 0;
        terminal_row++;
        if (terminal_row >= VGA_HEIGHT) {
            terminal_scroll();
            terminal_row = VGA_HEIGHT - 1;
        }
    }

    const size_t index = terminal_row * VGA_WIDTH + terminal_column;
    terminal_buffer[index] = vga_entry(c, terminal_color);
    terminal_column++;

    if (terminal_column == VGA_WIDTH) {
        terminal_column = 0;
        terminal_row++;
    }

    update_cursor();
}

/**
 * @brief Prints a string of data to the terminal
 * 
 * @param data the data to print to the terminal
 */
void terminal_print(const char* data) {
    for (size_t i = 0; data[i] != '\0'; i++) {
        terminal_putchar(data[i]);
    }
}

/**
 * @brief Prints a string of data to the terminal, then skips to the next row
 * 
 * @param data the data to be printed
 */
void terminal_println(const char *data) {
    terminal_print(data);
    terminal_putchar('\n');
}

/**
 * @brief Deletes the prior ASCII printed to the terminal
 * 
 */
void terminal_backspace() {
    if(terminal_column == 0 && terminal_row == 0) return;

    if (terminal_column > 0) {
        terminal_column--;
    } else {
        terminal_row--;
        terminal_column = VGA_WIDTH - 1;
    }

    const size_t index = terminal_row * VGA_WIDTH + terminal_column;
    terminal_buffer[index] = vga_entry(' ', terminal_color);
    update_cursor();
}

void update_cursor() {
    uint16_t pos = terminal_row * VGA_WIDTH + terminal_column;

    outb(0x3D4, 0x0F);
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    outb(0x3D4, 0x0E);
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}