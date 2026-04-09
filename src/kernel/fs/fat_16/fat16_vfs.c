#include "fat16_vfs.h"
#include "fat16.h"
#include "../../kernel_services/kernel_services.h"
#include "../../lib/string/string.h"
#include <stdbool.h>
#include <stdint.h>

#define MODULE "FAT16_VFS"

static uint32_t fat16_vfs_read(vfs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    return fat16_read_file(node->name, offset, size, buffer);
}

static uint32_t fat16_vfs_write(vfs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    uint32_t new_size = node->length;
    if (offset + size > node->length) {
        new_size = offset + size;
    }

    uint8_t* temp_buffer = (uint8_t*)kmem_zalloc(new_size);
    if (!temp_buffer) {
        return 0;
    }

    if (node->length > 0) {
        fat16_read_file(node->name, 0, node->length, temp_buffer);
    }

    memcpy(temp_buffer + offset, buffer, size);

    if (node->length > 0) {
        fat16_delete_file(node->name);
    }

    fat16_write_file(node->name, temp_buffer, new_size);

    node->length = new_size;

    kmem_free(temp_buffer);
    return size;
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
    node->write = fat16_vfs_write;

    return node;
}