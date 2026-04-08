#include "fat16.h"
#include "../../utils/logging/logger.h"
#include "../../lib/string/string.h" // CRITICAL: This provides the memcmp declaration
#include "../../drivers/disk/ata.h"
#include "../../kernel_services/kernel_services.h"

#include <stdbool.h>
#include <stdint.h>

#define MODULE "FAT16"

typedef struct {
    bool is_mounted;
    uint32_t root_dir_lba;
    uint32_t root_dir_sectors;
    uint32_t data_region_lba;
    uint16_t sectors_per_fat;
    uint16_t sectors_per_cluster;
    uint16_t reserved_sectors;
} fs_state_t;

static fs_state_t fs_state;

static void format_filename_to_fat(const char* input, char* output) {
    memset(output, ' ', 11);
    int i = 0, j = 0;

    // Handle Name (8 chars)
    while (input[i] && input[i] != '.' && j < 8) {
        char c = input[i++];
        if (c >= 'a' && c <= 'z') c -= 32;
        output[j++] = c;
    }

    // Skip to extension
    while (input[i] && input[i] != '.') i++;
    if (input[i] == '.') i++;

    // Handle Extension (3 chars)
    j = 8;
    while (input[i] && j < 11) {
        char c = input[i++];
        if (c >= 'a' && c <= 'z') c -= 32;
        output[j++] = c;
    }
}

static uint16_t get_fat_entry(uint16_t cluster) {
    uint8_t buf[512];
    uint32_t lba = fs_state.reserved_sectors + (cluster * 2 / 512);
    uint32_t offset = (cluster * 2 % 512);
    kdisk_read_sector(lba, buf);
    return *(uint16_t*)&buf[offset];
}

static void set_fat_entry(uint16_t cluster, uint16_t value) {
    uint8_t buf[512];
    uint32_t lba_off = (cluster * 2 / 512);
    uint32_t offset = (cluster * 2 % 512);

    kdisk_read_sector(fs_state.reserved_sectors + lba_off, buf);
    *(uint16_t*)&buf[offset] = value;

    // Fixed: Removed the stray '=' that was here
    kdisk_write_sector(fs_state.reserved_sectors + lba_off, buf);
    kdisk_write_sector(fs_state.reserved_sectors + fs_state.sectors_per_fat + lba_off, buf);
}

static uint16_t find_free_cluster(void) {
    uint8_t buf[512];
    for (uint16_t s = 0; s < fs_state.sectors_per_fat; s++) {
        kdisk_read_sector(fs_state.reserved_sectors + s, buf);
        uint16_t* entries = (uint16_t*)buf;
        for (int i = 0; i < 256; i++) {
            if (s == 0 && i < 2) continue; 
            if (entries[i] == 0x0000) return (s * 256) + i;
        }
    }
    return 0xFFFF;
}

static bool is_dir_empty(uint16_t cluster) {
    uint8_t buf[512];
    uint32_t lba = fs_state.data_region_lba + (cluster - 2) * fs_state.sectors_per_cluster;

    // Read the first sector of the directory
    kdisk_read_sector(lba, buf);
    fat16_dir_entry_t* entries = (fat16_dir_entry_t*)buf;

    for (int i = 0; i < 16; i++) {
        // 0x00 means end of list, so it's effectively empty
        if (entries[i].filename[0] == 0x00) return true;
        
        // Skip the tombstone
        if ((uint8_t)entries[i].filename[0] == 0xE5) continue;

        // Skip the "." and ".." entries
        if (entries[i].filename[0] == '.') {
            if (entries[i].filename[1] == ' ' || entries[i].filename[1] == '.') continue;
        }

        // If we found anything else, the directory is NOT empty!
        return false;
    }

    return true;
}

void fat16_format_drive(fat16_progress_callback_t callback) {
    // Buffer 1: For the Boot Sector
    uint8_t boot_buf[512] = {0};
    uint32_t total_sectors = ata_get_total_sectors();

    kdisk_read_sector(0, boot_buf);
    
    fat16_bpb_t* bpb = (fat16_bpb_t*)boot_buf;
    memcpy(bpb->oem_name, "MEOWMEOW", 8);
    bpb->bytes_per_sector = 512;
    bpb->sectors_per_cluster = 4;
    bpb->reserved_sectors = 256;
    bpb->fat_count = 2;
    bpb->root_dir_entries = 512;
    bpb->total_sectors_long = total_sectors;
    bpb->media_descriptor = 0xF8;
    
    uint32_t total_clusters = total_sectors / bpb->sectors_per_cluster;
    bpb->sectors_per_fat = (total_clusters * 2 / 512) + 1;
    
    kdisk_write_sector(0, boot_buf);

    // CACHE THE VALUES: Pull these out of the struct before doing anything else
    uint32_t bpb_reserved_sectors = bpb->reserved_sectors;
    uint32_t bpb_sectors_per_fat = bpb->sectors_per_fat;
    uint32_t root_sectors = (bpb->root_dir_entries * 32) / 512;
    
    uint32_t total_work = (bpb_sectors_per_fat * 2) + root_sectors;
    uint32_t progress = 0;

    // Buffer 2: For writing the FAT and Root Directory
    uint8_t fat_buf[512] = {0};
    fat_buf[0] = 0xF8; fat_buf[1] = 0xFF; fat_buf[2] = 0xFF; fat_buf[3] = 0xFF;

    for (uint32_t i = 0; i < bpb_sectors_per_fat; i++) {
        kdisk_write_sector(bpb_reserved_sectors + i, fat_buf);
        kdisk_write_sector(bpb_reserved_sectors + bpb_sectors_per_fat + i, fat_buf);
        
        // Clear the FAT signature after the first sector is written
        if (i == 0) memset(fat_buf, 0, 512); 
        
        progress += 2;
        if (callback) callback(progress, total_work);
    }

    uint32_t root_lba = bpb_reserved_sectors + (2 * bpb_sectors_per_fat);
    for (uint32_t i = 0; i < root_sectors; i++) {
        kdisk_write_sector(root_lba + i, fat_buf);
        progress++;
        if (callback) callback(progress, total_work);
    }

    fat16_initialize();
}

void fat16_initialize(void) {
    uint8_t buf[512];
    kdisk_read_sector(0, buf);
    fat16_bpb_t* bpb = (fat16_bpb_t*)buf;

    if (bpb->bytes_per_sector != 512 || bpb->fat_count != 2) {
        log_warning(MODULE, "Disk is unformatted or corrupt");
        fs_state.is_mounted = false;
        return;
    }

    fs_state.reserved_sectors = bpb->reserved_sectors;
    fs_state.sectors_per_fat = bpb->sectors_per_fat;
    fs_state.sectors_per_cluster = bpb->sectors_per_cluster;
    fs_state.root_dir_lba = bpb->reserved_sectors + (bpb->fat_count * bpb->sectors_per_fat);
    fs_state.root_dir_sectors = (bpb->root_dir_entries * 32) / 512;
    fs_state.data_region_lba = fs_state.root_dir_lba + fs_state.root_dir_sectors;

    fs_state.is_mounted = true;
    log_info(MODULE, "Initialized at LBA %d", fs_state.root_dir_lba);
}

uint32_t fat16_get_file_size(const char* filename) {
    char fat_name[11];
    format_filename_to_fat(filename, fat_name);

    uint8_t buf[512];
    for (uint32_t s = 0; s < fs_state.root_dir_sectors; s++) {
        kdisk_read_sector(fs_state.root_dir_lba + s, buf);
        fat16_dir_entry_t* entries = (fat16_dir_entry_t*)buf;
        for (int i = 0; i < 16; i++) {
            if (entries[i].filename[0] == 0x00) return 0;
            if (memcmp(entries[i].filename, fat_name, 8) == 0 && 
                memcmp(entries[i].extension, fat_name + 8, 3) == 0) {
                return entries[i].file_size;
            }
        }
    }
    return 0;
}

void fat16_delete_file(const char* filename) {
    if (!fs_state.is_mounted) {
        kprintf("Error: Filesystem is not mounted.\n");
        return;
    }

    char fat_name[11];
    format_filename_to_fat(filename, fat_name);

    uint8_t buf[512];
    uint16_t start_cluster = 0;
    bool found = false;

    for (uint32_t s = 0; s < fs_state.root_dir_sectors; s++) {
        kdisk_read_sector(fs_state.root_dir_lba + s, buf);
        fat16_dir_entry_t* entries = (fat16_dir_entry_t*)buf;

        for (int i = 0; i < 16; i++) {
            if (entries[i].filename[0] == 0x00) {
                break;
            }

            if ((uint8_t)entries[i].filename[0] != 0xE5 
            && memcmp(entries[i].filename, fat_name, 8) == 0 
            && memcmp(entries[i].extension, fat_name + 8, 3) == 0) {
                start_cluster = entries[i].cluster_low;
                entries[i].filename[0] = 0xE5;

                kdisk_write_sector(fs_state.root_dir_lba + s, buf);
                found = true;
                break;
            }
        }
        if (found) break;
    }

    if (!found) {
        kprintf("Error: File '%s' not found.\n", filename);
        return;
    }

    uint16_t current = start_cluster;

    while (current >= 2 && current < 0xFFF8) {
        uint16_t next = get_fat_entry(current);
        set_fat_entry(current, 0x0000);

        current = next;
    }

    log_info(MODULE, "Deleated file: %s", filename);
}


void fat16_create_dir(const char* dirname) {
    if (!fs_state.is_mounted) {
        kprintf("Error: Filesystem is not mounted. Please format the drive\n");
        return;
    }

    char fat_name[11];
    format_filename_to_fat(dirname, fat_name);

    uint16_t dir_cluster = find_free_cluster();
    if(dir_cluster == 0xFFFF) kpanic("Disk Full: Cannot create directory");
    set_fat_entry(dir_cluster, 0xFFFF);

    uint8_t cluster_buf[512] = {0};
    fat16_dir_entry_t* payload_entries = (fat16_dir_entry_t*)cluster_buf;

    memset(payload_entries[0].filename, ' ', 11);
    payload_entries[0].filename[0] = '.';
    payload_entries[0].attributes = 0x10;
    payload_entries[0].cluster_low = dir_cluster;

    memset(payload_entries[1].filename, ' ', 11);
    payload_entries[1].filename[0] = '.';
    payload_entries[1].filename[1] = '.';
    payload_entries[1].attributes = 0x10; 
    payload_entries[1].cluster_low = 0x0000;

    uint32_t data_lba = fs_state.data_region_lba + (dir_cluster - 2) * fs_state.sectors_per_cluster;

    kdisk_write_sector(data_lba, cluster_buf);

    uint8_t root_buf[512];
    for(uint32_t s = 0; s < fs_state.root_dir_sectors; s++) {
        kdisk_read_sector(fs_state.root_dir_lba + s, root_buf);
        fat16_dir_entry_t* root_entries = (fat16_dir_entry_t*)root_buf;

        for (int i = 0; i < 16; i++) {
            if (root_entries[i].filename[0] == 0x00 || (uint8_t)root_entries[i].filename[0] == 0xE5) {
                memcpy(root_entries[i].filename, fat_name, 11);
                root_entries[i].attributes = 0x10;
                root_entries[i].cluster_low = dir_cluster;
                root_entries[i].file_size = 0;

                kdisk_write_sector(fs_state.root_dir_lba + s, root_buf);
                log_info(MODULE, "Created Directory: %s", dirname);
                return;
            }
        }
    }
}

void fat16_delete_dir(const char* dirname) {
    if (!fs_state.is_mounted) return;

    char fat_name[11];
    format_filename_to_fat(dirname, fat_name);

    uint8_t buf[512];
    bool found = false;

    for (uint32_t s = 0; s < fs_state.root_dir_sectors; s++) {
        kdisk_read_sector(fs_state.root_dir_lba + s, buf);
        fat16_dir_entry_t* entries = (fat16_dir_entry_t*)buf;

        for (int i = 0; i < 16; i++) {
            if (entries[i].filename[0] == 0x00) break;

            if ((uint8_t)entries[i].filename[0] != 0xE5 &&
                memcmp(entries[i].filename, fat_name, 8) == 0 &&
                (entries[i].attributes & 0x10)) { // Ensure it IS a directory
                
                // Check if it's empty before killing it
                if (!is_dir_empty(entries[i].cluster_low)) {
                    kprintf("Error: Directory not empty.\n");
                    return;
                }

                // Save cluster and drop tombstone
                uint16_t cluster_to_free = entries[i].cluster_low;
                entries[i].filename[0] = 0xE5;

                kdisk_write_sector(fs_state.root_dir_lba + s, buf);

                // Free the cluster chain
                uint16_t current = cluster_to_free;
                while (current >= 2 && current < 0xFFF8) {
                    uint16_t next = get_fat_entry(current);
                    set_fat_entry(current, 0x0000);
                    current = next;
                }

                log_info(MODULE, "Deleted directory: %s", dirname);
                return;
            }
        }
    }
    kprintf("Error: Directory not found.\n");
}



uint32_t fat16_read_file(const char* filename, uint8_t* buffer) {
    if (!fs_state.is_mounted) {
        kprintf("Error: Filesystem is not mounted. Please format the drive\n");
        return 0;
    }
    char fat_name[11];
    format_filename_to_fat(filename, fat_name);

    uint8_t buf[512];
    fat16_dir_entry_t entry;
    bool found = false;

    for (uint32_t s = 0; s < fs_state.root_dir_sectors; s++) {
        kdisk_read_sector(fs_state.root_dir_lba + s, buf);
        fat16_dir_entry_t* entries = (fat16_dir_entry_t*)buf;
        for (int i = 0; i < 16; i++) {
            if (memcmp(entries[i].filename, fat_name, 8) == 0 && 
                memcmp(entries[i].extension, fat_name + 8, 3) == 0) {
                entry = entries[i];
                found = true;
                break;
            }
        }
        if (found) break;
    }

    if (!found) return 0;

    uint16_t cluster = entry.cluster_low;
    uint32_t bytes_read = 0;
    while (cluster < 0xFFF8 && bytes_read < entry.file_size) {
        uint32_t lba = fs_state.data_region_lba + (cluster - 2) * fs_state.sectors_per_cluster;
        for (int s = 0; s < fs_state.sectors_per_cluster && bytes_read < entry.file_size; s++) {
            uint8_t sector[512];
            kdisk_read_sector(lba + s, sector);
            uint32_t to_copy = (entry.file_size - bytes_read > 512) ? 512 : (entry.file_size - bytes_read);
            memcpy(buffer + bytes_read, sector, to_copy);
            bytes_read += to_copy;
        }
        cluster = get_fat_entry(cluster);
    }
    return bytes_read;
}

void fat16_write_file(const char* filename, uint8_t* data, uint32_t size) {
    if (!fs_state.is_mounted) {
        kprintf("Error: Filesystem is not mounted. Please format the drive");
        return;
    }
    char fat_name[11];
    format_filename_to_fat(filename, fat_name);

    uint16_t first_cluster = find_free_cluster();
    if (first_cluster == 0xFFFF) kpanic("Disk Full");

    uint32_t bytes_per_cluster = fs_state.sectors_per_cluster * 512;
    uint32_t clusters_needed = (size + bytes_per_cluster - 1) / bytes_per_cluster;

    uint16_t current = first_cluster;
    for (uint32_t i = 0; i < clusters_needed; i++) {
        uint32_t data_lba = fs_state.data_region_lba + (current - 2) * fs_state.sectors_per_cluster;
        for (int s = 0; s < fs_state.sectors_per_cluster; s++) {
            uint32_t offset = (i * bytes_per_cluster) + (s * 512);
            kdisk_write_sector(data_lba + s, (offset < size) ? (data + offset) : NULL);
        }

        if (i < clusters_needed - 1) {
            uint16_t next = find_free_cluster();
            set_fat_entry(current, next);
            set_fat_entry(next, 0xFFFF);
            current = next;
        } else {
            set_fat_entry(current, 0xFFFF);
        }
    }

    uint8_t buf[512];
    for (uint32_t s = 0; s < fs_state.root_dir_sectors; s++) {
        kdisk_read_sector(fs_state.root_dir_lba + s, buf);
        fat16_dir_entry_t* entries = (fat16_dir_entry_t*)buf;
        for (int i = 0; i < 16; i++) {
            if (entries[i].filename[0] == 0x00 || (uint8_t)entries[i].filename[0] == 0xE5) {
                memcpy(entries[i].filename, fat_name, 8);
                memcpy(entries[i].extension, fat_name + 8, 3);
                entries[i].attributes = 0x20; 
                entries[i].cluster_low = first_cluster;
                entries[i].file_size = size;
                kdisk_write_sector(fs_state.root_dir_lba + s, buf);
                return;
            }
        }
    }
}

void fat16_list(fat16_visitor_t visitor) {
    uint8_t buf[512];
    for (uint32_t s = 0; s < fs_state.root_dir_sectors; s++) {
        kdisk_read_sector(fs_state.root_dir_lba + s, buf);
        fat16_dir_entry_t* entries = (fat16_dir_entry_t*)buf;
        for (int i = 0; i < 16; i++) {
            if (entries[i].filename[0] == 0x00) return;
            if ((uint8_t)entries[i].filename[0] == 0xE5 || entries[i].attributes == 0x0F) continue;
            visitor(&entries[i]);
        }
    }
}