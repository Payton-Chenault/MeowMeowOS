#ifndef KERNEL_H
#define KERNEL_H

#include "drivers/serial/serial_logger.h"
#include "drivers/keyboard_input/keyboard.h"
#include "utils/console_print/kconsole.h"
#include "arch/x86/global_descriptor_table/gdt.h"
#include "arch/x86/pit/pit.h"
#include "mem/physical_memory_manager/pmm.h"
#include "mem/virtual_memory_manager/vmm.h"
#include "mem/heap/heap.h"
#include "progs/shell/shell.h"

void kernel_main();

#endif