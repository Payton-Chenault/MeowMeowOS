#ifndef VFS_H
#define VFS_H

#include <stdbool.h>
#include <stdint.h>

#define VFS_FILE 0x01
#define VFS_DIRECTORY 0x02
#define VFS_DEVICE 0x03

#define VFS_MODE_OTHER_EXEC  0001
#define VFS_MODE_OTHER_WRITE 0002
#define VFS_MODE_OTHER_READ  0004
#define VFS_MODE_GROUP_EXEC  0010
#define VFS_MODE_GROUP_WRITE 0020
#define VFS_MODE_GROUP_READ  0040
#define VFS_MODE_USER_EXEC   0100
#define VFS_MODE_USER_WRITE  0200
#define VFS_MODE_USER_READ   0400

#define VFS_DEFAULT_FILE_MODE 0755
#define VFS_DEFAULT_DIR_MODE  0755
#define VFS_DEFAULT_DEV_MODE  0600

#define VFS_ACCESS_READ  0x01
#define VFS_ACCESS_WRITE 0x02
#define VFS_ACCESS_EXEC  0x04

struct vfs_node;

typedef struct vfs_node {
  char name[256];
  bool log_use;

  uint32_t type;
  uint32_t length;

  uint32_t uid;
  uint32_t gid;
  uint16_t mode;
  uint32_t flags;

  uint32_t ref_count;
  bool persistent;

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

bool vfs_is_device(const vfs_node_t *node);

#endif