#include "fat16.h"
#include "../../drivers/disk/ata.h"
#include "../../kernel_services/kernel_services.h"
#include "../../lib/string/string.h"
#include "../../utils/logging/logger.h"

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

static inline bool check_if_mounted() {
  if (!fs_state.is_mounted) {
    log_error(MODULE, "Filesystem not mounted!");
    return false;
  }
  return true;
}

static int fat16_strcasecmp(const char *s1, const char *s2) {
  while (*s1 && *s2) {
    char c1 = (*s1 >= 'A' && *s1 <= 'Z') ? *s1 + 32 : *s1;
    char c2 = (*s2 >= 'A' && *s2 <= 'Z') ? *s2 + 32 : *s2;
    if (c1 != c2) return c1 - c2;
    s1++; s2++;
  }
  return *s1 - *s2;
}

static void format_filename_to_fat(const char *input, char *output) {
  if (input == NULL) {
    memset(output, ' ', 11);
    return;
  }
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

// VFAT Checksum: Validates if an LFN belongs to the short entry immediately following it
static uint8_t lfn_checksum(const uint8_t *short_name) {
  uint8_t sum = 0;
  for (int i = 11; i != 0; i--) {
    sum = ((sum & 1) ? 0x80 : 0) + (sum >> 1) + *short_name++;
  }
  return sum;
}

static void generate_short_name(const char *long_name, char *short_name) {
  memset(short_name, ' ', 11);
  int i = 0, j = 0;
  while (long_name[i] == '.' || long_name[i] == ' ') i++;

  while (long_name[i] && long_name[i] != '.' && j < 6) {
    char c = long_name[i++];
    if (c >= 'a' && c <= 'z') c -= 32;
    if (c != ' ' && c != '+' && c != ',' && c != ';' && c != '=' && c != '[' && c != ']') {
      short_name[j++] = c;
    }
  }
  
  int hash = 0;
  for(int x=0; long_name[x]; x++) hash += long_name[x];
  short_name[j++] = '~';
  short_name[j++] = '1' + (hash % 9);

  const char *ext = strrchr(long_name, '.');
  if (ext && ext != long_name) {
    ext++;
    int k = 8;
    while (*ext && k < 11) {
      char c = *ext++;
      if (c >= 'a' && c <= 'z') c -= 32;
      short_name[k++] = c;
    }
  }
}

static void extract_lfn_chars(fat16_lfn_entry_t *lfn, char *lfn_buf) {
  if ((lfn->order & 0x1F) == 0) return;
  int idx = ((lfn->order & 0x1F) - 1) * 13;

  for (int i = 0; i < 5; i++) {
    uint16_t c = lfn->name1[i];
    if (c == 0x0000 || c == 0xFFFF) { lfn_buf[idx] = '\0'; return; }
    lfn_buf[idx++] = (char)(c & 0xFF);
  }
  for (int i = 0; i < 6; i++) {
    uint16_t c = lfn->name2[i];
    if (c == 0x0000 || c == 0xFFFF) { lfn_buf[idx] = '\0'; return; }
    lfn_buf[idx++] = (char)(c & 0xFF);
  }
  for (int i = 0; i < 2; i++) {
    uint16_t c = lfn->name3[i];
    if (c == 0x0000 || c == 0xFFFF) { lfn_buf[idx] = '\0'; return; }
    lfn_buf[idx++] = (char)(c & 0xFF);
  }
  lfn_buf[idx] = '\0';
}

static void populate_lfn_entry(fat16_lfn_entry_t *entry, const char *long_name, int order, uint8_t checksum, bool is_last) {
  memset(entry, 0xFF, sizeof(fat16_lfn_entry_t));
  entry->order = order | (is_last ? 0x40 : 0x00);
  entry->attributes = 0x0F;
  entry->type = 0;
  entry->checksum = checksum;
  entry->cluster_low = 0;

  int offset = (order - 1) * 13;
  int len = strlen(long_name);
  
  for (int i=0; i<5; i++) entry->name1[i] = (offset+i < len) ? long_name[offset+i] : ((offset+i == len) ? 0x0000 : 0xFFFF);
  for (int i=0; i<6; i++) entry->name2[i] = (offset+5+i < len) ? long_name[offset+5+i] : ((offset+5+i == len) ? 0x0000 : 0xFFFF);
  for (int i=0; i<2; i++) entry->name3[i] = (offset+11+i < len) ? long_name[offset+11+i] : ((offset+11+i == len) ? 0x0000 : 0xFFFF);
}

// Bulk deletion helper: clears an entire LFN sequence and its short entry
static void mark_entries_deleted(uint32_t base_lba, int start_s, int start_i, int end_s, int end_i) {
    uint32_t s = start_s;
    int i = start_i;
    while (s <= end_s) {
        uint8_t buf[512];
        kdisk_read_sector(base_lba + s, buf);
        fat16_dir_entry_t *e = (fat16_dir_entry_t *)buf;
        while (i < 16) {
            e[i].filename[0] = 0xE5;
            if (s == end_s && i == end_i) break;
            i++;
        }
        kdisk_write_sector(base_lba + s, buf);
        if (s == end_s && i == end_i) break;
        s++;
        i = 0;
    }
}

static uint16_t get_fat_entry(uint16_t cluster) {
  uint8_t buf[512];
  uint32_t lba = fs_state.reserved_sectors + (cluster * 2 / 512);
  uint32_t offset = (cluster * 2 % 512);
  kdisk_read_sector(lba, buf);
  return *(uint16_t *)&buf[offset];
}

static void set_fat_entry(uint16_t cluster, uint16_t value) {
  uint8_t buf[512];
  uint32_t lba_off = (cluster * 2 / 512);
  uint32_t offset = (cluster * 2 % 512);

  kdisk_read_sector(fs_state.reserved_sectors + lba_off, buf);
  *(uint16_t *)&buf[offset] = value;

  kdisk_write_sector(fs_state.reserved_sectors + lba_off, buf);
  kdisk_write_sector(fs_state.reserved_sectors + fs_state.sectors_per_fat + lba_off, buf);
}

static uint16_t find_free_cluster(void) {
  uint8_t buf[512];
  for (uint16_t s = 0; s < fs_state.sectors_per_fat; s++) {
    kdisk_read_sector(fs_state.reserved_sectors + s, buf);
    uint16_t *entries = (uint16_t *)buf;
    for (int i = 0; i < 256; i++) {
      if (s == 0 && i < 2) continue;
      if (entries[i] == 0x0000) return (s * 256) + i;
    }
  }
  return 0xFFFF;
}

static bool find_entry_in_current_dir(const char *target_name, fat16_dir_entry_t *out_entry) {
  uint8_t buf[512];
  char lfn_buf[256];
  memset(lfn_buf, 0, 256);
  uint8_t lfn_chksum = 0;

  if (fs_state.current_dir_cluster == 0) {
    for (uint32_t s = 0; s < fs_state.root_dir_sectors; s++) {
      kdisk_read_sector(fs_state.root_dir_lba + s, buf);
      fat16_dir_entry_t *entries = (fat16_dir_entry_t *)buf;
      for (int i = 0; i < 16; i++) {
        fat16_dir_entry_t *e = &entries[i];

        if (e->filename[0] == 0x00) return false;
        if ((uint8_t)e->filename[0] == 0xE5) { memset(lfn_buf, 0, 256); continue; }
        if (e->attributes == 0x0F) { 
            extract_lfn_chars((fat16_lfn_entry_t *)e, lfn_buf); 
            lfn_chksum = ((fat16_lfn_entry_t *)e)->checksum;
            continue; 
        }

        // Invalidates orphaned ghost sequences
        if (lfn_buf[0] != '\0' && lfn_chksum != lfn_checksum((uint8_t*)e->filename)) {
            memset(lfn_buf, 0, 256);
        }

        bool matched = false;
        if (lfn_buf[0] != '\0') {
          if (fat16_strcasecmp(lfn_buf, target_name) == 0) matched = true;
        } else {
          char fat_target[11];
          format_filename_to_fat(target_name, fat_target);
          if (memcmp(e->filename, fat_target, 11) == 0) matched = true;
        }

        if (matched) {
          if (out_entry) *out_entry = *e;
          return true;
        }
        memset(lfn_buf, 0, 256);
      }
    }
  } else {
    uint16_t cluster = fs_state.current_dir_cluster;
    while (cluster >= 2 && cluster < 0xFFF8) {
      uint32_t lba = fs_state.data_region_lba + (cluster - 2) * fs_state.sectors_per_cluster;
      for (uint32_t s = 0; s < fs_state.sectors_per_cluster; s++) {
        kdisk_read_sector(lba + s, buf);
        fat16_dir_entry_t *entries = (fat16_dir_entry_t *)buf;
        for (int i = 0; i < 16; i++) {
          fat16_dir_entry_t *e = &entries[i];

          if (e->filename[0] == 0x00) return false;
          if ((uint8_t)e->filename[0] == 0xE5) { memset(lfn_buf, 0, 256); continue; }
          if (e->attributes == 0x0F) { 
              extract_lfn_chars((fat16_lfn_entry_t *)e, lfn_buf); 
              lfn_chksum = ((fat16_lfn_entry_t *)e)->checksum;
              continue; 
          }

          if (lfn_buf[0] != '\0' && lfn_chksum != lfn_checksum((uint8_t*)e->filename)) {
              memset(lfn_buf, 0, 256);
          }

          bool matched = false;
          if (lfn_buf[0] != '\0') {
            if (fat16_strcasecmp(lfn_buf, target_name) == 0) matched = true;
          } else {
            char fat_target[11];
            format_filename_to_fat(target_name, fat_target);
            if (memcmp(e->filename, fat_target, 11) == 0) matched = true;
          }

          if (matched) {
            if (out_entry) *out_entry = *e;
            return true;
          }
          memset(lfn_buf, 0, 256);
        }
      }
      cluster = get_fat_entry(cluster);
    }
  }
  return false;
}

static int add_dir_entry(const char *base_name, uint8_t attributes, uint16_t cluster, uint32_t size) {
  char short_name[11];
  int len = strlen(base_name);
  
  bool need_lfn = false;
  if (len > 12) need_lfn = true;
  for (int i = 0; i < len; i++) {
    if (base_name[i] >= 'a' && base_name[i] <= 'z') need_lfn = true;
    if (base_name[i] == ' ' && i != len - 1) need_lfn = true;
  }
  
  if (need_lfn) {
    generate_short_name(base_name, short_name);
  } else {
    format_filename_to_fat(base_name, short_name);
  }

  int lfn_count = need_lfn ? ((len + 12) / 13) : 0;
  int required_entries = lfn_count + 1;
  uint8_t chksum = lfn_checksum((uint8_t*)short_name);

  uint32_t slba = (fs_state.current_dir_cluster == 0)
        ? fs_state.root_dir_lba
        : fs_state.data_region_lba + (fs_state.current_dir_cluster - 2) * fs_state.sectors_per_cluster;
  uint32_t nsec = (fs_state.current_dir_cluster == 0)
                    ? fs_state.root_dir_sectors
                    : fs_state.sectors_per_cluster;

  int free_count = 0;
  uint32_t start_s = 0;
  int start_i = 0;
  bool found = false;
  
  uint8_t buf[512];

  for (uint32_t s = 0; s < nsec; s++) {
    kdisk_read_sector(slba + s, buf);
    fat16_dir_entry_t *e = (fat16_dir_entry_t *)buf;
    for (int i = 0; i < 16; i++) {
      if (e[i].filename[0] == 0x00 || (uint8_t)e[i].filename[0] == 0xE5) {
        if (free_count == 0) { start_s = s; start_i = i; }
        free_count++;
        if (free_count == required_entries) { found = true; break; }
      } else {
        free_count = 0;
      }
    }
    if (found) break;
  }

  if (!found) return -1;

  uint32_t write_s = start_s;
  int write_i = start_i;
  kdisk_read_sector(slba + write_s, buf);
  fat16_dir_entry_t *we = (fat16_dir_entry_t *)buf;

  for (int l = lfn_count; l >= 1; l--) {
    populate_lfn_entry((fat16_lfn_entry_t*)&we[write_i], base_name, l, chksum, l == lfn_count);
    write_i++;
    if (write_i == 16) {
      kdisk_write_sector(slba + write_s, buf);
      write_s++;
      write_i = 0;
      kdisk_read_sector(slba + write_s, buf);
      we = (fat16_dir_entry_t *)buf;
    }
  }

  memcpy(we[write_i].filename, short_name, 11);
  we[write_i].attributes = attributes;
  we[write_i].reserved = 0;
  we[write_i].creation_time_ms = 0;
  we[write_i].creation_time = 0;
  we[write_i].creation_date = 0;
  we[write_i].last_access_date = 0;
  we[write_i].cluster_high = 0;
  we[write_i].last_mod_time = 0;
  we[write_i].last_mod_date = 0;
  we[write_i].cluster_low = cluster;
  we[write_i].file_size = size;

  kdisk_write_sector(slba + write_s, buf);
  return 0;
}

void fat16_initialize(void) {
  uint8_t buf[512];
  kdisk_read_sector(0, buf);
  fat16_bpb_t *bpb = (fat16_bpb_t *)buf;

  if (bpb->bytes_per_sector != 512 || bpb->fat_count != 2) {
    log_warning(MODULE, "Disk is unformatted for this OS or corrupt");
    fat16_format_drive(0x80, 0, NULL, false);
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

static void protect_fat_chain(uint16_t start_cluster, uint8_t *protected_flags, uint16_t *old_fat, uint32_t total_clusters) {
  uint16_t c = start_cluster;
  while (c >= 2 && c < 0xFFF8) {
    if (protected_flags[c]) break;
    protected_flags[c] = 1;
    c = old_fat[c];
  }
}

static void protect_directory_tree(uint16_t dir_cluster, uint8_t *protected_flags, uint16_t *old_fat, uint32_t total_clusters) {
  uint8_t buf[512];
  uint16_t c = dir_cluster;
  while (c >= 2 && c < 0xFFF8) {
    protected_flags[c] = 1;
    uint32_t lba = fs_state.data_region_lba + (c - 2) * fs_state.sectors_per_cluster;
    for (uint32_t s = 0; s < fs_state.sectors_per_cluster; s++) {
      kdisk_read_sector(lba + s, buf);
      fat16_dir_entry_t *entries = (fat16_dir_entry_t *)buf;
      for (int i = 0; i < 16; i++) {
        if (entries[i].filename[0] == 0x00) return;
        if ((uint8_t)entries[i].filename[0] == 0xE5 || entries[i].attributes == 0x0F) continue;
        if (entries[i].filename[0] == '.' && (entries[i].filename[1] == ' ' || entries[i].filename[1] == '.')) continue;

        uint16_t child_start = entries[i].cluster_low;
        if (entries[i].attributes & 0x10) {
          protect_directory_tree(child_start, protected_flags, old_fat, total_clusters);
        } else {
          protect_fat_chain(child_start, protected_flags, old_fat, total_clusters);
        }
      }
    }
    c = old_fat[c];
  }
}

void fat16_format_drive(uint8_t drive_id, uint32_t max_sectors, fat16_progress_callback_t callback, bool preserve_system) {
  uint8_t boot_buf[512] = {0};
  uint8_t fat_buf[512] = {0}; 
  uint32_t total_disk_sectors = ata_get_total_sectors();
  uint32_t total_sectors = (max_sectors == 0 || max_sectors > total_disk_sectors) ? total_disk_sectors : max_sectors;

  kdisk_read_sector(0, boot_buf);
  fat16_bpb_t *bpb = (fat16_bpb_t *)boot_buf;

  uint16_t system_start_cluster = 0;
  bool found_system = false;
  uint8_t *protected_flags = NULL;
  uint16_t *old_fat = NULL;

  if (preserve_system && fs_state.is_mounted) {
    uint16_t saved_cwd_cluster = fs_state.current_dir_cluster;
    char saved_cwd[256];
    strcpy(saved_cwd, fs_state.current_path);
    fs_state.current_dir_cluster = 0;
    strcpy(fs_state.current_path, "/");

    fat16_dir_entry_t sys_entry;
    if (find_entry_in_current_dir("system", &sys_entry) && (sys_entry.attributes & 0x10)) {
      system_start_cluster = sys_entry.cluster_low;
      found_system = true;
    }

    fs_state.current_dir_cluster = saved_cwd_cluster;
    strcpy(fs_state.current_path, saved_cwd);

    if (found_system) {
      uint32_t total_clusters = total_sectors / fs_state.sectors_per_cluster + 2;
      old_fat = (uint16_t *)kmem_zalloc(fs_state.sectors_per_fat * 512);
      for (uint32_t i = 0; i < fs_state.sectors_per_fat; i++) {
        kdisk_read_sector(fs_state.reserved_sectors + i, (uint8_t *)(old_fat + i * 256));
      }
      protected_flags = (uint8_t *)kmem_zalloc(total_clusters);
      protect_directory_tree(system_start_cluster, protected_flags, old_fat, total_clusters);
    }
  }

  bpb->boot_jmp[0] = 0xEB; bpb->boot_jmp[1] = 0x3C; bpb->boot_jmp[2] = 0x90;
  memcpy(bpb->oem_name, "MEOWMEOW", 8);
  bpb->bytes_per_sector = 512;
  bpb->sectors_per_cluster = 32;
  bpb->reserved_sectors = 256;
  bpb->fat_count = 2;
  bpb->root_dir_entries = 512;
  if (total_sectors < 65536) { bpb->total_sectors_short = (uint16_t)total_sectors; bpb->total_sectors_long = 0; } 
  else { bpb->total_sectors_short = 0; bpb->total_sectors_long = total_sectors; }
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

  boot_buf[510] = 0x55; boot_buf[511] = 0xAA;
  kdisk_write_sector(0, boot_buf);

  uint32_t root_sectors = (bpb->root_dir_entries * 32) / 512;
  uint32_t total_work = (bpb->sectors_per_fat * 2) + root_sectors;
  uint32_t progress = 0;
  uint16_t fat_entries_per_sector = 256;

  for (uint32_t s = 0; s < bpb->sectors_per_fat; s++) {
    memset(fat_buf, 0, 512); 
    uint16_t *fat_entries = (uint16_t *)fat_buf;
    for (int i = 0; i < fat_entries_per_sector; i++) {
      uint32_t cluster = s * fat_entries_per_sector + i;
      if (cluster == 0) { fat_entries[i] = 0xFFF8; continue; }
      if (cluster == 1) { fat_entries[i] = 0xFFFF; continue; }
      if (preserve_system && found_system && protected_flags && protected_flags[cluster]) {
        fat_entries[i] = old_fat ? old_fat[cluster] : 0xFFFF;
      } else {
        fat_entries[i] = 0x0000;
      }
    }
    kdisk_write_sector(bpb->reserved_sectors + s, fat_buf);
    kdisk_write_sector(bpb->reserved_sectors + bpb->sectors_per_fat + s, fat_buf);
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

  if (found_system && system_start_cluster >= 2) {
    add_dir_entry("system", 0x10, system_start_cluster, 0);
  }

  if (old_fat) kmem_free(old_fat);
  if (protected_flags) kmem_free(protected_flags);
}

// Fixed: Parses absolute paths perfectly so descriptions can dynamically retrieve metadata size 
uint32_t fat16_get_file_size(const char *filename) {
  if (!check_if_mounted()) return 0;
  
  char old_cwd[256];
  strcpy(old_cwd, fat16_get_current_path());

  char path_copy[256];
  const char *base_name = filename;
  bool has_path = false;

  if (strrchr(filename, '/')) {
    strcpy(path_copy, filename);
    char *slash = strrchr(path_copy, '/');
    *slash = '\0';
    base_name = slash + 1;

    if (path_copy[0] == '\0') { fat16_chdir("/"); } 
    else { fat16_chdir(path_copy); }
    has_path = true;
  }

  fat16_dir_entry_t entry;
  uint32_t size = 0;
  if (find_entry_in_current_dir(base_name, &entry)) {
    size = entry.file_size;
  }
  
  if (has_path) fat16_chdir(old_cwd);
  return size;
}

int fat16_chdir(const char *path) {
  if (!check_if_mounted()) return -1;

  uint16_t orig_cluster = fs_state.current_dir_cluster;
  char orig_path[256];
  strcpy(orig_path, fs_state.current_path);

  char path_copy[256];
  strcpy(path_copy, path);

  if (path_copy[0] == '/') {
    fs_state.current_dir_cluster = 0;
    fs_state.current_path[0] = '/';
    fs_state.current_path[1] = '\0';
  }

  char *token = strtok(path_copy, "/");
  while (token != NULL) {
    if (strcmp(token, ".") == 0) {
      // do nothing
    } else if (strcmp(token, "..") == 0) {
      if (fs_state.current_dir_cluster != 0) {
        fat16_dir_entry_t entry;
        if (find_entry_in_current_dir("..", &entry)) {
          fs_state.current_dir_cluster = entry.cluster_low;
          size_t len = strlen(fs_state.current_path);
          if (len > 1) { 
            char *last_slash = strrchr(fs_state.current_path, '/');
            if (last_slash != NULL) {
              if (last_slash == fs_state.current_path) {
                fs_state.current_path[1] = '\0';
              } else {
                *last_slash = '\0';
              }
            }
          }
        } else {
          fs_state.current_dir_cluster = orig_cluster;
          strcpy(fs_state.current_path, orig_path);
          return -1;
        }
      }
    } else {
      fat16_dir_entry_t entry;
      if (find_entry_in_current_dir(token, &entry)) {
        if (entry.attributes & 0x10) {
          fs_state.current_dir_cluster = entry.cluster_low;
          size_t cur_len = strlen(fs_state.current_path);
          size_t tok_len = strlen(token);
          if (cur_len + tok_len + 2 < sizeof(fs_state.current_path)) { 
            if (cur_len > 1) { 
              fs_state.current_path[cur_len] = '/';
              fs_state.current_path[cur_len + 1] = '\0';
              strcat(fs_state.current_path, token);
            } else {
              strcat(fs_state.current_path, token);
            }
          } else {
            fs_state.current_dir_cluster = orig_cluster;
            strcpy(fs_state.current_path, orig_path);
            return -1;
          }
        } else {
          fs_state.current_dir_cluster = orig_cluster;
          strcpy(fs_state.current_path, orig_path);
          return -1;
        }
      } else {
        fs_state.current_dir_cluster = orig_cluster;
        strcpy(fs_state.current_path, orig_path);
        return -1;
      }
    }
    token = strtok(NULL, "/");
  }
  return 0;
}

const char *fat16_get_current_path() { return fs_state.current_path; }

void fat16_list(fat16_visitor_t visitor) {
  check_if_mounted();
  uint8_t buf[512];
  char lfn_buf[256];
  memset(lfn_buf, 0, 256);
  uint8_t lfn_chksum = 0;

  if (fs_state.current_dir_cluster == 0) {
    for (uint32_t s = 0; s < fs_state.root_dir_sectors; s++) {
      kdisk_read_sector(fs_state.root_dir_lba + s, buf);
      fat16_dir_entry_t *entries = (fat16_dir_entry_t *)buf;
      for (int i = 0; i < 16; i++) {
        fat16_dir_entry_t *e = &entries[i];

        if (e->filename[0] == 0x00) return;
        if ((uint8_t)e->filename[0] == 0xE5) { memset(lfn_buf, 0, 256); continue; }
        if (e->attributes == 0x0F) { 
            extract_lfn_chars((fat16_lfn_entry_t *)e, lfn_buf); 
            lfn_chksum = ((fat16_lfn_entry_t *)e)->checksum;
            continue; 
        }

        if (lfn_buf[0] != '\0' && lfn_chksum != lfn_checksum((uint8_t*)e->filename)) {
            memset(lfn_buf, 0, 256);
        }

        if (lfn_buf[0] != '\0') {
          visitor(e, lfn_buf);
        } else {
          char fallback[13];
          int pos = 0;
          for (int j = 0; j < 8 && e->filename[j] != ' '; j++) fallback[pos++] = e->filename[j];
          if (e->extension[0] != ' ') {
            fallback[pos++] = '.';
            for (int j = 0; j < 3 && e->extension[j] != ' '; j++) fallback[pos++] = e->extension[j];
          }
          fallback[pos] = '\0';
          visitor(e, fallback);
        }
        memset(lfn_buf, 0, 256);
      }
    }
  } else {
    uint16_t cluster = fs_state.current_dir_cluster;
    while (cluster >= 2 && cluster < 0xFFF8) {
      uint32_t lba = fs_state.data_region_lba + (cluster - 2) * fs_state.sectors_per_cluster;
      for (uint32_t s = 0; s < fs_state.sectors_per_cluster; s++) {
        kdisk_read_sector(lba + s, buf);
        fat16_dir_entry_t *entries = (fat16_dir_entry_t *)buf;
        for (int i = 0; i < 16; i++) {
          fat16_dir_entry_t *e = &entries[i];

          if (e->filename[0] == 0x00) return;
          if ((uint8_t)e->filename[0] == 0xE5) { memset(lfn_buf, 0, 256); continue; }
          if (e->attributes == 0x0F) { 
              extract_lfn_chars((fat16_lfn_entry_t *)e, lfn_buf); 
              lfn_chksum = ((fat16_lfn_entry_t *)e)->checksum;
              continue; 
          }

          if (lfn_buf[0] != '\0' && lfn_chksum != lfn_checksum((uint8_t*)e->filename)) {
              memset(lfn_buf, 0, 256);
          }

          if (lfn_buf[0] != '\0') {
            visitor(e, lfn_buf);
          } else {
            char fallback[13];
            int pos = 0;
            for (int j = 0; j < 8 && e->filename[j] != ' '; j++) fallback[pos++] = e->filename[j];
            if (e->extension[0] != ' ') {
              fallback[pos++] = '.';
              for (int j = 0; j < 3 && e->extension[j] != ' '; j++) fallback[pos++] = e->extension[j];
            }
            fallback[pos] = '\0';
            visitor(e, fallback);
          }
          memset(lfn_buf, 0, 256);
        }
      }
      cluster = get_fat_entry(cluster);
    }
  }
}

uint32_t fat16_read_file(const char *filename, uint32_t offset, uint32_t size, uint8_t *buffer) {
  check_if_mounted();
  char old_cwd[256];
  strcpy(old_cwd, fat16_get_current_path());

  char path_copy[256];
  const char *base_name = filename;
  bool has_path = false;

  if (strrchr(filename, '/')) {
    strcpy(path_copy, filename);
    char *slash = strrchr(path_copy, '/');
    *slash = '\0';
    base_name = slash + 1;

    if (path_copy[0] == '\0') { fat16_chdir("/"); } 
    else { fat16_chdir(path_copy); }
    has_path = true;
  }

  fat16_dir_entry_t entry;
  if (!find_entry_in_current_dir(base_name, &entry)) {
    if (has_path) fat16_chdir(old_cwd);
    return 0;
  }

  if (offset >= entry.file_size) {
    if (has_path) fat16_chdir(old_cwd);
    return 0;
  }

  uint32_t bytes_to_read = size;
  if (offset + bytes_to_read > entry.file_size) {
    bytes_to_read = entry.file_size - offset;
  }

  uint32_t cluster_size = fs_state.sectors_per_cluster * 512;
  uint16_t cluster = entry.cluster_low;
  uint32_t clusters_to_skip = offset / cluster_size;

  for (uint32_t i = 0; i < clusters_to_skip; i++) {
    if (cluster < 2 || cluster >= 0xFFF8) {
      if (has_path) fat16_chdir(old_cwd);
      return 0;
    }
    cluster = get_fat_entry(cluster);
  }

  uint32_t bytes_read = 0;
  uint32_t current_offset = offset % cluster_size;

  while (cluster >= 2 && cluster < 0xFFF8 && bytes_read < bytes_to_read) {
    uint32_t lba = fs_state.data_region_lba + (cluster - 2) * fs_state.sectors_per_cluster;
    for (uint32_t s = 0; s < fs_state.sectors_per_cluster && bytes_read < bytes_to_read; s++) {
      if (current_offset >= 512) {
        current_offset -= 512;
        continue;
      }

      uint8_t sector_buf[512];
      kdisk_read_sector(lba + s, sector_buf);

      uint32_t bytes_available = 512 - current_offset;
      uint32_t to_copy = bytes_available;
      if (to_copy > bytes_to_read - bytes_read) to_copy = bytes_to_read - bytes_read;

      memcpy(buffer + bytes_read, sector_buf + current_offset, to_copy);
      bytes_read += to_copy;
      current_offset = 0;
    }
    cluster = get_fat_entry(cluster);
  }

  if (has_path) fat16_chdir(old_cwd);
  return bytes_read;
}

void fat16_write_file(const char *filename, uint8_t *data, uint32_t size) {
  check_if_mounted();
  char old_cwd[256];
  strcpy(old_cwd, fat16_get_current_path());

  char path_copy[256];
  const char *base_name = filename;
  bool has_path = false;

  if (strrchr(filename, '/')) {
    strcpy(path_copy, filename);
    char *slash = strrchr(path_copy, '/');
    *slash = '\0';
    base_name = slash + 1;

    if (path_copy[0] == '\0') { fat16_chdir("/"); } 
    else { fat16_chdir(path_copy); }
    has_path = true;
  }

  uint16_t first_cluster = find_free_cluster();
  if (first_cluster == 0xFFFF) kpanic("Disk Full");

  uint32_t bpc = fs_state.sectors_per_cluster * 512;
  uint32_t clusters_needed = 0;
  if (size > 0) clusters_needed = (size + bpc - 1) / bpc;
  
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

  if (add_dir_entry(base_name, 0x20, first_cluster, size) == -1) {
    log_error(MODULE, "Directory full, failed to write file.");
  }

  if (has_path) fat16_chdir(old_cwd);
}

void fat16_delete_file(const char *filename) {
  check_if_mounted();
  if (filename == NULL || filename[0] == '\0') return;

  char old_cwd[256];
  strcpy(old_cwd, fat16_get_current_path());

  char path_copy[256];
  const char *base_name = filename;
  bool has_path = false;

  if (strrchr(filename, '/')) {
    strcpy(path_copy, filename);
    char *slash = strrchr(path_copy, '/');
    *slash = '\0';
    base_name = slash + 1;

    if (path_copy[0] == '\0') { fat16_chdir("/"); } 
    else { fat16_chdir(path_copy); }
    has_path = true;
  }

  uint8_t buf[512];
  char lfn_buf[256];
  memset(lfn_buf, 0, 256);
  uint8_t lfn_chksum = 0;
  int lfn_start_s = -1, lfn_start_i = -1;

  uint32_t slba = (fs_state.current_dir_cluster == 0)
          ? fs_state.root_dir_lba
          : fs_state.data_region_lba + (fs_state.current_dir_cluster - 2) * fs_state.sectors_per_cluster;
  uint32_t nsec = (fs_state.current_dir_cluster == 0)
                      ? fs_state.root_dir_sectors
                      : fs_state.sectors_per_cluster;

  for (uint32_t s = 0; s < nsec; s++) {
    kdisk_read_sector(slba + s, buf);
    fat16_dir_entry_t *entries = (fat16_dir_entry_t *)buf;

    for (int i = 0; i < 16; i++) {
      fat16_dir_entry_t *e = &entries[i];

      if (e->filename[0] == 0x00) {
        if (has_path) fat16_chdir(old_cwd);
        return;
      }
      if ((uint8_t)e->filename[0] == 0xE5) { 
          memset(lfn_buf, 0, 256); 
          lfn_start_s = -1;
          continue; 
      }
      if (e->attributes == 0x0F) { 
          if (lfn_start_s == -1) { lfn_start_s = s; lfn_start_i = i; }
          extract_lfn_chars((fat16_lfn_entry_t *)e, lfn_buf); 
          lfn_chksum = ((fat16_lfn_entry_t *)e)->checksum;
          continue; 
      }

      if (lfn_buf[0] != '\0' && lfn_chksum != lfn_checksum((uint8_t*)e->filename)) {
          memset(lfn_buf, 0, 256);
          lfn_start_s = -1;
      }

      bool matched = false;
      if (lfn_buf[0] != '\0') {
        if (fat16_strcasecmp(lfn_buf, base_name) == 0 && !(e->attributes & 0x10)) matched = true;
      } else {
        char fat_name[11];
        format_filename_to_fat(base_name, fat_name);
        if (memcmp(e->filename, fat_name, 11) == 0 && !(e->attributes & 0x10)) matched = true;
      }

      if (matched) {
        if (lfn_start_s != -1) {
            mark_entries_deleted(slba, lfn_start_s, lfn_start_i, s, i);
        } else {
            mark_entries_deleted(slba, s, i, s, i);
        }

        uint16_t curr = e->cluster_low;
        while (curr >= 2 && curr < 0xFFF8) {
          uint16_t next = get_fat_entry(curr);
          set_fat_entry(curr, 0x0000);
          curr = next;
        }

        if (has_path) fat16_chdir(old_cwd);
        return;
      }
      memset(lfn_buf, 0, 256);
      lfn_start_s = -1;
    }
  }

  if (has_path) fat16_chdir(old_cwd);
}

int fat16_create_dir(const char *dirname) {
  if (!check_if_mounted()) return -1;
  if (dirname == NULL || dirname[0] == '\0') return -1;

  char saved_path[256];
  strcpy(saved_path, fs_state.current_path);

  char absolute_path[256];
  if (dirname[0] == '/') {
    strcpy(absolute_path, dirname);
  } else if (strcmp(fs_state.current_path, "/") == 0) {
    snprintf(absolute_path, sizeof(absolute_path), "/%s", dirname);
  } else {
    snprintf(absolute_path, sizeof(absolute_path), "%s/%s", fs_state.current_path, dirname);
  }

  char parent_path[256];
  char base_name[64];
  char path_copy[256];
  strcpy(path_copy, absolute_path);

  char *last = strrchr(path_copy, '/');
  if (last == NULL) {
    strcpy(parent_path, "/");
    strcpy(base_name, path_copy);
  } else if (last == path_copy) {
    strcpy(parent_path, "/");
    strcpy(base_name, path_copy + 1);
  } else {
    *last = '\0';
    strcpy(parent_path, path_copy[0] == '\0' ? "/" : path_copy);
    strcpy(base_name, last + 1);
  }

  if (base_name[0] == '\0') { fat16_chdir(saved_path); return -1; }
  if (fat16_chdir(parent_path) != 0) { fat16_chdir(saved_path); return -1; }

  fat16_dir_entry_t entry;
  if (find_entry_in_current_dir(base_name, &entry)) {
    fat16_chdir(saved_path);
    return -1;
  }

  uint16_t dir_cluster = find_free_cluster();
  if (dir_cluster == 0xFFFF) { fat16_chdir(saved_path); return -1; }

  set_fat_entry(dir_cluster, 0xFFFF);

  uint8_t cluster_buf[512];
  memset(cluster_buf, 0, sizeof(cluster_buf));
  fat16_dir_entry_t *dir_entries = (fat16_dir_entry_t *)cluster_buf;

  memcpy(dir_entries[0].filename, ".          ", 11);
  dir_entries[0].attributes = 0x10;
  dir_entries[0].cluster_low = dir_cluster;
  dir_entries[0].file_size = 0;

  memcpy(dir_entries[1].filename, "..         ", 11);
  dir_entries[1].attributes = 0x10;
  dir_entries[1].cluster_low = fs_state.current_dir_cluster;
  dir_entries[1].file_size = 0;

  uint32_t cluster_lba = fs_state.data_region_lba + (dir_cluster - 2) * fs_state.sectors_per_cluster;
  kdisk_write_sector(cluster_lba, cluster_buf);

  if (add_dir_entry(base_name, 0x10, dir_cluster, 0) == -1) {
    fat16_chdir(saved_path);
    return -1;
  }

  fat16_chdir(saved_path);
  return 0;
}

static bool is_dir_empty(uint16_t cluster) {
  uint8_t buf[512];
  kdisk_read_sector(fs_state.data_region_lba + (cluster - 2) * fs_state.sectors_per_cluster, buf);
  fat16_dir_entry_t *e = (fat16_dir_entry_t *)buf;
  for (int i = 0; i < 16; i++) {
    if (e[i].filename[0] == 0x00) return true;
    if ((uint8_t)e[i].filename[0] == 0xE5 || e[i].attributes == 0x0F) continue;
    if (e[i].filename[0] == '.' && (e[i].filename[1] == ' ' || e[i].filename[1] == '.')) continue;
    return false;
  }
  return true;
}

int fat16_copy_file(const char *src, const char *dst) {
  if (!src || !dst) return -1;
  char saved_path[256];
  strcpy(saved_path, fs_state.current_path);

  char src_copy[256];
  char dst_copy[256];
  strcpy(src_copy, src);
  strcpy(dst_copy, dst);

  char *src_name = strrchr(src_copy, '/');
  char *dst_name = strrchr(dst_copy, '/');
  if (!src_name || !dst_name) return -1;

  *src_name = '\0'; src_name++;
  *dst_name = '\0'; dst_name++;

  char src_dir[256];
  char dst_dir[256];
  if (src_copy[0] == '\0') strcpy(src_dir, "/"); else strcpy(src_dir, src_copy);
  if (dst_copy[0] == '\0') strcpy(dst_dir, "/"); else strcpy(dst_dir, dst_copy);

  if (fat16_chdir(src_dir) != 0) { fat16_chdir(saved_path); return -1; }

  uint32_t size = fat16_get_file_size(src_name);
  if (size == 0) { fat16_chdir(saved_path); return -1; }

  uint8_t *buf = (uint8_t *)kmem_alloc(size);
  if (!buf) { fat16_chdir(saved_path); return -1; }

  uint32_t bytes_read = fat16_read_file(src_name, 0, size, buf);
  if (bytes_read != size) { kmem_free(buf); fat16_chdir(saved_path); return -1; }

  if (fat16_chdir(dst_dir) != 0) { kmem_free(buf); fat16_chdir(saved_path); return -1; }

  fat16_write_file(dst_name, buf, size);

  kmem_free(buf);
  fat16_chdir(saved_path);
  return 0;
}

void fat16_delete_dir(const char *dirname) {
  check_if_mounted();
  if (dirname == NULL || dirname[0] == '\0') return;

  char old_cwd[256];
  strcpy(old_cwd, fat16_get_current_path());

  char path_copy[256];
  const char *base_name = dirname;
  bool has_path = false;

  if (strrchr(dirname, '/')) {
    strcpy(path_copy, dirname);
    char *slash = strrchr(path_copy, '/');
    *slash = '\0';
    base_name = slash + 1;

    if (path_copy[0] == '\0') { fat16_chdir("/"); } 
    else { fat16_chdir(path_copy); }
    has_path = true;
  }

  uint8_t buf[512];
  char lfn_buf[256];
  memset(lfn_buf, 0, 256);
  uint8_t lfn_chksum = 0;
  int lfn_start_s = -1, lfn_start_i = -1;

  uint32_t slba = (fs_state.current_dir_cluster == 0)
          ? fs_state.root_dir_lba
          : fs_state.data_region_lba + (fs_state.current_dir_cluster - 2) * fs_state.sectors_per_cluster;
  uint32_t nsec = (fs_state.current_dir_cluster == 0)
                      ? fs_state.root_dir_sectors
                      : fs_state.sectors_per_cluster;

  for (uint32_t s = 0; s < nsec; s++) {
    kdisk_read_sector(slba + s, buf);
    fat16_dir_entry_t *entries = (fat16_dir_entry_t *)buf;

    for (int i = 0; i < 16; i++) {
      fat16_dir_entry_t *e = &entries[i];

      if (e->filename[0] == 0x00) {
        if (has_path) fat16_chdir(old_cwd);
        return;
      }
      if ((uint8_t)e->filename[0] == 0xE5) { 
          memset(lfn_buf, 0, 256); 
          lfn_start_s = -1;
          continue; 
      }
      if (e->attributes == 0x0F) { 
          if (lfn_start_s == -1) { lfn_start_s = s; lfn_start_i = i; }
          extract_lfn_chars((fat16_lfn_entry_t *)e, lfn_buf); 
          lfn_chksum = ((fat16_lfn_entry_t *)e)->checksum;
          continue; 
      }

      if (lfn_buf[0] != '\0' && lfn_chksum != lfn_checksum((uint8_t*)e->filename)) {
          memset(lfn_buf, 0, 256);
          lfn_start_s = -1;
      }

      bool matched = false;
      if (lfn_buf[0] != '\0') {
        if (fat16_strcasecmp(lfn_buf, base_name) == 0 && (e->attributes & 0x10)) matched = true;
      } else {
        char fat_name[11];
        format_filename_to_fat(base_name, fat_name);
        if (memcmp(e->filename, fat_name, 11) == 0 && (e->attributes & 0x10)) matched = true;
      }

      if (matched) {
        if (!is_dir_empty(e->cluster_low)) {
          kprintf("Dir not empty\n");
          if (has_path) fat16_chdir(old_cwd);
          return;
        }

        if (lfn_start_s != -1) {
            mark_entries_deleted(slba, lfn_start_s, lfn_start_i, s, i);
        } else {
            mark_entries_deleted(slba, s, i, s, i);
        }

        uint16_t curr = e->cluster_low;
        while (curr >= 2 && curr < 0xFFF8) {
          uint16_t next = get_fat_entry(curr);
          set_fat_entry(curr, 0x0000);
          curr = next;
        }

        if (has_path) fat16_chdir(old_cwd);
        return;
      }
      memset(lfn_buf, 0, 256);
      lfn_start_s = -1;
    }
  }

  if (has_path) fat16_chdir(old_cwd);
}