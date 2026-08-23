#include "logger.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include "../../arch/x86/pit/pit.h"
#include "../../lib/integer_ascii_converters/itoa.h"

#define RING_BUFFER_SIZE 16384
static char log_buffer[RING_BUFFER_SIZE];
static uint32_t log_head = 0;
static bool log_wrapped = false;

static logger_config_t g_logger = {.min_level = LOG_LEVEL_INFO,
                                   .output_func = NULL,
                                   .context = NULL,
                                   .color = RESET_LOOK};

// Routes a single character to both the active output device and the ring buffer
static void log_putchar(char c) {
  if (g_logger.output_func) {
    g_logger.output_func(c, g_logger.context);
  }

  log_buffer[log_head++] = c;
  if (log_head >= RING_BUFFER_SIZE) {
    log_head = 0;
    log_wrapped = true;
  }
}

static void write_string(const char *str) {
  for (const char *c = str; *c != '\0'; c++) {
    log_putchar(*c);
  }
}

static void write_hex(uint32_t num) {
  const char hex[] = "0123456789ABCDEF";
  write_string("0x");

  if (num == 0) {
    write_string("0");
    return;
  }

  int started = 0;
  for (int i = 28l; i >= 0; i -= 4) {
    uint8_t nibble = (num >> i) & 0xF;
    if (nibble != 0 || started || i == 0) {
      char c = hex[nibble];
      log_putchar(c);
      started = 1;
    }
  }
}

// NEW: Helper function to correctly handle unsigned integers without overflowing
static void write_uint(uint32_t num) {
  if (num == 0) {
    log_putchar('0');
    return;
  }

  char buffer[12];
  int i = 0;
  while (num > 0) {
    buffer[i++] = '0' + (num % 10);
    num /= 10;
  }
  while (i > 0) {
    log_putchar(buffer[--i]);
  }
}

void logger_initialize(log_level_t min_level) {
  g_logger.min_level = min_level;
  g_logger.color = RESET_LOOK;
}

void logger_set_output(log_output_func output_func, void *context) {
  g_logger.output_func = output_func;
  g_logger.context = context;
}

void logger_set_level(log_level_t level) { g_logger.min_level = level; }

// Reads the ring buffer chronologically into a user buffer
uint32_t logger_read_log(char *user_buf, uint32_t max_size) {
  uint32_t total_size = log_wrapped ? RING_BUFFER_SIZE : log_head;
  
  if (max_size < total_size) {
    total_size = max_size; 
  }
  
  if (total_size == 0) return 0;
  
  uint32_t read_ptr = log_wrapped ? log_head : 0;
  
  if (max_size < (log_wrapped ? RING_BUFFER_SIZE : log_head)) {
    if (log_wrapped) {
      read_ptr = (log_head + (RING_BUFFER_SIZE - max_size)) % RING_BUFFER_SIZE;
    } else {
      read_ptr = log_head - max_size;
    }
  }
  
  uint32_t copied = 0;
  for (uint32_t i = 0; i < total_size; i++) {
    user_buf[i] = log_buffer[read_ptr];
    read_ptr = (read_ptr + 1) % RING_BUFFER_SIZE;
    copied++;
  }
  
  return copied;
}

void log_message(log_level_t level, const char *module, const char *fmt,
                 va_list args) {
  if (level > g_logger.min_level)
    return;

  uint32_t ticks = get_ticks();

  switch (level) {
  case LOG_LEVEL_NONE: {
    break;
  }
  case LOG_LEVEL_TRACE: {
    g_logger.color = TRACE_LOOK;
    break;
  }
  case LOG_LEVEL_DEBUG: {
    g_logger.color = DEBUG_LOOK;
    break;
  }
  case LOG_LEVEL_INFO: {
    g_logger.color = INFO_LOOK;
    break;
  }
  case LOG_LEVEL_WARNING: {
    g_logger.color = WARNING_LOOK;
    break;
  }
  case LOG_LEVEL_ERROR: {
    g_logger.color = ERROR_LOOK;
    break;
  }
  }

  const char *level_str[] = {"NONE", "ERROR", "WARN", "INFO", "DEBUG", "TRACE"};

  write_string(g_logger.color);
  write_string("[");
  write_string(level_str[level]);
  write_string("] ");
  write_string(RESET_LOOK);

  if (module) {
    write_string(module);
    write_string(": ");
  }

  char tick_buf[16];
  itoa(ticks, tick_buf, 10);
  write_string("Tick: [");
  write_string(tick_buf);
  write_string("] ");

  for (const char *p = fmt; *p != '\0'; p++) {
    if (*p == '%' && *(p + 1) != '\0') {
      p++;
      switch (*p) {
      case 's': {
        const char *str = va_arg(args, const char *);
        write_string(str ? str : "(null)");
        break;
      }
      case 'd': {
        int i = va_arg(args, int);
        char buffer[12];
        itoa(i, buffer, 10);
        write_string(buffer);
        break;
      }
      case 'u': { // <--- NEW FIX
        uint32_t u = va_arg(args, uint32_t);
        write_uint(u);
        break;
      }
      case 'x':
      case 'X': {
        uint32_t num = va_arg(args, uint32_t);
        write_hex(num);
        break;
      }
      case 'c': {
        char c = (char)va_arg(args, int);
        log_putchar(c);
        break;
      }
      case '%':
        log_putchar('%');
        break;
      default:
        log_putchar('%');
        log_putchar(*p);
        break;
      }
    } else {
      log_putchar(*p);
    }
  }

  log_putchar('\n');
}

void log_error(const char *module, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  log_message(LOG_LEVEL_ERROR, module, fmt, args);
  va_end(args);
}

void log_warning(const char *module, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  log_message(LOG_LEVEL_WARNING, module, fmt, args);
  va_end(args);
}

void log_info(const char *module, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  log_message(LOG_LEVEL_INFO, module, fmt, args);
  va_end(args);
}

void log_debug(const char *module, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  log_message(LOG_LEVEL_DEBUG, module, fmt, args);
  va_end(args);
}

void log_trace(const char *module, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  log_message(LOG_LEVEL_TRACE, module, fmt, args);
  va_end(args);
}

void log_hexdump(log_level_t level, const char *module, const void *data,
                 size_t size) {
  if (level > g_logger.min_level)
    return;

  const uint8_t *bytes = (const uint8_t *)data;

  for (size_t i = 0; i < size; i++) {
    if (i % 16 == 0) {
      if (i > 0)
        write_string("\n");

      write_string("[");
      const char *level_str[] = {"NONE", "ERROR", "WARN",
                                 "INFO", "DEBUG", "TRACE"};
      write_string(level_str[level]);
      write_string("] ");

      if (module) {
        write_string(module);
        write_string(": ");
      }

      write_hex(i);
      write_string(": ");
    }

    char hex[3];
    const char *hex_chars = "0123456789ABCDEF";
    hex[0] = hex_chars[(bytes[i] >> 4) & 0xF];
    hex[1] = hex_chars[bytes[i] & 0xF];
    hex[2] = '\0';

    for (int j = 0; j < 2; j++) {
      log_putchar(hex[j]);
    }
    log_putchar(' ');
  }
  write_string("\n");
}