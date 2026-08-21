#include "keyboard_vfs.h"
#include "../../kernel_services/kernel_services.h"
#include "../../lib/string/string.h"
#include "../../utils/logging/logger.h"
#include "keyboard.h"
#include <stdint.h>

#define MODULE "KEYBOARD_VFS"

static uint32_t keyboard_vfs_read(vfs_node_t *node, uint32_t offset,
                                  uint32_t size, uint8_t *buffer) {
  uint32_t read_bytes = 0;

  while (read_bytes < size) {
    char c = keyboard_read_char();
    if (c == 0)
      break;

    buffer[read_bytes] = (uint8_t)c;
    read_bytes++;
  }

  return read_bytes;
}

void keyboard_vfs_initialize() {
  vfs_node_t *keyboard_node = (vfs_node_t *)kmem_zalloc(sizeof(vfs_node_t));

  strcpy(keyboard_node->name, "stdin");
  keyboard_node->type = VFS_DEVICE;

  keyboard_node->read = keyboard_vfs_read;
  keyboard_node->write = NULL;

  keyboard_node->uid = 0;
  keyboard_node->gid = 0;
  keyboard_node->mode = VFS_DEFAULT_DEV_MODE;
  keyboard_node->persistent = true;
  keyboard_node->ref_count = 1;

  vfs_register_node(keyboard_node);
  log_info(MODULE, "Initialized");
}