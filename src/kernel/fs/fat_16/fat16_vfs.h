#ifndef FAT16_VFS_H
#define FAT16_VFS_H

#include "../vfs/vfs.h"

vfs_node_t* fat16_vfs_open(const char* filename);

#endif