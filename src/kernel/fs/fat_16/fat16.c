#include "fat16.h"

#include "../../utils/logging/logger.h"
#include "../../lib/string/string.h"
#include "../../drivers/disk/ata.h"
#include "../../kernel_services/kernel_services.h"

#include <stdbool.h>
#include <stdint.h>

#define MODULE "FAT16"

typedef struct {
    uint32_t root_dir_lba;
    uint32_t root_dir_sectors;
    uint32_t data_region_lba;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_cluster;
    uint16_t reserved_sectors;
} fat16_state_t;

static fat16_state_t fs_state;


/**
 * @brief Converts "test.txt" to "TEST    TXT"
 * 
 * @param input The input filename
 * @param output The fat16 compatable filename
 */
static void fat16_format_filename(const char* input, char* output) {
    memset(output, ' ', 11);
    int i = 0; 
    int j = 0;

    while (input[i] != '.' && input[i] != '\0' && j < 8) {
        char c = input[i++];
        if (c >= 'a' && c <= 'z') c -= 32;
        output[j++] = c;
    }

    while (input[i] != '.' && input[i] != '\0') i++;
    if (input[i] == '.') i++;

    j = 8;
    while (input[i] != '\0' && j < 11) {
        char c = input[i++];
        if (c >= 'a' && c <= 'z') c -= 32;
        output[j++] = c;
    }
}

static uint16_t fat16_find_free_cluster() {
    uint8_t buffer[512];

    for (uint32_t i = 0; i < fs_state.sectors_per_fat; i++) {
        kdisk_read_sector(fs_state.reserved_sectors + i, buffer);
        uint16_t* entries = (uint16_t*)buffer;

        for (int j = 0; j < 256; j++) {
            if (i == 0 && j < 2) continue;

            if (entries[j] == 0x0000) {
                return (i * 256) + j;
            }
        }
    }

    return 0xFFFF; // Full Disk
}

static uint16_t fat16_get_fat_entry(uint16_t cluster) {
    uint8_t buffer[512];

    uint32_t fat_sector_offset = (cluster * 2) / 512;
    uint32_t entry_offset = (cluster * 2) % 512;

    kdisk_read_sector(fs_state.reserved_sectors + fat_sector_offset, buffer);

    return *(uint16_t*)&buffer[entry_offset];
}

static void fat16_set_fat_entry(uint16_t cluster, uint16_t value) {
    uint8_t buffer[512];

    uint32_t fat_sector_offset = (cluster * 2) / 512;
    uint32_t entry_offset = (cluster * 2) % 512;

    kdisk_read_sector(fs_state.reserved_sectors + fat_sector_offset, buffer);
    *(uint16_t*)&buffer[entry_offset] = value;

    // FAT 1
    kdisk_write_sector(fs_state.reserved_sectors + fat_sector_offset, buffer);

    // FAT 2
    kdisk_write_sector(fs_state.reserved_sectors + fs_state.sectors_per_fat + fat_sector_offset, buffer);
}

uint32_t fat16_get_file_size(const char *filename) {
    char fat_name[11];
    fat16_format_filename(filename, fat_name);

    uint8_t root_buf[512];
    for (uint32_t s = 0; s < fs_state.root_dir_sectors; s++) {
        kdisk_read_sector(fs_state.root_dir_lba + s, root_buf);
        fat16_dir_entry_t* entries = (fat16_dir_entry_t*)root_buf;

    for (int i = 0; i < 16; i++) {
            if (entries[i].filename[0] == 0x00) return 0; // End of directory
            if ((uint8_t)entries[i].filename[0] == 0xE5) continue; // Deleted file
            if (entries[i].attributes == 0x0F) continue; // Long File Name

            // Check if all 11 characters match
            bool match = true;
            for(int j = 0; j < 11; j++) {
                if(entries[i].filename[j] != fat_name[j]) {
                    match = false; 
                    break;
                }
            }

            if (match) {
                return entries[i].file_size;
            }
        }
    }
    return 0;
}

uint32_t fat16_read_file(const char* filename, uint8_t* buffer) {
    char fat_name[11];
    fat16_format_filename(filename, fat_name);

    uint8_t root_buf[512];
    fat16_dir_entry_t target_entry;
    bool found = false;

    for (uint32_t s = 0; s < fs_state.root_dir_sectors; s++) {
        kdisk_read_sector(fs_state.root_dir_lba + s, root_buf);
        fat16_dir_entry_t* entries = (fat16_dir_entry_t*)root_buf;

        for (int i = 0; i < 16; i++) {
            if (entries[i].filename[0] == 0x00) goto search_done;
            if ((uint8_t)entries[i].filename[0] == 0xE5) continue;

            bool match = true;
            for(int j = 0; j < 11; j++) {
                if(entries[i].filename[j] != fat_name[j]) {
                    match = false; 
                    break;
                }
            }

            if (match) {
                target_entry = entries[i];
                found = true;
                goto search_done;
            }
        }
    }

search_done:
    if (!found) {
        log_error(MODULE, "FAILED: File not found: %s", filename);
        return 0;
    }

    uint16_t current_cluster = target_entry.cluster_low;
    uint32_t bytes_read = 0;
    uint32_t file_size = target_entry.file_size;
    uint8_t sector_buf[512];

    while (current_cluster >= 2 && current_cluster < 0xFFF8 && bytes_read < file_size) {
        uint32_t data_lba = fs_state.data_region_lba + (current_cluster - 2) * fs_state.sectors_per_cluster;

        for (int s = 0; s < fs_state.sectors_per_cluster; s++) {
            if (bytes_read >= file_size) break;

            kdisk_read_sector(data_lba + s, sector_buf);

            uint32_t bytes_to_copy = 512;
            if (file_size - bytes_read < 512) {
                bytes_to_copy = file_size - bytes_read;
            }

            memcpy(buffer + bytes_read, sector_buf, bytes_to_copy);
            bytes_read += bytes_to_copy;
        }

        current_cluster = fat16_get_fat_entry(current_cluster);
    }

    log_info(MODULE, "OK: Read %d bytes from %s", bytes_read, filename);
    return bytes_read;
}

void fat16_write_file(const char* filename, uint8_t* data, uint32_t size) {
    char fat_name[11];
    fat16_format_filename(filename, fat_name);

    uint32_t bytes_per_cluster = fs_state.sectors_per_cluster * 512;
    uint32_t clusters_needed = (size + (bytes_per_cluster - 1)) / bytes_per_cluster;

    uint16_t first_cluster = 0;
    uint16_t prev_cluster = 0;

    for (uint32_t i = 0; i < clusters_needed; i++) {
        uint16_t current_cluster = fat16_find_free_cluster();
        if(current_cluster == 0xFFFF) {kpanic("Disk Full!");}

        if (i == 0) first_cluster = current_cluster;

        if (prev_cluster != 0) fat16_set_fat_entry(prev_cluster, current_cluster);
        fat16_set_fat_entry(current_cluster, 0xFFFF);

        uint32_t data_lba = fs_state.data_region_lba + (current_cluster - 2) * fs_state.sectors_per_cluster;

        for (int s = 0; s < fs_state.sectors_per_cluster; s++) {
            uint32_t offset = (i * bytes_per_cluster) + (s * 512);
            if (offset < size) {
                kdisk_write_sector(data_lba + s, data + offset);
            }
        }

        prev_cluster = current_cluster;
    }

    uint8_t root_buf[512];
    for (uint32_t s = 0; s < fs_state.root_dir_sectors; s++) {
        kdisk_read_sector(fs_state.root_dir_lba + s, root_buf);
        fat16_dir_entry_t* entries = (fat16_dir_entry_t*)root_buf;

        for (int i = 0; i < 16; i++) {
            if (entries[i].filename[0] == 0x00 || entries[i].filename[0] == 0xE5) {
                memcpy(entries[i].filename, fat_name, 11);
                entries[i].cluster_low = first_cluster;
                entries[i].file_size = size;
                entries[i].attributes = 0x20;

                kdisk_write_sector(fs_state.root_dir_lba + s, root_buf);
                return;
            }
        }
    }
}

void fat16_list(fat16_visitor_t visitor) {
    uint8_t buffer[512];
    
    for (uint32_t s = 0; s < fs_state.root_dir_sectors; s++) {
        kdisk_read_sector(fs_state.root_dir_lba + s, buffer);
        fat16_dir_entry_t* entries = (fat16_dir_entry_t*)buffer;

        for (int i = 0; i < 16; i++) {
            if(entries[i].filename[0] == 0x00) return;
            if((uint8_t)entries[i].filename[0] == 0xE5) continue;
            if(entries[i].attributes == 0x0F) continue;

            visitor(&entries[i]);
        }
    }
}

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

    fs_state.reserved_sectors = bpb->reserved_sectors;
    fs_state.sectors_per_fat = bpb->sectors_per_fat;
    fs_state.sectors_per_cluster = bpb->sectors_per_cluster;

    fs_state.root_dir_lba = bpb->reserved_sectors + (bpb->fat_count * bpb->sectors_per_fat);
    fs_state.root_dir_sectors = (bpb->root_dir_entries * 32) / 512;
    fs_state.data_region_lba = fs_state.root_dir_lba + fs_state.root_dir_sectors;

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