#include "kconsole.h"
#include <stddef.h>
#include <stdint.h>

#include "../../drivers/vga_display/vga.h"
#include "../../fs/vfs/vfs.h"

#define MODULE "KERNEL_CONSOLE"
void kprint(const char *c) { terminal_print(c); }

void kput_char(char c) { terminal_putchar(c); }

void kclear_screen(void) { terminal_clear(); }

void kscreen_initialize() {
  terminal_initialize();
  log_info(MODULE, "Initialized");
}

void kbackspace() { terminal_backspace(); }

size_t kconsole_read_line(char *buffer, size_t size) {
  if (buffer == NULL || size == 0) {
    return 0;
  }

  vfs_node_t *stdin = vfs_find("stdin");
  if (!stdin) {
    log_error(MODULE, "FAILED: stdin not found in VFS");
    return 0;
  }

  size_t index = 0;
  char c;

  while (index < size - 1) {
    if (vfs_read(stdin, 0, 1, (uint8_t *)&c)) {
      if (c == '\n' || c == '\r') {
        kput_char('\n');
        buffer[index] = '\0';
        return index;
      } else if (c == '\b' || c == 0x7F) {
        if (index > 0) {
          index--;
          kbackspace();
        }
      } else if ((c >= 0x20 && c <= 0x7E)) {
        buffer[index] = c;
        kput_char(c);
        index++;
      }
    }
  }

  buffer[index] = '\0';
  return index;
}