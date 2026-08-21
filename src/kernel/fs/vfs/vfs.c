#include "vfs.h"
#include "../../kernel_services/kernel_services.h"
#include "../../lib/string/string.h"
#include "../../utils/logging/logger.h"
#include <stddef.h>
#include <stdint.h>

#define MODULE "VFS"
#define VFS_HASH_SIZE 128

typedef struct vfs_hash_entry {
  char name[32];
  vfs_node_t *node;
  struct vfs_hash_entry *next;
} vfs_hash_entry_t;

static vfs_hash_entry_t *vfs_hash_table[VFS_HASH_SIZE];

uint32_t vfs_hash(const char *str) {
  uint32_t hash = 5381;
  int c;
  while ((c = *str++)) {
    hash = ((hash << 5) + hash) + c;
  }
  return hash % VFS_HASH_SIZE;
}

vfs_node_t *vfs_root = NULL;

uint32_t vfs_read(vfs_node_t *node, uint32_t offset, uint32_t size,
                  uint8_t *buffer) {
  if (node == NULL || node->read == NULL || buffer == NULL)
    return 0;

  if (node->log_use)
    log_debug(MODULE, "read from node \"%s\"", node->name);

  return node->read(node, offset, size, buffer);
}

uint32_t vfs_write(vfs_node_t *node, uint32_t offset, uint32_t size,
                   uint8_t *buffer) {
  if (node == NULL || node->write == NULL || buffer == NULL)
    return 0;

  if (node->log_use)
    log_debug(MODULE, "write to node \"%s\"", node->name);

  return node->write(node, offset, size, buffer);
}

vfs_node_t *vfs_retain(vfs_node_t *node) {
  if (node == NULL)
    return NULL;

  if (!node->persistent) {
    node->ref_count++;
  }

  return node;
}

void vfs_release(vfs_node_t *node) {
  if (node == NULL)
    return;

  if (node->persistent)
    return;

  if (node->ref_count > 0) {
    node->ref_count--;
  }

  if (node->ref_count == 0) {
    kmem_free(node);
  }
}

vfs_node_t *vfs_open(const char *name) {
  vfs_node_t *node = vfs_find(name);
  if (node != NULL) {
    return vfs_retain(node);
  }

  return NULL;
}

void vfs_close(vfs_node_t *node) {
  vfs_release(node);
}

void vfs_register_node(vfs_node_t *node) {
  if (node == NULL)
    return;

  node->next = NULL;
  node->prev = NULL;

  if (node->ref_count == 0)
    node->ref_count = 1;

  if (vfs_root == NULL) {
    vfs_root = node;
  } else {
    vfs_node_t *current = vfs_root;
    while (current->next != NULL) {
      current = current->next;
    }

    current->next = node;
    node->prev = current;
  }

  uint32_t h = vfs_hash(node->name);
  vfs_hash_entry_t *entry = kmem_zalloc(sizeof(vfs_hash_entry_t));
  strcpy(entry->name, node->name);
  entry->node = node;

  entry->next = vfs_hash_table[h];
  vfs_hash_table[h] = entry;

  log_debug(MODULE, "OK: Node Registered: %s", node->name);
}

void vfs_unregister_node(vfs_node_t *node) {
  if (node == NULL)
    return;

  if (node->prev != NULL) {
    node->prev->next = node->next;
  } else {
    vfs_root = node->next;
  }

  if (node->next != NULL) {
    node->next->prev = node->prev;
  }

  uint32_t h = vfs_hash(node->name);
  vfs_hash_entry_t *entry = vfs_hash_table[h];
  vfs_hash_entry_t *prev_entry = NULL;

  while (entry != NULL) {
    if (entry->node == node) {
      if (prev_entry == NULL) {
        vfs_hash_table[h] = entry->next;
      } else {
        prev_entry->next = entry->next;
      }

      kmem_free(entry);
      log_debug(MODULE, "OK: Hash entry removed for: %s", node->name);
      break;
    }
    prev_entry = entry;
    entry = entry->next;
  }

  node->next = NULL;
  node->prev = NULL;

  log_debug(MODULE, "OK: Node Unregistered: %s", node->name);
}

vfs_node_t *vfs_find(const char *name) {
  if (name == NULL)
    return NULL;

  uint32_t h = vfs_hash(name);
  vfs_hash_entry_t *entry = vfs_hash_table[h];

  while (entry != NULL) {
    if (strcmp(entry->name, name) == 0) {
      return entry->node;
    }
    entry = entry->next;
  }

  return NULL;
}

vfs_node_t *vfs_find_path(const char *path) {
  if (path == NULL)
    return NULL;

  const char *search_name = path;
  if (path[0] == '/') {
    search_name = path + 1;
  }

  return vfs_find(search_name);
}