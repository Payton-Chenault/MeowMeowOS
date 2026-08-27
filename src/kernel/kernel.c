#include "kernel.h"
#include "arch/x86/global_descriptor_table/gdt.h"
#include "arch/x86/interrupt_descriptor_table/idt.h"
#include "arch/x86/pit/pit.h"
#include "arch/x86/task/task.h"
#include "drivers/disk/block_dev.h"
#include "drivers/keyboard/keyboard.h"
#include "drivers/keyboard/keyboard_vfs.h"
#include "drivers/serial/serial_logger.h"
#include "drivers/cmos/rtc.h"
#include "drivers/pci/pci.h"
#include "drivers/acpi/acpi.h"
#include "drivers/vga_display/vga_vfs.h"
#include "fs/fat_16/fat16.h"
#include "fs/fat_16/fat16_vfs.h"
#include "fs/vfs/vfs.h"
#include "kernel_services/kernel_services.h"
#include "mem/heap/heap.h"
#include "mem/physical_memory_manager/pmm.h"
#include "mem/virtual_memory_manager/vmm.h"
#include "progs/shell/shell.h"
#include "utils/console_print/kconsole.h"
#include <stdint.h>

#define MODULE "KERNEL"

typedef struct __attribute__((packed)) {
    uint16_t attributes;
    uint8_t window_a;
    uint8_t window_b;
    uint16_t granularity;
    uint16_t window_size;
    uint16_t segment_a;
    uint16_t segment_b;
    uint32_t win_func_ptr;
    uint16_t pitch;
    uint16_t width;
    uint16_t height;
    uint8_t w_char;
    uint8_t y_char;
    uint8_t planes;
    uint8_t bpp;
    uint8_t banks;
    uint8_t memory_model;
    uint8_t bank_size;
    uint8_t image_pages;
    uint8_t reserved0;
    uint8_t red_mask, red_position;
    uint8_t green_mask, green_position;
    uint8_t blue_mask, blue_position;
    uint8_t reserved_mask, reserved_position;
    uint8_t direct_color_attributes;
    uint32_t framebuffer;
    uint32_t off_screen_mem_off;
    uint16_t off_screen_mem_size;
    uint8_t reserved1[206];
} vbe_mode_info_t;

extern void vmm_map_region(uint32_t phys_start, uint32_t virt_start, uint32_t size, uint32_t flags);

void kernel_bootstrap() {
  serial_logging_initialize(LOG_LEVEL_DEBUG);
  gdt_initialize();
  idt_initialize();
  pmm_initialize_from_map();
  vmm_initialize();
  heap_initialize(0x600000, 0x100000);

  vbe_mode_info_t* vbe_info = (vbe_mode_info_t*)0x8000;
  if (vbe_info->framebuffer != 0) {
      uint32_t fb_size = vbe_info->height * vbe_info->pitch;
      vmm_map_region(vbe_info->framebuffer, vbe_info->framebuffer, fb_size, PAGE_PRESENT | PAGE_WRITE);
      log_info(MODULE, "Framebuffer mapped: Addr=0x%x, Res=%dx%d, BPP=%d",
                vbe_info->framebuffer, vbe_info->width, vbe_info->height, vbe_info->bpp);
  }

  task_initialize();
  pit_initialize(1000);
  rtc_initialize();
  pci_initialize();
  acpi_initialize();
  block_device_initialize();
  fat16_initialize();
  fat16_vfs_driver_initialize();
  kscreen_initialize();
  keyboard_initialize();
  vga_vfs_initialize();
  keyboard_vfs_initialize();
  enable_interrupts();
}

void kernel_main() {
  kernel_bootstrap();

  kclear_screen();
  fb_draw_bmp_file("/splash.bmp");
  fb_draw_bmp_file("/system/assets/splash_screen/splash.bmp");
  ksleep(1000);
  kclear_screen();

  kprintf("Meow-Meow-OS is ready. Type 'help' for commands.\n");

  task_create("shell", kshell_main, (uint32_t)vmm_get_directory());

  while (1) {
    enable_interrupts();
    wait_for_interrupt();
  }
}