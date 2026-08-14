#ifndef SERIAL_LOGGER_H
#define SERIAL_LOGGER_H

#include "../../utils/logging/logger.h"

void serial_log_output(char c, void *context);
void serial_logging_initialize(log_level_t logging_level);

#endif