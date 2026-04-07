#ifndef LOGGER_H
#define LOGGER_H


#include <stdarg.h>
#include <stddef.h>


#define ERROR_LOOK "\033[1;91m"
#define INFO_LOOK "\033[32m"
#define WARNING_LOOK "\033[33m"
#define DEBUG_LOOK "\033[36m"
#define TRACE_LOOK "\033[35m"
#define RESET_LOOK "\033[0m"

typedef enum {
    LOG_LEVEL_NONE = 0,
    LOG_LEVEL_ERROR = 1,
    LOG_LEVEL_WARNING = 2,
    LOG_LEVEL_INFO = 3,
    LOG_LEVEL_DEBUG = 4,
    LOG_LEVEL_TRACE = 5
} log_level_t; 

typedef void (*log_output_func)(char c, void* context);

typedef struct {
    log_level_t min_level;
    log_output_func output_func;
    void* context;
    char* color;
} logger_config_t;

void logger_initialize(log_level_t min_level);

void logger_set_output(log_output_func output_func, void* context);

void logger_set_level(log_level_t level);

void log_error(const char* module, const char* fmt, ...);
void log_warning(const char* module, const char* fmt, ...);
void log_info(const char* module, const char* fmt, ...);
void log_debug(const char* module, const char* fmt, ...);
void log_trace(const char* module, const char* fmt, ...);

void log_message(log_level_t level, const char* module, const char* fmt, va_list args);

void log_hexdump(log_level_t level, const char* module, const void* data, size_t size);

#define MODULE_LOGGER(module_name) \
    static inline void log_##module_name##_error(const char* fmt, ...) { \
        va_list args; \
        va_start(args, fmt); \
        log_message(LOG_LEVEL_ERROR, #module_name, fmt, args); \
        va_end(args); \
    } \
    static inline void log_##module_name##_warning(const char* fmt, ...) { \
        va_list args; \
        va_start(args, fmt); \
        log_message(LOG_LEVEL_WARNING, #module_name, fmt, args); \
        va_end(args); \
    } \
    static inline void log_##module_name##_info(const char* fmt, ...) { \
        va_list args; \
        va_start(args, fmt); \
        log_message(LOG_LEVEL_INFO, #module_name, fmt, args); \
        va_end(args); \
    } \
    static inline void log_##module_name##_debug(const char* fmt, ...) { \
        va_list args; \
        va_start(args, fmt); \
        log_message(LOG_LEVEL_DEBUG, #module_name, fmt, args); \
        va_end(args); \
    } \
    static inline void log_##module_name##_trace(const char* fmt, ...) { \
        va_list args; \
        va_start(args, fmt); \
        log_message(LOG_LEVEL_TRACE, #module_name, fmt, args); \
        va_end(args); \
    }
    
#endif