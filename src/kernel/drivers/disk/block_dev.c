#include "block_dev.h"
#include "../../drivers/disk/ata.h"
#include "../../fs/vfs/vfs.h"
#include "../../lib/string/string.h"
#include "../../kernel_services/kernel_services.h"
#include "../../utils/logging/logger.h"

#include <stdint.h>

#define MODULE "BLOCK_DEV"
#define SECTOR_SIZE 512

static uint32_t ata_vfs_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; // Unused parameter
    uint8_t sector_buffer[SECTOR_SIZE];
    uint32_t bytes_read = 0;

    while (bytes_read < size) {
        uint32_t current_offset = offset + bytes_read;
        uint32_t sector_lba = current_offset / SECTOR_SIZE;
        uint32_t sector_offset = current_offset % SECTOR_SIZE;

        uint32_t bytes_to_read = SECTOR_SIZE - sector_offset;

        // Don't read past the requested size
        if (bytes_to_read > (size - bytes_read)) {
            bytes_to_read = size - bytes_read;
        }

        ata_read_sector(sector_lba, sector_buffer);
        memcpy(buffer + bytes_read, sector_buffer + sector_offset, bytes_to_read);

        bytes_read += bytes_to_read;
    }

    return bytes_read;
}

static uint32_t ata_vfs_write(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; // Unused parameter
    uint8_t sector_buffer[SECTOR_SIZE];
    uint32_t bytes_written = 0;

    while (bytes_written < size) {
        uint32_t current_offset = offset + bytes_written;
        uint32_t sector_lba = current_offset / SECTOR_SIZE;
        uint32_t sector_offset = current_offset % SECTOR_SIZE;

        uint32_t bytes_to_write = SECTOR_SIZE - sector_offset;
        
        // Don't write past the requested size
        if (bytes_to_write > (size - bytes_written)) {
            bytes_to_write = size - bytes_written;
        }

        // Read-Modify-Write cycle: If we aren't overwriting the whole sector, 
        // we must read the existing data first to avoid destroying it.
        if (bytes_to_write < SECTOR_SIZE) {
            ata_read_sector(sector_lba, sector_buffer);
        }

        // Overlay our new data
        memcpy(sector_buffer + sector_offset, buffer + bytes_written, bytes_to_write);

        // Commit back to disk
        ata_write_sector(sector_lba, sector_buffer);

        bytes_written += bytes_to_write; 
    }

    return bytes_written;
}

void block_device_initialize(void) {
    // kmem_zalloc ensures all fields (like prev/next pointers) start as 0/NULL
    vfs_node_t* hda_node = kmem_zalloc(sizeof(vfs_node_t));
    if (hda_node == NULL) {
        kpanic("Failed to allocate memory for hda VFS node");
    }

    // Set Name (Safely)
    memcpy(hda_node->name, "hda", 4); 

    // Set Node Metadata
    hda_node->type = VFS_DEVICE; 
    hda_node->length = (ata_get_total_sectors() * SECTOR_SIZE); 
    hda_node->log_use = false; 
    
    // Bind Functions
    hda_node->read = ata_vfs_read;
    hda_node->write = ata_vfs_write;

    // Register with the Virtual File System
    vfs_register_node(hda_node);
    
    log_info(MODULE, "Initialized 'hda' (Size: %d KB)", hda_node->length / 1024);
}