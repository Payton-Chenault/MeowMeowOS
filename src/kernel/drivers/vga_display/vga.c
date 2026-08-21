#include "vga.h"
#include "../../arch/x86/sync/spinlock.h"
#include "../../utils/logging/logger.h"

#define MODULE "VGA"

static const size_t VGA_WIDTH = 80;
static const size_t VGA_HEIGHT = 25;

volatile uint16_t *terminal_buffer;
size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;

static spinlock_t vga_lock = SPINLOCK_INIT;

static inline uint8_t vga_entry_color(enum VGA_COLOR fg, enum VGA_COLOR bg) {
  return fg | (bg << 4);
}

static inline uint16_t vga_entry(unsigned char uc, uint8_t color) {
  return (uint16_t)uc | ((uint16_t)color << 8);
}

void terminal_initialize(void) {
  uint32_t flags = spinlock_acquire_irq_save(&vga_lock);

  terminal_row = 0;
  terminal_column = 0;
  terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
  terminal_buffer = (uint16_t *)0xB8000;

  uint16_t blank = vga_entry(' ', terminal_color);
  for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
    terminal_buffer[i] = blank;
  }

  update_cursor();
  spinlock_release_irq_restore(&vga_lock, flags);

  log_info(MODULE, "Initialized");
}

void terminal_clear() {
  uint32_t flags = spinlock_acquire_irq_save(&vga_lock);

  terminal_row = 0;
  terminal_column = 0;

  uint16_t blank = vga_entry(' ', terminal_color);
  for (size_t i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
    terminal_buffer[i] = blank;
  }

  update_cursor();
  spinlock_release_irq_restore(&vga_lock, flags);
}

static void terminal_scroll_locked(void) {
  for (size_t i = VGA_WIDTH; i < VGA_HEIGHT * VGA_WIDTH; i++) {
    terminal_buffer[i - VGA_WIDTH] = terminal_buffer[i];
  }

  size_t last_row_start = VGA_WIDTH * (VGA_HEIGHT - 1);
  for (size_t i = 0; i < VGA_WIDTH; i++) {
    terminal_buffer[last_row_start + i] = vga_entry(' ', terminal_color);
  }
}

void terminal_putchar(char c) {
  uint32_t flags = spinlock_acquire_irq_save(&vga_lock);

  if (c == '\r') {
    terminal_column = 0;
    update_cursor();
    spinlock_release_irq_restore(&vga_lock, flags);
    return;
  }

  if (c == '\n') {
    terminal_column = 0;
    terminal_row++;

    if (terminal_row >= VGA_HEIGHT) {
      terminal_scroll_locked();
      terminal_row = VGA_HEIGHT - 1;
    }

    update_cursor();
    spinlock_release_irq_restore(&vga_lock, flags);
    return;
  }

  if (c == '\t') {
    terminal_column = (terminal_column + 8) & ~7;

    if (terminal_column >= VGA_WIDTH) {
      terminal_column = 0;
      terminal_row++;

      if (terminal_row >= VGA_HEIGHT) {
        terminal_scroll_locked();
        terminal_row = VGA_HEIGHT - 1;
      }
    }

    update_cursor();
    spinlock_release_irq_restore(&vga_lock, flags);
    return;
  }

  if (terminal_column == VGA_WIDTH - 1) {
    terminal_column = 0;
    terminal_row++;

    if (terminal_row >= VGA_HEIGHT) {
      terminal_scroll_locked();
      terminal_row = VGA_HEIGHT - 1;
    }
  }

  const size_t index = terminal_row * VGA_WIDTH + terminal_column;
  terminal_buffer[index] = vga_entry(c, terminal_color);
  terminal_column++;

  if (terminal_column == VGA_WIDTH) {
    terminal_column = 0;
    terminal_row++;

    if (terminal_row >= VGA_HEIGHT) {
      terminal_scroll_locked();
      terminal_row = VGA_HEIGHT - 1;
    }
  }

  update_cursor();
  spinlock_release_irq_restore(&vga_lock, flags);
}

void terminal_print(const char *data) {
  if (data == NULL) {
    return;
  }

  uint32_t flags = spinlock_acquire_irq_save(&vga_lock);

  for (size_t i = 0; data[i] != '\0'; i++) {
    char c = data[i];

    if (c == '\r') {
      terminal_column = 0;
      continue;
    }

    if (c == '\n') {
      terminal_column = 0;
      terminal_row++;

      if (terminal_row >= VGA_HEIGHT) {
        terminal_scroll_locked();
        terminal_row = VGA_HEIGHT - 1;
      }

      continue;
    }

    if (c == '\t') {
      terminal_column = (terminal_column + 8) & ~7;

      if (terminal_column >= VGA_WIDTH) {
        terminal_column = 0;
        terminal_row++;

        if (terminal_row >= VGA_HEIGHT) {
          terminal_scroll_locked();
          terminal_row = VGA_HEIGHT - 1;
        }
      }

      continue;
    }

    if (terminal_column == VGA_WIDTH - 1) {
      terminal_column = 0;
      terminal_row++;

      if (terminal_row >= VGA_HEIGHT) {
        terminal_scroll_locked();
        terminal_row = VGA_HEIGHT - 1;
      }
    }

    const size_t index = terminal_row * VGA_WIDTH + terminal_column;
    terminal_buffer[index] = vga_entry(c, terminal_color);
    terminal_column++;

    if (terminal_column == VGA_WIDTH) {
      terminal_column = 0;
      terminal_row++;

      if (terminal_row >= VGA_HEIGHT) {
        terminal_scroll_locked();
        terminal_row = VGA_HEIGHT - 1;
      }
    }
  }

  update_cursor();
  spinlock_release_irq_restore(&vga_lock, flags);
}

void terminal_println(const char *data) {
  terminal_print(data);
  terminal_putchar('\n');
}

void terminal_backspace() {
  uint32_t flags = spinlock_acquire_irq_save(&vga_lock);

  if (terminal_column == 0 && terminal_row == 0) {
    spinlock_release_irq_restore(&vga_lock, flags);
    return;
  }

  if (terminal_column > 0) {
    terminal_column--;
  } else {
    terminal_row--;
    terminal_column = VGA_WIDTH - 1;
  }

  const size_t index = terminal_row * VGA_WIDTH + terminal_column;
  terminal_buffer[index] = vga_entry(' ', terminal_color);
  update_cursor();

  spinlock_release_irq_restore(&vga_lock, flags);
}

void update_cursor() {
  uint16_t pos = terminal_row * VGA_WIDTH + terminal_column;

  outb(0x3D4, 0x0F);
  outb(0x3D5, (uint8_t)(pos & 0xFF));
  outb(0x3D4, 0x0E);
  outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}