#include "serial.h"
#include "serial_logger.h"
#include <stddef.h>


#define MODULE "SERIAL_LOGGER"

void serial_log_output(char c, void* context) {
    serial_put_char(c);
}

void serial_logging_initialize(log_level_t logging_level) {
    logger_set_output(serial_log_output, NULL);
    logger_initialize(logging_level);
    log_info(MODULE, "Initialized"); 
}