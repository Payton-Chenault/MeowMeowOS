#include "vga.h"

static void vga_log_output(char c, void* context) {
    terminal_putchar(c);
}

void init_vga_logging(void) {
    logger_set_output(vga_log_output, NULL);
    logger_init(LOG_LEVEL_INFO);
    log_info("Logger", "VGA Logging Backend Initialized"); 
}