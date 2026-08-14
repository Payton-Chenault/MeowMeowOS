#include "kernel.h"

#include "arch/x86/global_descriptor_table/gdt.h"
#include "arch/x86/interrupt_descriptor_table/idt.h"
#include "arch/x86/pit/pit.h"
#include "arch/x86/task/task.h"
#include "drivers/disk/block_dev.h"
#include "drivers/keyboard/keyboard.h"
#include "drivers/keyboard/keyboard_vfs.h"
#include "drivers/serial/serial_logger.h"
#include "drivers/vga_display/vga_vfs.h"
#include "fs/fat_16/fat16.h"
#include "kernel_services/kernel_services.h"
#include "mem/heap/heap.h"
#include "mem/physical_memory_manager/pmm.h"
#include "mem/virtual_memory_manager/vmm.h"
#include "progs/shell/shell.h"
#include "utils/console_print/kconsole.h"
#include <stdint.h>

#define MODULE "KERNEL"

const char *splash_screen =
    " _____                   _____                   _____ _____\n|     |___ "
    "___ _ _ _ ___|     |___ ___ _ _ _ ___|     |   __|\n| | | | -_| . | | | "
    "|___| | | | -_| . | | | |___|  |  |__   |\n|_|_|_|___|___|_____|   "
    "|_|_|_|___|___|_____|   |_____|_____|\n\n";

void kernel_bootstrap() {

  serial_logging_initialize(LOG_LEVEL_DEBUG);
  gdt_initialize();
  idt_initialize();

  pmm_initialize_from_map();
  vmm_initialize();
  heap_initialize(0x600000, 0x100000);

  task_initialize();
  pit_initialize(1000);

  block_device_initialize();
  fat16_initialize();

  kscreen_initialize();
  keyboard_initialize();

  vga_vfs_initialize();
  keyboard_vfs_initialize();

  enable_interrupts();
}

void kernel_main() {
  kernel_bootstrap();

  kprintf(splash_screen);
  kprintf("MeowMeowOS is ready. Type 'help' for commands.\n");

  task_create("shell", kshell_main, (uint32_t)vmm_get_directory());
  while (1) {
    enable_interrupts();
    wait_for_interrupt();
  }
}