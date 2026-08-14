#ifndef FAT16_H
#define FAT16_H

#include <stdbool.h>
#include <stdint.h>


/* Directory Entry Structure (Packed to match disk layout) */
typedef struct {
  char filename[8];
  char extension[3];
  uint8_t attributes;
  uint8_t reserved;
  uint8_t creation_time_ms;
  uint16_t creation_time;
  uint16_t creation_date;
  uint16_t last_access_date;
  uint16_t cluster_high;
  uint16_t last_mod_time;
  uint16_t last_mod_date;
  uint16_t cluster_low;
  uint32_t file_size;
} __attribute__((packed)) fat16_dir_entry_t;

/* BIOS Parameter Block (Sector 0) */
typedef struct {
  uint8_t boot_jmp[3];
  char oem_name[8];
  uint16_t bytes_per_sector;
  uint8_t sectors_per_cluster;
  uint16_t reserved_sectors;
  uint8_t fat_count;
  uint16_t root_dir_entries;
  uint16_t total_sectors_short;
  uint8_t media_descriptor;
  uint16_t sectors_per_fat;
  uint16_t sectors_per_track;
  uint16_t head_count;
  uint32_t hidden_sectors;
  uint32_t total_sectors_long;
  uint8_t drive_number;
  uint8_t current_head;
  uint8_t boot_signature;
  uint32_t volume_id;
  char volume_label[11];
  char fs_type[8];
} __attribute__((packed)) fat16_bpb_t;

typedef void (*fat16_visitor_t)(fat16_dir_entry_t *entry);
typedef void (*fat16_progress_callback_t)(uint32_t current, uint32_t total);

void fat16_initialize(void);
void fat16_format_drive(uint8_t drive_id, uint32_t max_sectors,
                        fat16_progress_callback_t callback);
uint32_t fat16_get_file_size(const char *filename);
uint32_t fat16_read_file(const char *filename, uint32_t offset, uint32_t size,
                         uint8_t *buffer);
void fat16_write_file(const char *filename, uint8_t *data, uint32_t size);
void fat16_create_dir(const char *dirname);
void fat16_list(fat16_visitor_t visitor);
void fat16_delete_file(const char *filename);
void fat16_delete_dir(const char *dirname);
void fat16_chdir(const char *path);
const char *fat16_get_current_path();

#endif