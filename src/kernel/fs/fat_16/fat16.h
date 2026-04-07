#ifndef FAT16_H
#define FAT16_H

#include <stdint.h>

typedef struct __attribute__((packed)){
    uint8_t  jmp[3];              // Jump instruction to skip over this data
    uint8_t  oem_name[8];         // OEM Name
    uint16_t bytes_per_sector;    // Usually 512
    uint8_t  sectors_per_cluster; // Usually 1, 2, 4, 8, etc.
    uint16_t reserved_sectors;    // Sectors before the FAT starts (Usually 1)
    uint8_t  fat_count;           // Number of FATs (Usually 2)
    uint16_t root_dir_entries;    // Max files in the root folder (Usually 512)
    uint16_t total_sectors_16;    // Used if volume is smaller than 32MB
    uint8_t  media_descriptor;    // Media type (0xF8 for hard disk)
    uint16_t sectors_per_fat;     // How many sectors ONE FAT table takes up
    uint16_t sectors_per_track;   // Legacy CHS geometry
    uint16_t heads;               // Legacy CHS geometry
    uint32_t hidden_sectors;      // Sectors before the partition starts
    uint32_t total_sectors_32;    // Used if volume is larger than 32MB
    
    uint8_t  drive_number;        // Usually 0x80 for hard drives
    uint8_t  reserved;            // Reserved
    uint8_t  boot_signature;      // Extended boot signature (0x29)
    uint32_t volume_id;           // Serial number
    uint8_t  volume_label[11];    // Volume label
    uint8_t  fs_type[8];          // File system string ("FAT16")
    
    uint8_t  boot_code[448];      // The actual assembly bootloader goes here
    uint16_t magic;               // 0xAA55
} fat16_bpb_t;

typedef struct __attribute__((packed)) {
    char     filename[8];
    char     extension[3];
    uint8_t  attributes;
    uint8_t  reserved;
    uint8_t  creation_time_ms;
    uint16_t creation_time;
    uint16_t creation_date;
    uint16_t last_access_date;
    uint16_t cluster_high;    // Always 0 in FAT16
    uint16_t modify_time;
    uint16_t modify_date;
    uint16_t cluster_low;     // The starting cluster
    uint32_t file_size;
} fat16_dir_entry_t;

typedef void (*fat16_visitor_t)(fat16_dir_entry_t* entry);  

void fat16_initialize(void);
void fat16_format_drive(void);
void fat16_write_file(const char* filename, uint8_t* data, uint32_t size);
void fat16_list(fat16_visitor_t visitor);

#endif