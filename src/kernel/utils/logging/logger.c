#include "logger.h"
#include <stddef.h>
#include <stdint.h>
#include <stdarg.h>

#include "../../lib/integer_ascii_converters/itoa.h"

static logger_config_t g_logger = {
    .min_level = LOG_LEVEL_INFO,
    .output_func = NULL, 
    .context = NULL,
    .color = RESET_LOOK
};

static void write_string(const char* str) {
    if (!g_logger.output_func) return;

    for (const char* c = str; *c != '\0'; c++) {
        g_logger.output_func(*c, g_logger.context);
    }
} 

static void write_hex(uint32_t num) {
    const char hex[] = "0123456789ABCDEF";
    char buffer[9];
    write_string("0x");

    if(num == 0) {
        write_string("0");
        return;
    }

    int started = 0;
    for (int i = 28l; i >= 0; i -=4) {
        uint8_t nibble = (num >> i) & 0xF;
        if(nibble != 0 || started || i == 0) {
            char c = hex[nibble];
            g_logger.output_func(c, g_logger.context);
            started = 1;
        }
    }
}

void logger_initialize(log_level_t min_level) {
    g_logger.min_level = min_level;
    g_logger.color = RESET_LOOK;
}

void logger_set_output(log_output_func output_func, void* context) {
    g_logger.output_func = output_func;
    g_logger.context = context;
}

void logger_set_level(log_level_t level) {
    g_logger.min_level = level;
}

void log_message(log_level_t level, const char* module, const char* fmt, va_list args) {
    if (level > g_logger.min_level) return; 
    if (!g_logger.output_func) return;

    switch (level) {
        case LOG_LEVEL_NONE: {
            break;
        }
        case LOG_LEVEL_TRACE: {
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

    const char* level_str[] = {
        "NONE", "ERROR", "WARN", "INFO", "DEBUG", "TRACE"
    };

    write_string(g_logger.color);
    write_string("[");
    write_string(level_str[level]);
    write_string("] ");
    write_string(RESET_LOOK);

    if(module) {
        write_string(module);
        write_string(": ");
    }

    for (const char* p = fmt; *p != '\0'; p++) {
        if (*p == '%' && *(p+1) != '\0') {
            p++;
            switch (*p) {
                case 's': {
                    const char* str = va_arg(args, const char*);
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
                case 'x':
                case 'X': {
                    uint32_t num = va_arg(args, uint32_t);
                    write_hex(num);
                    break;
                }
                case 'c': {
                    char c = (char)va_arg(args, int);
                    g_logger.output_func(c, g_logger.context);
                    break; 
                }
                case '%':
                    g_logger.output_func('%', g_logger.context);
                    break;
                default:
                    g_logger.output_func('%', g_logger.context);
                    g_logger.output_func(*p, g_logger.context);
                    break;
            }
        } else {
            g_logger.output_func(*p, g_logger.context);
        }
    }

    g_logger.output_func('\n', g_logger.context);
}

void log_error(const char* module, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_message(LOG_LEVEL_ERROR, module, fmt, args);
    va_end(args);
}

void log_warning(const char* module, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_message(LOG_LEVEL_WARNING, module, fmt, args);
    va_end(args);
}

void log_info(const char* module, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_message(LOG_LEVEL_INFO, module, fmt, args);
    va_end(args);
}

void log_debug(const char* module, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_message(LOG_LEVEL_DEBUG, module, fmt, args);
    va_end(args);
}

void log_trace(const char* module, const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
    log_message(LOG_LEVEL_TRACE, module, fmt, args);
    va_end(args);
}

void log_hexdump(log_level_t level, const char* module, const void* data, size_t size) {
    if (level > g_logger.min_level) return;
    
    const uint8_t* bytes = (const uint8_t*)data;
    
    for (size_t i = 0; i < size; i++) {
        if (i % 16 == 0) {
            if (i > 0) write_string("\n");
            
            // FIXED: Can't call log_message directly with format args
            // Instead, manually write the offset
            write_string("[");
            const char* level_str[] = {"NONE", "ERROR", "WARN", "INFO", "DEBUG", "TRACE"};
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
        const char* hex_chars = "0123456789ABCDEF";
        hex[0] = hex_chars[(bytes[i] >> 4) & 0xF];
        hex[1] = hex_chars[bytes[i] & 0xF];
        hex[2] = '\0';
        
        for (int j = 0; j < 2; j++) {
            g_logger.output_func(hex[j], g_logger.context);
        }
        g_logger.output_func(' ', g_logger.context);
    }
    write_string("\n");
}

