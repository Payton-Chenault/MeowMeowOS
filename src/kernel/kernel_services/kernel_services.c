#include "kernel_services.h"
#include "../arch/x86/pit/pit.h"
#include "../drivers/disk/ata.h"
#include "../fs/vfs/vfs.h"
#include "../fs/fat_16/fat16_vfs.h"
#include "../drivers/audio/ac97.h"
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

// Minimal WAV Parser definitions for Audio Manager
typedef struct __attribute__((packed)) {
    char riff_tag[4];
    uint32_t riff_size;
    char wave_tag[4];
} k_wave_header_t;

typedef struct __attribute__((packed)) {
    char chunk_id[4];
    uint32_t chunk_size;
    uint16_t audio_format;
    uint16_t num_channels;
    uint32_t sample_rate;
    uint32_t byte_rate;
    uint16_t block_align;
    uint16_t bits_per_sample;
} k_wave_fmt_chunk_t;

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
      log_error(MODULE, "VFS Storage Error: Node 'hda' not found");
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
      itoa((int)val, numbuf, 10);
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
  log_error(MODULE, "CRITICAL: Kernel Panic: %s", msg);
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
  log_trace(MODULE, "ksleep called for %u ms", ms);
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

// -----------------------------------------------------------------------
// Internal Kernel Audio Manager Subsystem
// -----------------------------------------------------------------------

void kplay_sound_file(const char *path) {
    log_trace(MODULE, "kplay_sound_file: Attempting to play '%s'", path);
    if (!ac97_is_present()) {
        log_warning(MODULE, "kplay_sound_file: AC'97 hardware not present. Audio skipped.");
        return;
    }

    vfs_node_t *node = vfs_open(path);
    if (!node) {
        node = fat16_vfs_open(path);
    }
    if (!node) {
        log_debug(MODULE, "Kernel audio asset not found: %s", path);
        return;
    }

    k_wave_header_t header;
    if (vfs_read(node, 0, sizeof(header), (uint8_t*)&header) != sizeof(header)) {
        log_warning(MODULE, "Premature EOF reading WAV header for %s", path);
        vfs_close(node);
        return;
    }

    if (strncmp(header.riff_tag, "RIFF", 4) != 0 || strncmp(header.wave_tag, "WAVE", 4) != 0) {
        log_error(MODULE, "Kernel audio asset invalid format: %s", path);
        vfs_close(node);
        return;
    }

    k_wave_fmt_chunk_t fmt;
    uint32_t offset = 12;
    char chunk_hdr[8];
    uint32_t data_offset = 0;
    uint32_t data_size = 0;

    while (vfs_read(node, offset, 8, (uint8_t*)chunk_hdr) == 8) {
        uint32_t csize = *(uint32_t *)(chunk_hdr + 4);
        if (strncmp(chunk_hdr, "fmt ", 4) == 0) {
            vfs_read(node, offset + 8, sizeof(k_wave_fmt_chunk_t) - 8, ((uint8_t*)&fmt) + 8);
            memcpy(&fmt, chunk_hdr, 8);
        } else if (strncmp(chunk_hdr, "data", 4) == 0) {
            data_size = csize;
            data_offset = offset + 8;
            break;
        }
        offset += 8 + csize;
    }

    if (data_size > 0 && fmt.audio_format == 1) {
        uint8_t *audio_data = kmem_alloc(data_size);
        if (audio_data) {
            log_info(MODULE, "Playing audio asset '%s' (%u bytes, %u Hz, %u ch, %u-bit)",
                     path, data_size, fmt.sample_rate, fmt.num_channels, fmt.bits_per_sample);
            vfs_read(node, data_offset, data_size, audio_data);
            ac97_play_pcm(audio_data, data_size, fmt.sample_rate, fmt.num_channels, fmt.bits_per_sample);
            kmem_free(audio_data);
        } else {
            log_error(MODULE, "Out of memory allocating %u bytes for audio playback (%s)", data_size, path);
        }
    } else {
        log_warning(MODULE, "Unsupported audio parameters in %s: fmt=%u, size=%u", path, fmt.audio_format, data_size);
    }

    vfs_close(node);
}

void ksound_notify(void) {
    log_trace(MODULE, "ksound_notify: Invoked notification chime");
    vfs_node_t *node = vfs_open("/system/assets/sounds/notifications/blip.wav");
    if (!node) node = fat16_vfs_open("/system/assets/sounds/notifications/blip.wav");
    if (node) {
        vfs_close(node);
        kplay_sound_file("/system/assets/sounds/notifications/blip.wav");
        return;
    }
    kplay_sound_file("/notification_blip.wav");
}

void ksound_error(void) {
    log_trace(MODULE, "ksound_error: Invoked error alert sound");
    vfs_node_t *node = vfs_open("/system/assets/sounds/errors/blip.wav");
    if (!node) node = fat16_vfs_open("/system/assets/sounds/errors/blip.wav");
    if (node) {
        vfs_close(node);
        kplay_sound_file("/system/assets/sounds/errors/blip.wav");
        return;
    }
    kplay_sound_file("/error_blip.wav");
}

void ksound_boot(void) {
    log_trace(MODULE, "ksound_boot: Invoked startup sound sequence");
    // 1. Post-installation path
    vfs_node_t *node = vfs_open("/system/assets/sounds/system/startup.wav");
    if (!node) node = fat16_vfs_open("/system/assets/sounds/system/startup.wav");
    if (node) {
        vfs_close(node);
        kplay_sound_file("/system/assets/sounds/system/startup.wav");
        return;
    }
    // 2. Pre-installation fallback paths on root
    node = vfs_open("/system_startup.wav");
    if (!node) node = fat16_vfs_open("/system_startup.wav");
    if (node) {
        vfs_close(node);
        kplay_sound_file("/system_startup.wav");
        return;
    }
    node = vfs_open("/startup.wav");
    if (!node) node = fat16_vfs_open("/startup.wav");
    if (node) {
        vfs_close(node);
        kplay_sound_file("/startup.wav");
        return;
    }
    log_warning(MODULE, "No valid startup audio file located for boot sequence");
}