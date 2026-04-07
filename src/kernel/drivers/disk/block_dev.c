#include "block_dev.h"
#include "ata.h"
#include "../../lib/string/string.h"
#include "../../kernel_services/kernel_services.h"
#include "../../utils/logging/logger.h"
#include <stdint.h>

#define MODULE "BLOCK_DEV"
#define SECTOR_SIZE 512

static uint32_t ata_vfs_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    uint8_t sector_buffer[SECTOR_SIZE];
    uint32_t bytes_read = 0;

    while (bytes_read < size) {
        uint32_t current_offset = offset + bytes_read;
        uint32_t sector_lba = current_offset / SECTOR_SIZE;
        uint32_t sector_offset = current_offset % SECTOR_SIZE;

        uint32_t bytes_to_read = SECTOR_SIZE - sector_offset;

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
    uint8_t sector_buffer[SECTOR_SIZE];
    uint32_t bytes_written = 0;

    while (bytes_written < size) {
        uint32_t current_offset = offset + bytes_written;
        uint32_t sector_lba = current_offset / SECTOR_SIZE;
        uint32_t sector_offset = current_offset % SECTOR_SIZE;

        uint32_t bytes_to_write = SECTOR_SIZE - sector_offset;
        if (bytes_to_write > (size - bytes_written)) {
            bytes_to_write = size - bytes_written;
        }

        if (bytes_to_write < SECTOR_SIZE) {
            ata_read_sector(sector_lba, sector_buffer);
        }

        memcpy(sector_buffer + sector_offset, buffer + bytes_written, bytes_to_write);

        ata_write_sector(sector_lba, sector_buffer);

        bytes_written += bytes_to_write; 
    }

    return bytes_written;
}

void block_device_initialize(void) {
    vfs_node_t* hda_node = kmem_zalloc(sizeof(vfs_node_t));

    strcpy(hda_node->name, "hda");
    hda_node->type = VFS_DEVICE;

    hda_node->length = (ata_get_total_sectors() * 512); 
    

    hda_node->read = ata_vfs_read;
    hda_node->write = ata_vfs_write;

    vfs_register_node(hda_node);
    log_info(MODULE, "Initialized (Size: %d KB)", hda_node->length / 1024);
}