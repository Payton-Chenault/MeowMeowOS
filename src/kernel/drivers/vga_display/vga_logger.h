#ifndef VGA_LOGGER_H
#define  VGA_LOGGER_H

#include "../../utils/logging/logger.h"

void vga_log_output(char c, void* context);
void vga_logging_initialize(log_level_t logging_level);

#endif