#ifndef KERNEL_H
#define KERNEL_H

#include "implementations/console_print/kconsole.h"
#include "intf/serial/serial_logger.h"
#include "intf/keyboard_input/keyboard.h"
#include "utils/global_descriptor_table/gdt.h"
#include "mem/physical_memory_manager/pmm.h"
#include "mem/virtual_memory_manager/vmm.h"
#include "mem/heap/heap.h"

void kernel_main();

#endif