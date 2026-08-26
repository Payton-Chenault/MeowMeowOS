#include "pipe.h"
#include "../../mem/heap/heap.h"
#include "../../arch/x86/task/task.h"
#include "../../lib/string/string.h"
#include "../../utils/logging/logger.h"

#define MODULE "PIPE"

static uint32_t pipe_vfs_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)offset;
    
    pipe_ring_t *pipe = (pipe_ring_t *)node->ptr;
    if (!pipe || buffer == NULL || size == 0) return 0;

    uint32_t bytes_read = 0;

    while (bytes_read < size) {
        spinlock_acquire(&pipe->lock);

        if (pipe->available_bytes > 0) {
            buffer[bytes_read++] = pipe->buffer[pipe->head];
            pipe->head = (pipe->head + 1) % PIPE_BUFFER_SIZE;
            pipe->available_bytes--;
            spinlock_release(&pipe->lock);
        } else {
            // Stream return: if we already collected data, return immediately
            if (bytes_read > 0) {
                spinlock_release(&pipe->lock);
                break;
            }

            // End-of-File: write endpoint is closed and buffer is empty
            if (pipe->write_closed) {
                spinlock_release(&pipe->lock);
                break;
            }

            spinlock_release(&pipe->lock);
            task_yield();
        }
    }

    return bytes_read;
}

static uint32_t pipe_vfs_write(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)offset;
    
    pipe_ring_t *pipe = (pipe_ring_t *)node->ptr;
    if (!pipe || buffer == NULL || size == 0) return 0;

    uint32_t bytes_written = 0;

    while (bytes_written < size) {
        spinlock_acquire(&pipe->lock);

        if (pipe->read_closed) {
            spinlock_release(&pipe->lock);
            break; // Reader disconnected
        }

        if (pipe->available_bytes < PIPE_BUFFER_SIZE) {
            pipe->buffer[pipe->tail] = buffer[bytes_written++];
            pipe->tail = (pipe->tail + 1) % PIPE_BUFFER_SIZE;
            pipe->available_bytes++;
            spinlock_release(&pipe->lock);
        } else {
            spinlock_release(&pipe->lock);
            task_yield();
        }
    }

    return bytes_written;
}

static void pipe_vfs_close_read(vfs_node_t *node) {
    pipe_ring_t *pipe = (pipe_ring_t *)node->ptr;
    if (!pipe) return;

    spinlock_acquire(&pipe->lock);
    pipe->read_closed = true;
    bool should_free = pipe->write_closed;
    spinlock_release(&pipe->lock);

    if (should_free) {
        mem_free(pipe);
    }
    mem_free(node);
}

static void pipe_vfs_close_write(vfs_node_t *node) {
    pipe_ring_t *pipe = (pipe_ring_t *)node->ptr;
    if (!pipe) return;

    spinlock_acquire(&pipe->lock);
    pipe->write_closed = true;
    bool should_free = pipe->read_closed;
    spinlock_release(&pipe->lock);

    if (should_free) {
        mem_free(pipe);
    }
    mem_free(node);
}

int pipe_create(vfs_node_t **read_node, vfs_node_t **write_node) {
    pipe_ring_t *pipe = (pipe_ring_t *)mem_zalloc(sizeof(pipe_ring_t));
    if (!pipe) return -1;

    memset(&pipe->lock, 0, sizeof(spinlock_t));
    pipe->head = 0;
    pipe->tail = 0;
    pipe->available_bytes = 0;
    pipe->read_closed = false;
    pipe->write_closed = false;

    *read_node = (vfs_node_t *)mem_zalloc(sizeof(vfs_node_t));
    if (!*read_node) {
        mem_free(pipe);
        return -1;
    }
    (*read_node)->type = VFS_PIPE;
    (*read_node)->ref_count = 1;
    (*read_node)->persistent = false;
    strcpy((*read_node)->name, "pipe_read");
    (*read_node)->ptr = pipe;
    (*read_node)->read = pipe_vfs_read;
    (*read_node)->write = NULL;
    (*read_node)->close = pipe_vfs_close_read;

    *write_node = (vfs_node_t *)mem_zalloc(sizeof(vfs_node_t));
    if (!*write_node) {
        mem_free(*read_node);
        mem_free(pipe);
        return -1;
    }
    (*write_node)->type = VFS_PIPE;
    (*write_node)->ref_count = 1;
    (*write_node)->persistent = false;
    strcpy((*write_node)->name, "pipe_write");
    (*write_node)->ptr = pipe;
    (*write_node)->read = NULL;
    (*write_node)->write = pipe_vfs_write;
    (*write_node)->close = pipe_vfs_close_write;

    return 0;
}