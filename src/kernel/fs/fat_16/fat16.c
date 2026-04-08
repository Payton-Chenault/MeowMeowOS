#include "fat16.h"
#include "../../utils/logging/logger.h"
#include "../../lib/string/string.h"
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

    uint16_t current_dir_cluster; 
    char current_path[256];
} fs_state_t;

static fs_state_t fs_state;

static inline void check_if_mounted() {
    if(!fs_state.is_mounted) {
        kprintf("Error: filesystem not mounted\n");
    }
}

static void format_filename_to_fat(const char* input, char* output) {
    if (strcmp(input, ".") == 0) {
        memcpy(output, ".          ", 11);
        return;
    }
    if (strcmp(input, "..") == 0) {
        memcpy(output, "..         ", 11);
        return;
    }

    memset(output, ' ', 11);
    int i = 0, j = 0;

    while (input[i] && input[i] != '.' && j < 8) {
        char c = input[i++];
        if (c >= 'a' && c <= 'z') c -= 32;
        output[j++] = c;
    }

    while (input[i] && input[i] != '.') i++;
    if (input[i] == '.') i++;

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

static bool find_entry_in_current_dir(const char* fat_name, fat16_dir_entry_t* out_entry) {
    uint8_t buf[512];
    if (fs_state.current_dir_cluster == 0) {
        for (uint32_t s = 0; s < fs_state.root_dir_sectors; s++) {
            kdisk_read_sector(fs_state.root_dir_lba + s, buf);
            fat16_dir_entry_t* entries = (fat16_dir_entry_t*)buf;
            for (int i = 0; i < 16; i++) {
                if (entries[i].filename[0] == 0x00) return false;
                if ((uint8_t)entries[i].filename[0] == 0xE5) continue;
                if (memcmp(entries[i].filename, fat_name, 11) == 0) {
                    if (out_entry) *out_entry = entries[i];
                    return true;
                }
            }
        }
    } else {
        uint16_t cluster = fs_state.current_dir_cluster;
        while (cluster >= 2 && cluster < 0xFFF8) {
            uint32_t lba = fs_state.data_region_lba + (cluster - 2) * fs_state.sectors_per_cluster;
            for (uint32_t s = 0; s < fs_state.sectors_per_cluster; s++) {
                kdisk_read_sector(lba + s, buf);
                fat16_dir_entry_t* entries = (fat16_dir_entry_t*)buf;
                for (int i = 0; i < 16; i++) {
                    if (entries[i].filename[0] == 0x00) return false;
                    if ((uint8_t)entries[i].filename[0] == 0xE5) continue;
                    if (memcmp(entries[i].filename, fat_name, 11) == 0) {
                        if (out_entry) *out_entry = entries[i];
                        return true;
                    }
                }
            }
            cluster = get_fat_entry(cluster);
        }
    }
    return false;
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
    fs_state.current_dir_cluster = 0;

    fs_state.is_mounted = true;
    strcpy(fs_state.current_path, "/");
    log_info(MODULE, "Mounted at LBA %d", fs_state.root_dir_lba);
}

void fat16_format_drive(fat16_progress_callback_t callback) {
    uint8_t boot_buf[512] = {0};
    uint32_t total_sectors = ata_get_total_sectors();
    kdisk_read_sector(0, boot_buf);
    
    fat16_bpb_t* bpb = (fat16_bpb_t*)boot_buf;
    
    bpb->boot_jmp[0] = 0xEB;
    bpb->boot_jmp[1] = 0x3C;
    bpb->boot_jmp[2] = 0x90;

    memcpy(bpb->oem_name, "MEOWMEOW", 8);
    bpb->bytes_per_sector = 512;
    bpb->sectors_per_cluster = 32;
    bpb->reserved_sectors = 256;
    bpb->fat_count = 2;
    bpb->root_dir_entries = 512;
    
    if (total_sectors < 65536) {
        bpb->total_sectors_short = (uint16_t)total_sectors;
        bpb->total_sectors_long = 0;
    } else {
        bpb->total_sectors_short = 0;
        bpb->total_sectors_long = total_sectors;
    }
    
    bpb->media_descriptor = 0xF8;
    
    uint32_t total_clusters = total_sectors / bpb->sectors_per_cluster;
    bpb->sectors_per_fat = (total_clusters * 2 / 512) + 1;
    
    bpb->sectors_per_track = 63;
    bpb->head_count = 255;
    bpb->hidden_sectors = 0;
    

    bpb->drive_number = 0x80;
    bpb->current_head = 0x00;
    bpb->boot_signature = 0x29;
    bpb->volume_id = 0x12345678;
    memcpy(bpb->volume_label, "MEOWMEOW   ", 11);
    memcpy(bpb->fs_type, "FAT16   ", 8);

    boot_buf[510] = 0x55;
    boot_buf[511] = 0xAA;
    
    kdisk_write_sector(0, boot_buf);

    uint32_t root_sectors = (bpb->root_dir_entries * 32) / 512;
    uint32_t total_work = (bpb->sectors_per_fat * 2) + root_sectors;
    uint32_t progress = 0;

    uint8_t fat_buf[512] = {0};
    fat_buf[0] = 0xF8; fat_buf[1] = 0xFF; fat_buf[2] = 0xFF; fat_buf[3] = 0xFF;

    for (uint32_t i = 0; i < bpb->sectors_per_fat; i++) {
        kdisk_write_sector(bpb->reserved_sectors + i, fat_buf);
        kdisk_write_sector(bpb->reserved_sectors + bpb->sectors_per_fat + i, fat_buf);
        if (i == 0) memset(fat_buf, 0, 512); 
        progress += 2;
        if (callback) callback(progress, total_work);
    }

    uint32_t root_lba = bpb->reserved_sectors + (2 * bpb->sectors_per_fat);
    memset(fat_buf, 0, 512);
    for (uint32_t i = 0; i < root_sectors; i++) {
        kdisk_write_sector(root_lba + i, fat_buf);
        progress++;
        if (callback) callback(progress, total_work);
    }
    fat16_initialize();
}

uint32_t fat16_get_file_size(const char* filename) {
    check_if_mounted();
    char fat_name[11];
    format_filename_to_fat(filename, fat_name);
    fat16_dir_entry_t entry;
    if (find_entry_in_current_dir(fat_name, &entry)) return entry.file_size;
    return 0;
}

void fat16_chdir(const char* path) {
    check_if_mounted();

    // 1. Create a local copy so we don't destroy the original argv string
    char path_copy[256];
    strcpy(path_copy, path);

    // 2. Handle Absolute Paths (starting with /)
    if (path[0] == '/') {
        fs_state.current_dir_cluster = 0;
        strcpy(fs_state.current_path, "/");
    }

    // 3. Start Tokenizing the path (splitting by /)
    char* token = strtok(path_copy, "/");

    while (token != NULL) {
        // CASE 1: Move Up (..)
        if (strcmp(token, "..") == 0) {
            if (fs_state.current_dir_cluster != 0) {
                fat16_dir_entry_t entry;
                char dotdot[11];
                format_filename_to_fat("..", dotdot);
                
                if (find_entry_in_current_dir(dotdot, &entry)) {
                    // Update the cluster to the parent
                    fs_state.current_dir_cluster = entry.cluster_low;

                    // --- PATH SNIPPING LOGIC ---
                    int len = strlen(fs_state.current_path);
                    if (len > 1) { // Only snip if we aren't already at "/"
                        // If path ends in a slash (e.g. "/APPS/"), remove it first
                        if (fs_state.current_path[len - 1] == '/') {
                            fs_state.current_path[len - 1] = '\0';
                        }
                        
                        // Find the last slash (e.g. the one in "/APPS/GAMES")
                        char* last_slash = strrchr(fs_state.current_path, '/');
                        if (last_slash != NULL) {
                            // If it's the root slash, keep it: "/"
                            if (last_slash == fs_state.current_path) {
                                *(last_slash + 1) = '\0';
                            } else {
                                // Otherwise, terminate the string at the slash
                                *last_slash = '\0';
                            }
                        }
                    }
                }
            }
        }
        // CASE 2: Current Directory (.)
        else if (strcmp(token, ".") == 0) {
            // Do nothing, just skip to the next token
        }
        // CASE 3: Moving Forward (Subdirectory name)
        else {
            char fat_name[11];
            format_filename_to_fat(token, fat_name);
            fat16_dir_entry_t entry;

            if (find_entry_in_current_dir(fat_name, &entry)) {
                if (entry.attributes & 0x10) {
                    // Update the cluster to the new folder
                    fs_state.current_dir_cluster = entry.cluster_low;

                    // --- PATH APPEND LOGIC ---
                    int len = strlen(fs_state.current_path);
                    
                    // Add a separator slash only if we're not at the root "/"
                    if (fs_state.current_path[len - 1] != '/') {
                        strcat(fs_state.current_path, "/");
                    }
                    
                    // Add the new folder name to the path string
                    strcat(fs_state.current_path, token);
                } else {
                    kprintf("Not a directory: %s\n", token);
                    return; // Stop processing if path is invalid
                }
            } else {
                kprintf("Directory not found: %s\n", token);
                return; // Stop processing if folder doesn't exist
            }
        }

        // Move to the next token in the path
        token = strtok(NULL, "/");
    }
}

const char* fat16_get_current_path() {
    return fs_state.current_path;
}

void fat16_list(fat16_visitor_t visitor) {
    check_if_mounted();
    uint8_t buf[512];
    if (fs_state.current_dir_cluster == 0) {
        for (uint32_t s = 0; s < fs_state.root_dir_sectors; s++) {
            kdisk_read_sector(fs_state.root_dir_lba + s, buf);
            fat16_dir_entry_t* e = (fat16_dir_entry_t*)buf;
            for (int i = 0; i < 16; i++) {
                if (e[i].filename[0] == 0x00) return;
                if ((uint8_t)e[i].filename[0] == 0xE5 || e[i].attributes == 0x0F) continue;
                visitor(&e[i]);
            }
        }
    } else {
        uint16_t cluster = fs_state.current_dir_cluster;
        while (cluster >= 2 && cluster < 0xFFF8) {
            uint32_t lba = fs_state.data_region_lba + (cluster - 2) * fs_state.sectors_per_cluster;
            for (uint32_t s = 0; s < fs_state.sectors_per_cluster; s++) {
                kdisk_read_sector(lba + s, buf);
                fat16_dir_entry_t* e = (fat16_dir_entry_t*)buf;
                for (int i = 0; i < 16; i++) {
                    if (e[i].filename[0] == 0x00) return;
                    if ((uint8_t)e[i].filename[0] == 0xE5 || e[i].attributes == 0x0F) continue;
                    visitor(&e[i]);
                }
            }
            cluster = get_fat_entry(cluster);
        }
    }
}

uint32_t fat16_read_file(const char* filename, uint8_t* buffer) {
    check_if_mounted();
    char fat_name[11];
    format_filename_to_fat(filename, fat_name);
    fat16_dir_entry_t entry;
    if (!find_entry_in_current_dir(fat_name, &entry)) return 0;

    uint16_t cluster = entry.cluster_low;
    uint32_t bytes_read = 0;
    while (cluster >= 2 && cluster < 0xFFF8 && bytes_read < entry.file_size) {
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
    check_if_mounted();
    char fat_name[11];
    format_filename_to_fat(filename, fat_name);
    uint16_t first_cluster = find_free_cluster();
    if (first_cluster == 0xFFFF) kpanic("Disk Full");

    uint32_t bpc = fs_state.sectors_per_cluster * 512;
    uint32_t clusters_needed = (size + bpc - 1) / bpc;
    uint16_t current = first_cluster;
    set_fat_entry(current, 0xFFFF);
    for (uint32_t i = 0; i < clusters_needed; i++) {
        uint32_t lba = fs_state.data_region_lba + (current - 2) * fs_state.sectors_per_cluster;
        for (int s = 0; s < fs_state.sectors_per_cluster; s++) {
            uint32_t offset = (i * bpc) + (s * 512);
            kdisk_write_sector(lba + s, (offset < size) ? (data + offset) : NULL);
        }
        if (i < clusters_needed - 1) {
            uint16_t next = find_free_cluster();
            set_fat_entry(current, next);
            set_fat_entry(next, 0xFFFF);
            current = next;
        }
    }

    uint8_t buf[512];
    uint32_t slba = (fs_state.current_dir_cluster == 0) ? fs_state.root_dir_lba : fs_state.data_region_lba + (fs_state.current_dir_cluster - 2) * fs_state.sectors_per_cluster;
    uint32_t nsec = (fs_state.current_dir_cluster == 0) ? fs_state.root_dir_sectors : fs_state.sectors_per_cluster;

    for (uint32_t s = 0; s < nsec; s++) {
        kdisk_read_sector(slba + s, buf);
        fat16_dir_entry_t* e = (fat16_dir_entry_t*)buf;
        for (int i = 0; i < 16; i++) {
            if (e[i].filename[0] == 0x00 || (uint8_t)e[i].filename[0] == 0xE5) {
                memcpy(e[i].filename, fat_name, 11);
                e[i].attributes = 0x20;
                e[i].cluster_low = first_cluster;
                e[i].file_size = size;
                kdisk_write_sector(slba + s, buf);
                return;
            }
        }
    }
}

void fat16_delete_file(const char* filename) {
    check_if_mounted();
    char fat_name[11];
    format_filename_to_fat(filename, fat_name);
    uint8_t buf[512];
    uint16_t start_cluster = 0;
    uint32_t slba = (fs_state.current_dir_cluster == 0) ? fs_state.root_dir_lba : fs_state.data_region_lba + (fs_state.current_dir_cluster - 2) * fs_state.sectors_per_cluster;
    uint32_t nsec = (fs_state.current_dir_cluster == 0) ? fs_state.root_dir_sectors : fs_state.sectors_per_cluster;

    for (uint32_t s = 0; s < nsec; s++) {
        kdisk_read_sector(slba + s, buf);
        fat16_dir_entry_t* e = (fat16_dir_entry_t*)buf;
        for (int i = 0; i < 16; i++) {
            if ((uint8_t)e[i].filename[0] != 0xE5 && memcmp(e[i].filename, fat_name, 11) == 0) {
                start_cluster = e[i].cluster_low;
                e[i].filename[0] = 0xE5;
                kdisk_write_sector(slba + s, buf);
                uint16_t curr = start_cluster;
                while (curr >= 2 && curr < 0xFFF8) {
                    uint16_t next = get_fat_entry(curr);
                    set_fat_entry(curr, 0x0000);
                    curr = next;
                }
                return;
            }
        }
    }
}

void fat16_create_dir(const char* dirname) {
    check_if_mounted();
    char fat_name[11];
    format_filename_to_fat(dirname, fat_name);
    uint16_t dir_cluster = find_free_cluster();
    set_fat_entry(dir_cluster, 0xFFFF);

    uint8_t cbuf[512] = {0};
    fat16_dir_entry_t* pe = (fat16_dir_entry_t*)cbuf;
    memcpy(pe[0].filename, ".          ", 11);
    pe[0].attributes = 0x10; pe[0].cluster_low = dir_cluster;
    memcpy(pe[1].filename, "..         ", 11);
    pe[1].attributes = 0x10; pe[1].cluster_low = fs_state.current_dir_cluster;
    kdisk_write_sector(fs_state.data_region_lba + (dir_cluster - 2) * fs_state.sectors_per_cluster, cbuf);

    uint8_t buf[512];
    uint32_t slba = (fs_state.current_dir_cluster == 0) ? fs_state.root_dir_lba : fs_state.data_region_lba + (fs_state.current_dir_cluster - 2) * fs_state.sectors_per_cluster;
    uint32_t nsec = (fs_state.current_dir_cluster == 0) ? fs_state.root_dir_sectors : fs_state.sectors_per_cluster;
    for (uint32_t s = 0; s < nsec; s++) {
        kdisk_read_sector(slba + s, buf);
        fat16_dir_entry_t* e = (fat16_dir_entry_t*)buf;
        for (int i = 0; i < 16; i++) {
            if (e[i].filename[0] == 0x00 || (uint8_t)e[i].filename[0] == 0xE5) {
                memcpy(e[i].filename, fat_name, 11);
                e[i].attributes = 0x10; e[i].cluster_low = dir_cluster;
                kdisk_write_sector(slba + s, buf);
                return;
            }
        }
    }
}

static bool is_dir_empty(uint16_t cluster) {
    uint8_t buf[512];
    kdisk_read_sector(fs_state.data_region_lba + (cluster - 2) * fs_state.sectors_per_cluster, buf);
    fat16_dir_entry_t* e = (fat16_dir_entry_t*)buf;
    for (int i = 0; i < 16; i++) {
        if (e[i].filename[0] == 0x00) return true;
        if ((uint8_t)e[i].filename[0] == 0xE5) continue;
        if (e[i].filename[0] == '.' && (e[i].filename[1] == ' ' || e[i].filename[1] == '.')) continue;
        return false;
    }
    return true;
}

void fat16_delete_dir(const char* dirname) {
    check_if_mounted();
    char fat_name[11];
    format_filename_to_fat(dirname, fat_name);
    uint8_t buf[512];
    uint32_t slba = (fs_state.current_dir_cluster == 0) ? fs_state.root_dir_lba : fs_state.data_region_lba + (fs_state.current_dir_cluster - 2) * fs_state.sectors_per_cluster;
    uint32_t nsec = (fs_state.current_dir_cluster == 0) ? fs_state.root_dir_sectors : fs_state.sectors_per_cluster;
    for (uint32_t s = 0; s < nsec; s++) {
        kdisk_read_sector(slba + s, buf);
        fat16_dir_entry_t* e = (fat16_dir_entry_t*)buf;
        for (int i = 0; i < 16; i++) {
            if ((uint8_t)e[i].filename[0] != 0xE5 && memcmp(e[i].filename, fat_name, 11) == 0 && (e[i].attributes & 0x10)) {
                if (!is_dir_empty(e[i].cluster_low)) { kprintf("Dir not empty\n"); return; }
                uint16_t c = e[i].cluster_low;
                e[i].filename[0] = 0xE5;
                kdisk_write_sector(slba + s, buf);
                while (c >= 2 && c < 0xFFF8) { uint16_t n = get_fat_entry(c); set_fat_entry(c, 0x0000); c = n; }
                return;
            }
        }
    }
}