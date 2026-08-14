#include "kernel_services.h"
#include "../arch/x86/pit/pit.h"
#include "../drivers/disk/ata.h"
#include "../fs/vfs/vfs.h"
#include "../lib/integer_ascii_converters/itoa.h"
#include "../lib/string/string.h"
#include "../mem/heap/heap.h"
#include "../utils/console_print/kconsole.h"
#include "../utils/logging/logger.h"

#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>

#define MODULE "KERNEL_SERVICES"

#define VFS_STDOUT_NODE "stdout"
#define VFS_DISK_NODE "hda"

static vfs_node_t *cached_stdio = NULL;
static vfs_node_t *cached_disk = NULL;

static void kout_str(const char *s) {
  if (cached_stdio == NULL) {
    cached_stdio = vfs_find(VFS_STDOUT_NODE);
  }

  size_t len = strlen(s);
  if (cached_stdio) {
    vfs_write(cached_stdio, 0, len, (uint8_t *)s);
  } else {
    // Fallback if VFS isn't ready
    while (*s)
      kput_char(*s++);
  }
}

static void cache_disk_node() {
  if (cached_disk == NULL) {
    cached_disk = vfs_find(VFS_DISK_NODE);
    if (cached_disk == NULL) {
      kpanic("VFS Storage Error: Node 'hda' not found");
    }
  }
}

/* --- Public API --- */

void kprintf(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);

  for (const char *p = fmt; *p != '\0'; p++) {
    if (*p != '%') {
      char buf[2] = {*p, 0};
      kout_str(buf);
      continue;
    }

    p++; // Skip '%'
    char buffer[32];

    switch (*p) {
    case 'c': {
      buffer[0] = (char)va_arg(args, int);
      buffer[1] = '\0';
      kout_str(buffer);
      break;
    }
    case 's': {
      const char *s = va_arg(args, const char *);
      kout_str(s ? s : "(null)");
      break;
    }
    case 'd':
    case 'i': {
      itoa(va_arg(args, int), buffer, 10);
      kout_str(buffer);
      break;
    }
    case 'u': {
      itoa(va_arg(args, unsigned int), buffer, 10);
      kout_str(buffer);
      break;
    }
    case 'x':
    case 'p': {
      itoa(va_arg(args, uint32_t), buffer, 16);
      if (*p == 'p')
        kout_str("0x");
      kout_str(buffer);
      break;
    }
    case '%': {
      kout_str("%");
      break;
    }
    case '-': {
      // Check if a minus sign is followed by a width (e.g., -10)
      p++;
      int width = 0;
      while (*p >= '0' && *p <= '9') {
        width = width * 10 + (*p - '0');
        p++;
      }

      if (*p == 's') {
        char *s = va_arg(args, char *);
        if (s == NULL)
          s = "(null)";

        size_t len = strlen(s);
        kout_str(s);

        // Padding logic: If string is shorter than width, add spaces
        if (len < (size_t)width) {
          for (size_t i = 0; i < (width - len); i++) {
            kout_str(" ");
          }
        }
      }
      break;
    }
    default: {
      char unknown[2] = {*p, 0};
      kout_str(unknown);
      break;
    }
    }
  }

  va_end(args);
}

void kpanic(const char *msg) {
  kclear_screen();
  kprintf("************************************************\n");
  kprintf("             KERNEL PANIC CAUGHT                \n");
  kprintf("************************************************\n\n");
  kprintf("Reason: %s\n\n", msg);
  kprintf("The system has been halted to prevent damage.\n");
  kprintf("Please manually restart MeowMeowOS.");

  __asm__ volatile("cli"); // Disable interrupts
  for (;;) {
    __asm__ volatile("hlt");
  } // Infinite halt
}

void ksleep(uint32_t ms) {
  uint32_t start_ticks = get_ticks();
  uint32_t wait_ticks = (ms * get_system_freq()) / 1000;

  while ((get_ticks() - start_ticks) < wait_ticks) {
    __asm__ volatile("hlt");
  }
}

void *kmem_alloc(size_t size) { return mem_alloc(size); }
void *kmem_zalloc(size_t size) { return mem_zalloc(size); }
void kmem_free(void *ptr) { mem_free(ptr); }

void kdisk_read_sector(uint32_t lba, uint8_t *buffer) {
  cache_disk_node();
  // Assuming 512 byte sectors
  vfs_read(cached_disk, (lba * 512), 512, buffer);
}

void kdisk_write_sector(uint32_t lba, uint8_t *buffer) {
  cache_disk_node();
  vfs_write(cached_disk, (lba * 512), 512, buffer);
}