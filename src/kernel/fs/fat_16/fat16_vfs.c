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

  // --- Handle absolute paths ---
  char path_copy[256];
  strcpy(path_copy, filename);

  char *base_name = NULL;
  char directory[256] = "/";
  bool has_path = false;

  char *slash = strrchr(path_copy, '/');
  if (slash != NULL) {
    has_path = true;
    *slash = '\0'; // split directory and filename
    base_name = slash + 1;

    if (path_copy[0] == '\0') {
      strcpy(directory, "/");
    } else {
      strcpy(directory, path_copy);
    }
  } else {
    base_name = path_copy;
  }

  // Save current working directory
  char old_cwd[256];
  strcpy(old_cwd, fat16_get_current_path());

  // If path contains a directory, temporarily change to it
  if (has_path) {
    if (fat16_chdir(directory) != 0) {
      log_error(MODULE, "fat16_vfs_open: failed to chdir to %s", directory);
      return NULL;
    }
  }

  // Now open the base name
  uint32_t file_size = fat16_get_file_size(base_name);
  log_debug(MODULE, "fat16_get_file_size returned: %u", file_size);

  vfs_node_t *node = NULL;

  if (file_size > 0) {
    node = (vfs_node_t *)kmem_zalloc(sizeof(vfs_node_t));
    if (node) {

      if (has_path) {
        strcpy(node->name, filename); 
      } else {
        strcpy(node->name, base_name);
      }
      node->type = VFS_FILE;
      node->length = file_size;
      node->log_use = false;
      node->read = fat16_vfs_read;
      node->write = fat16_vfs_write;
      node->uid = 0;
      node->gid = 0;
      node->mode = VFS_DEFAULT_FILE_MODE;
      node->flags = 0;
      node->ref_count = 1;
      node->persistent = false;
    }
  }

  // Restore cwd
  fat16_chdir(old_cwd);
  return node;
}