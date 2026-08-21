#ifndef VFS_H
#define VFS_H

#include <stdbool.h>
#include <stdint.h>

#define VFS_FILE 0x01
#define VFS_DIRECTORY 0x02
#define VFS_DEVICE 0x03

struct vfs_node;

typedef struct vfs_node {
  char name[256];
  bool log_use;

  uint32_t type;
  uint32_t length;

  // Lifecycle management
  uint32_t ref_count;
  bool persistent;   // true for stdin/stdout/stderr/global device nodes

  struct vfs_node *prev;
  struct vfs_node *next;

  uint32_t (*read)(struct vfs_node *node, uint32_t offset, uint32_t size,
                   uint8_t *buffer);
  uint32_t (*write)(struct vfs_node *node, uint32_t offset, uint32_t size,
                    uint8_t *buffer);
} vfs_node_t;

void vfs_register_node(vfs_node_t *node);
void vfs_unregister_node(vfs_node_t *node);

vfs_node_t *vfs_find(const char *name);
vfs_node_t *vfs_find_path(const char *path);

uint32_t vfs_read(vfs_node_t *node, uint32_t offset, uint32_t size,
                  uint8_t *buffer);
uint32_t vfs_write(vfs_node_t *node, uint32_t offset, uint32_t size,
                   uint8_t *buffer);

vfs_node_t *vfs_retain(vfs_node_t *node);
void vfs_release(vfs_node_t *node);

vfs_node_t *vfs_open(const char *name);
void vfs_close(vfs_node_t *node);

#endif