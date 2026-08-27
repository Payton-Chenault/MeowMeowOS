#ifndef ACPI_H
#define ACPI_H

#include <stdint.h>
#include <stdbool.h>

void acpi_initialize(void);
void acpi_poweroff(void);
void acpi_reboot(void);

#endif