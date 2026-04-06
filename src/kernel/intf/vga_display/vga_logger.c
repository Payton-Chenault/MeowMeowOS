#include "vga.h"
#include "vga_logger.h"

#define MODULE "VGA_LOGGER"

void vga_log_output(char c, void* context) {
    terminal_putchar(c);
}

void vga_logging_initialize(log_level_t logging_level) {
    logger_set_output(vga_log_output, NULL);
    logger_initialize(logging_level);
    log_debug(MODULE, "VGA Logging Backend Initialized"); 
}