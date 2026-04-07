#ifndef VFS_H
#define VFS_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#define VFS_FILE        0x01
#define VFS_DIRECTORY   0x02
#define VFS_DEVICE      0x03

struct vfs_node;

typedef uint32_t (*vfs_read_t)(struct vfs_node*, uint32_t offset, uint32_t size, uint8_t* buffer);
typedef uint32_t (*vfs_write_t)(struct vfs_node*, uint32_t offset, uint32_t size, uint8_t* buffer);
typedef void (*vfs_open_t)(struct vfs_node*);
typedef void (*vfs_close_t)(struct vfs_node*);

typedef struct vfs_node {
    char name[128];
    uint32_t type;
    uint32_t length;

    vfs_read_t read;
    vfs_write_t write;
    vfs_open_t open;
    vfs_close_t close;

    struct vfs_node* next;
    bool log_use;
} vfs_node_t;

uint32_t vfs_read(vfs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer);
uint32_t vfs_write(vfs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer);
void vfs_register_node(vfs_node_t* node);
vfs_node_t* vfs_find(const char* name);
vfs_node_t* vfs_find_path(const char* path);

#endif