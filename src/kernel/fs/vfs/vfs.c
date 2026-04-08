#include "vfs.h"
#include <stdint.h>
#include <stddef.h>
#include "../../utils/logging/logger.h"
#include "../../lib/string/string.h"

#define MODULE "VFS"

vfs_node_t* vfs_root = NULL;

uint32_t vfs_read(vfs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if (node == NULL) return 0;
    if (node->log_use) log_debug(MODULE, "read from node \"%s\"", node->name);
    
    if (node->read != NULL) {
        return node->read(node, offset, size, buffer);
    }
    return 0;
}

uint32_t vfs_write(vfs_node_t* node, uint32_t offset, uint32_t size, uint8_t* buffer) {
    if (node == NULL) return 0;
    if (node->log_use) log_debug(MODULE, "write to node \"%s\"", node->name);
    
    if (node->write != NULL) {
        return node->write(node, offset, size, buffer);
    }
    return 0;
}

void vfs_register_node(vfs_node_t* node) {
    if (node == NULL) return;
    
    // Safety first: ensure the new node's pointers are clean
    node->next = NULL;
    node->prev = NULL;

    if (vfs_root == NULL) {
        vfs_root = node;
    } else {
        vfs_node_t* current = vfs_root;
        while (current->next != NULL) {
            current = current->next;
        }

        // The Handshake!
        current->next = node;  // Old last node points forward to our new node
        node->prev = current;  // Our new node points backward to the old last node
    }

    log_debug(MODULE, "OK: Node Registered: %s", node->name);
}

/**
 * @brief Removes a node from the VFS in O(1) time. 
 * This is the magic of the Doubly Linked List!
 */
void vfs_unregister_node(vfs_node_t* node) {
    if (node == NULL) return;

    // 1. If there's a node behind us, tell it to point to the node in front of us
    if (node->prev != NULL) {
        node->prev->next = node->next;
    } else {
        // If there's no node behind us, WE were the root. Update the root!
        vfs_root = node->next;
    }

    // 2. If there's a node in front of us, tell it to point to the node behind us
    if (node->next != NULL) {
        node->next->prev = node->prev;
    }

    // 3. Sever our ties to the list
    node->next = NULL;
    node->prev = NULL;

    log_debug(MODULE, "OK: Node Unregistered: %s", node->name);
}

vfs_node_t* vfs_find(const char* name) {
    if (vfs_root == NULL || name == NULL) return NULL;

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
    if (path[0] == '/') {
        search_name = path + 1;
    }

    return vfs_find(search_name);
}