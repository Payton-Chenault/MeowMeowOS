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