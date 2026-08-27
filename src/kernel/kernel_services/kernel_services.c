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
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define MODULE "KERNEL_SERVICES"

#define VFS_STDOUT_NODE "/dev/stdout"
#define VFS_DISK_NODE "/dev/hda"

static vfs_node_t *cached_stdio = NULL;
static vfs_node_t *cached_disk = NULL;

static void cache_stdio_node(void) {
  if (cached_stdio == NULL) {
    cached_stdio = vfs_open(VFS_STDOUT_NODE);
  }
}

static void kout_str(const char *s) {
  if (s == NULL) {
    s = "(null)";
  }
  cache_stdio_node();
  size_t len = strlen(s);
  if (cached_stdio != NULL) {
    vfs_write(cached_stdio, 0, len, (uint8_t *)s);
  } else {
    kprint(s);
  }
}

static void cache_disk_node(void) {
  if (cached_disk == NULL) {
    cached_disk = vfs_open(VFS_DISK_NODE);
    if (cached_disk == NULL) {
      kpanic("VFS Storage Error: Node 'hda' not found");
    }
  }
}

void kprintf(const char *fmt, ...) {
  static char buffer[2048];
  size_t len = 0;
  va_list args;
  va_start(args, fmt);

  for (const char *p = fmt; *p != '\0' && len < sizeof(buffer) - 1; p++) {
    if (*p != '%') {
      buffer[len++] = *p;
      continue;
    }
    p++;
    if (*p == '\0') {
      break;
    }

    bool pad_zero = false;
    bool left_align = false;
    while (*p == '0' || *p == '-') {
      if (*p == '0') pad_zero = true;
      if (*p == '-') left_align = true;
      p++;
    }
    if (left_align) {
      pad_zero = false;
    }

    int width = 0;
    while (*p >= '0' && *p <= '9') {
      width = width * 10 + (*p - '0');
      p++;
    }
    if (*p == '\0') {
      break;
    }

    switch (*p) {
    case 'c': {
      char c = (char)va_arg(args, int);
      if (!left_align && width > 1) {
        for (int i = 0; i < width - 1 && len < sizeof(buffer) - 1; i++) {
          buffer[len++] = ' ';
        }
      }
      if (len < sizeof(buffer) - 1) {
        buffer[len++] = c;
      }
      if (left_align && width > 1) {
        for (int i = 0; i < width - 1 && len < sizeof(buffer) - 1; i++) {
          buffer[len++] = ' ';
        }
      }
      break;
    }
    case 's': {
      const char *s = va_arg(args, const char *);
      if (s == NULL) {
        s = "(null)";
      }
      int slen = (int)strlen(s);
      if (!left_align && width > slen) {
        for (int i = 0; i < width - slen && len < sizeof(buffer) - 1; i++) {
          buffer[len++] = ' ';
        }
      }
      while (*s != '\0' && len < sizeof(buffer) - 1) {
        buffer[len++] = *s++;
      }
      if (left_align && width > slen) {
        for (int i = 0; i < width - slen && len < sizeof(buffer) - 1; i++) {
          buffer[len++] = ' ';
        }
      }
      break;
    }
    case 'd':
    case 'i': {
      char numbuf[32];
      int val = va_arg(args, int);
      itoa(val, numbuf, 10);
      int nlen = (int)strlen(numbuf);
      char pad_char = pad_zero ? '0' : ' ';

      if (val < 0 && pad_zero) {
        if (len < sizeof(buffer) - 1) {
          buffer[len++] = '-';
        }
        char *num_digits = numbuf + 1;
        for (int i = 0; i < width - nlen && len < sizeof(buffer) - 1; i++) {
          buffer[len++] = '0';
        }
        for (int i = 0; num_digits[i] != '\0' && len < sizeof(buffer) - 1; i++) {
          buffer[len++] = num_digits[i];
        }
      } else {
        if (!left_align && width > nlen) {
          for (int i = 0; i < width - nlen && len < sizeof(buffer) - 1; i++) {
            buffer[len++] = pad_char;
          }
        }
        for (int i = 0; numbuf[i] != '\0' && len < sizeof(buffer) - 1; i++) {
          buffer[len++] = numbuf[i];
        }
        if (left_align && width > nlen) {
          for (int i = 0; i < width - nlen && len < sizeof(buffer) - 1; i++) {
            buffer[len++] = ' ';
          }
        }
      }
      break;
    }
    case 'u': {
      char numbuf[32];
      unsigned int val = va_arg(args, unsigned int);
      itoa(val, numbuf, 10);
      int nlen = (int)strlen(numbuf);
      char pad_char = pad_zero ? '0' : ' ';
      if (!left_align && width > nlen) {
        for (int i = 0; i < width - nlen && len < sizeof(buffer) - 1; i++) {
          buffer[len++] = pad_char;
        }
      }
      for (int i = 0; numbuf[i] != '\0' && len < sizeof(buffer) - 1; i++) {
        buffer[len++] = numbuf[i];
      }
      if (left_align && width > nlen) {
        for (int i = 0; i < width - nlen && len < sizeof(buffer) - 1; i++) {
          buffer[len++] = ' ';
        }
      }
      break;
    }
    case 'x':
    case 'X':
    case 'p': {
      char numbuf[32];
      uint32_t val = va_arg(args, uint32_t);
      itoa(val, numbuf, 16);
      if (*p == 'X') {
        for (int i = 0; numbuf[i] != '\0'; i++) {
          if (numbuf[i] >= 'a' && numbuf[i] <= 'f') {
            numbuf[i] = numbuf[i] - 'a' + 'A';
          }
        }
      }
      int nlen = (int)strlen(numbuf);
      if (*p == 'p') {
        if (len + 2 < sizeof(buffer) - 1) {
          buffer[len++] = '0';
          buffer[len++] = 'x';
        }
      }
      char pad_char = pad_zero ? '0' : ' ';
      if (!left_align && width > nlen) {
        for (int i = 0; i < width - nlen && len < sizeof(buffer) - 1; i++) {
          buffer[len++] = pad_char;
        }
      }
      for (int i = 0; numbuf[i] != '\0' && len < sizeof(buffer) - 1; i++) {
        buffer[len++] = numbuf[i];
      }
      if (left_align && width > nlen) {
        for (int i = 0; i < width - nlen && len < sizeof(buffer) - 1; i++) {
          buffer[len++] = ' ';
        }
      }
      break;
    }
    case '%': {
      buffer[len++] = '%';
      break;
    }
    default: {
      buffer[len++] = '%';
      if (len < sizeof(buffer) - 1) {
        buffer[len++] = *p;
      }
      break;
    }
    }
  }

  buffer[len] = '\0';
  va_end(args);
  kout_str(buffer);
}

void kpanic(const char *msg) {
  kclear_screen();
  kprintf("************************************************\n");
  kprintf("             KERNEL PANIC CAUGHT                \n");
  kprintf("************************************************\n\n");
  kprintf("Reason: %s\n\n", msg);
  kprintf("The system has been halted to prevent damage.\n");
  kprintf("Please manually restart MeowMeowOS.");
  __asm__ volatile("cli");
  for (;;) {
    __asm__ volatile("hlt");
  }
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
  vfs_read(cached_disk, (lba * 512), 512, buffer);
}

void kdisk_write_sector(uint32_t lba, uint8_t *buffer) {
  cache_disk_node();
  vfs_write(cached_disk, (lba * 512), 512, buffer);
}