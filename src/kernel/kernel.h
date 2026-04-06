#ifndef KERNEL_H
#define KERNEL_H

#include "intf/serial/serial_logger.h"
#include "intf/keyboard_input/keyboard.h"
#include "utils/console_print/kconsole.h"
#include "utils/global_descriptor_table/gdt.h"
#include "utils/pit/pit.h"
#include "mem/physical_memory_manager/pmm.h"
#include "mem/virtual_memory_manager/vmm.h"
#include "mem/heap/heap.h"
#include "progs/shell/shell.h"

void kernel_main();

#endif