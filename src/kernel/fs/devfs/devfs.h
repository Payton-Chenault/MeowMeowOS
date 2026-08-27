#ifndef DEVFS_H
#define DEVFS_H

#include "../vfs/vfs.h"

void devfs_initialize(void);
void devfs_register_node(vfs_node_t *node);

#endif