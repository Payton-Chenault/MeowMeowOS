#ifndef VGA_VFS_H
#define VGA_VFS_H

#include "../../fs/vfs/vfs.h"

static uint32_t vga_write(vfs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer);
void vga_vfs_initialize(void);

#endif