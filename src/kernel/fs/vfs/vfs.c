#include "vfs.h"
#include <stdint.h>
#include "../../utils/logging/logger.h"
#include "../../lib/string/string.h"

#define MODULE "VFS"

vfs_node_t* vfs_root = NULL;

uint32_t vfs_read(vfs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if(node->log_use) log_debug(MODULE, "read from node \"%s\"", node->name);
    if (node && node->read) {
        return node->read(node, offset, size, buffer);
    }
    return 0;
}

uint32_t vfs_write(vfs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if(node->log_use) log_debug(MODULE, "read from node \"%s\"", node->name);
    if (node && node->write) {
        return node->write(node, offset, size, buffer);
    }
    return 0;
}

void vfs_register_node(vfs_node_t* node) {
    if (node == NULL) return;
    node->next = NULL;
    if (!vfs_root) {
        vfs_root = node;
    } else {
        vfs_node_t* current = vfs_root;
        while(current->next) {
            current = current->next;
        }

        current->next = node;
    }

    log_debug(MODULE, "OK: Node Registered: %s", node->name);
}

vfs_node_t* vfs_find(const char* name) {
    if (vfs_root == NULL || name == NULL) {
        return NULL;
    }

    vfs_node_t* current = vfs_root;

    while (current != NULL) {
        if (strcmp(current->name, name) == 0) {
            return current;
        } 

        current = current->next;
    }

    return NULL;
}

vfs_node_t* vfs_find_path(const char* path) {
    if (path == NULL) return NULL;

    const char* search_name = path;
    if(path[0] == '/') {
        search_name = path + 1;
    }

    return vfs_find(search_name);
}

