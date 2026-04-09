#include "fat16_vfs.h"
#include "fat16.h"
#include "../../kernel_services/kernel_services.h"
#include "../../lib/string/string.h"
#include <stdbool.h>
#include <stdint.h>

#define MODULE "FAT16_VFS"

static uint32_t fat16_vfs_read(vfs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if (offset >= node->length) return 0;

    uint32_t bytes_to_read = size;
    if (offset + bytes_to_read > node->length) {
        bytes_to_read = node->length - offset;
    }

    uint8_t* temp_buffer = (uint8_t*)kmem_zalloc(node->length);
    fat16_read_file(node->name, temp_buffer);

    memcpy(buffer, temp_buffer + offset, bytes_to_read);
    kmem_free(temp_buffer);

    return bytes_to_read;
}

vfs_node_t* fat16_vfs_open(const char* filename) {
    uint32_t file_size = fat16_get_file_size(filename);

    if (file_size == 0) {
        return NULL;
    }

    vfs_node_t* node = (vfs_node_t*)kmem_zalloc(sizeof(vfs_node_t));

    strcpy(node->name, filename);
    node->type = VFS_FILE;
    node->length = file_size;
    node->log_use = false;

    node->read = fat16_vfs_read;
    node->write = NULL; // TODO;

    return node;
}