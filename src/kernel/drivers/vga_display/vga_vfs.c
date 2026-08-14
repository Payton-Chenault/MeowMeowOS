#include "vga_vfs.h"
#include "../../kernel_services/kernel_services.h"
#include "vga.h"

#include "../../lib/string/string.h"
#include <stdint.h>

#define MODULE "VGA_VFS"

static uint32_t vga_write(vfs_node_t *node, uint32_t offset, uint32_t size,
                          uint8_t *buffer) {
  for (uint32_t i = 0; i < size; i++) {
    terminal_putchar((char)buffer[i]);
  }

  return size;
}

void vga_vfs_initialize() {
  vfs_node_t *vga_node = kmem_alloc(sizeof(vfs_node_t));

  strcpy(vga_node->name, "stdout");
  vga_node->type = VFS_DEVICE;
  vga_node->length = 0;
  vga_node->log_use = false;

  vga_node->write = vga_write;
  vga_node->read = NULL;

  vfs_register_node(vga_node);
  log_info(MODULE, "Initialized");
}
