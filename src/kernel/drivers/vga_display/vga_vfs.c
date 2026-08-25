#include "vga_vfs.h"
#include "../../kernel_services/kernel_services.h"
#include "../../utils/console_print/kconsole.h"
#include "../../lib/string/string.h"
#include "../../utils/logging/logger.h"
#include <stdint.h>

#define MODULE "VGA_VFS"

static uint32_t vga_write(vfs_node_t *node, uint32_t offset, uint32_t size,
                          uint8_t *buffer) {
  (void)node;
  (void)offset;

  if (buffer == NULL || size == 0) {
    return 0;
  }

  for (uint32_t i = 0; i < size; i++) {
    kput_char((char)buffer[i]);
  }

  return size;
}

void vga_vfs_initialize() {
  vfs_node_t *vga_node = kmem_zalloc(sizeof(vfs_node_t));
  if (vga_node == NULL) {
    kpanic("Failed to allocate VGA VFS node");
  }

  strcpy(vga_node->name, "stdout");
  vga_node->type = VFS_DEVICE;
  vga_node->length = 0;
  vga_node->log_use = false;

  vga_node->write = vga_write;
  vga_node->read = NULL;

  vga_node->uid = 0;
  vga_node->gid = 0;
  vga_node->mode = VFS_DEFAULT_DEV_MODE;
  vga_node->persistent = true;
  vga_node->ref_count = 1;

  vfs_register_node(vga_node);
  log_info(MODULE, "Initialized");
}