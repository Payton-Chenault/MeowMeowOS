#ifndef PIPE_H
#define PIPE_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "../vfs/vfs.h"
#include "../../arch/x86/sync/spinlock.h"

#define PIPE_BUFFER_SIZE 4096

typedef struct {
    uint8_t buffer[PIPE_BUFFER_SIZE];
    uint32_t head;
    uint32_t tail;
    uint32_t available_bytes;
    
    bool read_closed;
    bool write_closed;
    
    spinlock_t lock;
} pipe_ring_t;

/**
 * @brief Allocates a new pipe and returns the corresponding read and write VFS nodes.
 * @param read_node  Pointer to store the generated read VFS node
 * @param write_node Pointer to store the generated write VFS node
 * @return 0 on success, -1 on failure
 */
int pipe_create(vfs_node_t **read_node, vfs_node_t **write_node);

#endif