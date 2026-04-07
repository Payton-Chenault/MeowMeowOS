#include "fat16.h"

#include "../../utils/logging/logger.h"
#include "../../lib/string/string.h"
#include "../../drivers/disk/ata.h"
#include "../../kernel_services/kernel_services.h"

#include <stdint.h>

#define MODULE "FAT16"

void fat16_format_drive() {
    log_warning(MODULE, "OK: Formatting Drive");
    uint8_t sector_buffer[512] = {0};
    uint32_t total_sectors = ata_get_total_sectors();

    if (total_sectors == 0) {
        log_error(MODULE, "FATAL: Format Failed. No Sectors Found");
        kpanic("No Disk Sectors Found");
        return;
    }

    kdisk_read_sector(0, sector_buffer);

    uint8_t  sectors_per_cluster = 4;
    uint16_t reserved_sectors = 64;
    uint8_t  fat_count = 2;
    uint16_t root_dir_entries = 512;



    kdisk_write_sector(0, sector_buffer);
    log_info(MODULE, "BPB Formatted. Total Sectors: %d" , total_sectors);

    uint32_t total_clusters = total_sectors / sectors_per_cluster;
    uint16_t sectors_per_fat = (uint16_t)((total_clusters * 2) / 512) + 1;

    sector_buffer[11] = 0x00; sector_buffer[12] = 0x02; // 512
    sector_buffer[13] = sectors_per_cluster;
    sector_buffer[14] = (uint8_t)(reserved_sectors & 0xFF);
    sector_buffer[15] = (uint8_t)((reserved_sectors >> 8) & 0xFF);
    sector_buffer[16] = fat_count;
    sector_buffer[17] = (uint8_t)(root_dir_entries & 0xFF);
    sector_buffer[18] = (uint8_t)((root_dir_entries >> 8) & 0xFF);
    sector_buffer[22] = (uint8_t)(sectors_per_fat & 0xFF);
    sector_buffer[23] = (uint8_t)((sectors_per_fat >> 8) & 0xFF);
    sector_buffer[32] = (uint8_t)(total_sectors & 0xFF);
    sector_buffer[33] = (uint8_t)((total_sectors >> 8) & 0xFF);
    sector_buffer[34] = (uint8_t)((total_sectors >> 16) & 0xFF);
    sector_buffer[35] = (uint8_t)((total_sectors >> 24) & 0xFF);

    kdisk_write_sector(0, sector_buffer);

    memset(sector_buffer, 0, 512);
    sector_buffer[0] = 0xF8; // Media Descriptor (Hard Disk)
    sector_buffer[1] = 0xFF;
    sector_buffer[2] = 0xFF; // End of Cluster Chain marker
    sector_buffer[3] = 0xFF;
    
    uint32_t fat1_start = reserved_sectors;
    uint32_t fat2_start = reserved_sectors + sectors_per_fat;

    kdisk_write_sector(fat1_start, sector_buffer);
    kdisk_write_sector(fat2_start, sector_buffer);

    memset(sector_buffer, 0, 512);
    for (uint32_t i = 1; i < sectors_per_fat; i++) {
        kdisk_write_sector(fat1_start + i, sector_buffer);
        kdisk_write_sector(fat2_start + i, sector_buffer);
    }

    log_info(MODULE, "OK: FAT Tables Formatted");

    uint32_t root_start = fat2_start + sectors_per_fat;
    uint32_t root_size_sectors = (root_dir_entries * 32) / 512;

    for (uint32_t i = 0; i < root_size_sectors; i++) {
        kdisk_write_sector(root_start + i, sector_buffer);
    }

    log_info(MODULE, "OK: Format Complete. Root Start=%d", root_start);
}

void fat16_initialize() {
    uint8_t sector_buffer[512];
    kdisk_read_sector(0, sector_buffer);
    fat16_bpb_t* bpb = (fat16_bpb_t*)sector_buffer;

    char temp_oem[9];
    char temp_label[12];

    temp_oem[8] = '\0';
    temp_label[11] = '\0';

    memcpy(temp_oem, bpb->oem_name, 8);
    memcpy(temp_label, bpb->volume_label, 11);
    log_debug(MODULE, "FOUND: OEM Name=%s", temp_oem);
    log_debug(MODULE, "FOUND: Volume Label=%s", temp_label);
    log_debug(MODULE, "FOUND: Bytes Per Sector=%d", bpb->bytes_per_sector);
    log_debug(MODULE, "FOUND: Sectors Per Cluster=%d", bpb->sectors_per_cluster);
    log_debug(MODULE, "FOUND: Reserved Sectors=%d", bpb->reserved_sectors);
    log_debug(MODULE, "FOUND: Root DIR. Entries=%d", bpb->root_dir_entries);
    log_debug(MODULE, "FOUND: Sectors Per FAT=%d", bpb->sectors_per_fat);
    log_info(MODULE, "Initialized");
}