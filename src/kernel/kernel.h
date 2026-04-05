#ifndef KERNEL_H
#define KERNEL_H

#include "implementations/console_print//kconsole.h"
#include "intf/vga_display/vga_backend.h"
#include "intf/keyboard_input/keyboard.h"

void kernel_main();

#endif