#include "fat16_vfs.h"
#include "../../kernel_services/kernel_services.h"
#include "../../lib/string/string.h"
#include "../../utils/logging/logger.h"
#include "fat16.h"
#include <stdbool.h>
#include <stdint.h>


#define MODULE "FAT16_VFS"

static uint32_t fat16_vfs_read(vfs_node_t *node, uint32_t offset, uint32_t size,
                               uint8_t *buffer) {
  return fat16_read_file(node->name, offset, size, buffer);
}

static uint32_t fat16_vfs_write(vfs_node_t *node, uint32_t offset,
                                uint32_t size, uint8_t *buffer) {
  uint32_t new_size = node->length;
  if (offset + size > node->length) {
    new_size = offset + size;
  }

  uint8_t *temp_buffer = (uint8_t *)kmem_zalloc(new_size);
  if (!temp_buffer) {
    return 0;
  }

  if (node->length > 0) {
    fat16_read_file(node->name, 0, node->length, temp_buffer);
  }

  memcpy(temp_buffer + offset, buffer, size);

  if (node->length > 0) {
    fat16_delete_file(node->name);
  }

  fat16_write_file(node->name, temp_buffer, new_size);

  node->length = new_size;

  kmem_free(temp_buffer);
  return size;
}

vfs_node_t *fat16_vfs_open(const char *filename) {
  log_debug(MODULE, "fat16_vfs_open called for: %s", filename);

  if (filename == NULL) {
    log_error(MODULE, "fat16_vfs_open: null filename");
    return NULL;
  }

  uint32_t file_size = fat16_get_file_size(filename);
  log_debug(MODULE, "fat16_get_file_size returned: %u", file_size);

  if (file_size == 0) {
    log_debug(MODULE, "File not found or empty: %s", filename);
    return NULL;
  }

  vfs_node_t *node = (vfs_node_t *)kmem_zalloc(sizeof(vfs_node_t));
  if (node == NULL) {
    log_error(MODULE, "kmem_zalloc failed for vfs_node");
    return NULL;
  }

  strcpy(node->name, filename);
  node->type = VFS_FILE;
  node->length = file_size;
  node->log_use = false;

  node->read = fat16_vfs_read;
  node->write = fat16_vfs_write;

  log_debug(MODULE, "fat16_vfs_open success, node = %x", node);
  return node;
}