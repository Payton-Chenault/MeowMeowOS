#include "syscall.h"
#include "../arch/x86/task/task.h"
#include "../fs/fat_16/fat16.h"
#include "../fs/fat_16/fat16_vfs.h"
#include "../kernel_services/kernel_services.h"
#include "../lib/integer_ascii_converters/itoa.h"
#include "../lib/string/string.h"
#include "../mem/physical_memory_manager/pmm.h"
#include "../mem/virtual_memory_manager/vmm.h"
#include "../security/auth/auth.h"
#include "../utils/logging/logger.h"
#include <stdbool.h>
#include <stdint.h>

extern int task_is_root(void);
extern uint32_t get_ticks(void);

#define MODULE "SYSCALL"

typedef struct {
  uint32_t es;
  uint32_t ds;
  uint32_t edi;
  uint32_t esi;
  uint32_t ebp;
  uint32_t esp;
  uint32_t ebx;
  uint32_t edx;
  uint32_t ecx;
  uint32_t eax;
} syscall_regs_t;

#define KERNEL_USER_BOUNDARY 0xC0000000u
#define USER_ADDR_MIN 0x100000u

static bool is_valid_user_ptr(const void *ptr, uint32_t len) {
  if (ptr == NULL) {
    return false;
  }

  uint32_t addr = (uint32_t)ptr;
  if (addr < USER_ADDR_MIN || addr >= KERNEL_USER_BOUNDARY) {
    return false;
  }

  if (len == 0) {
    return true;
  }

  uint32_t end = addr + len;
  if (end < addr || end >= KERNEL_USER_BOUNDARY) {
    return false;
  }

  return true;
}

static bool is_valid_user_cstr(const char *ptr, uint32_t max_len) {
  if (!is_valid_user_ptr(ptr, max_len)) {
    return false;
  }

  for (uint32_t i = 0; i < max_len; i++) {
    char c = ((const char *)ptr)[i];
    if (c == '\0') {
      return true;
    }
  }

  return false;
}

static void write_to_stdout(const char *data, uint32_t len) {
  vfs_node_t *stdout_node = vfs_find("stdout");
  if (stdout_node) {
    vfs_write(stdout_node, 0, len, (uint8_t *)data);
  }
}

static void sys_dir_visitor_callback(fat16_dir_entry_t *entry) {
  if (entry->attributes == 0x0F || entry->filename[0] == 0xE5) {
    return;
  }

  // Build 8.3 filename from FAT entry
  char name[13];
  int pos = 0;

  for (int i = 0; i < 8; i++) {
    char c = entry->filename[i];
    if (c == ' ')
      break;
    name[pos++] = c;
  }

  bool has_ext = false;
  for (int i = 8; i < 11; i++) {
    if (entry->filename[i] != ' ') {
      has_ext = true;
      break;
    }
  }

  if (has_ext) {
    name[pos++] = '.';
    for (int i = 8; i < 11; i++) {
      char c = entry->filename[i];
      if (c == ' ')
        break;
      name[pos++] = c;
    }
  }

  if (entry->attributes & 0x10) {
    name[pos++] = '/';
  }
  name[pos] = '\0';

  char line[128];
  int lp = 0;

  // Filename column: fixed width 24, left-aligned
  int name_len = strlen(name);
  for (int i = 0; i < name_len; i++) {
    line[lp++] = name[i];
  }
  while (lp < 24) {
    line[lp++] = ' ';
  }

  // Size column: right-aligned width 8
  char size_buf[32];
  itoa((int)entry->file_size, size_buf, 10);
  int size_len = strlen(size_buf);

  for (int i = 0; i < 8 - size_len; i++) {
    line[lp++] = ' ';
  }
  for (int i = 0; i < size_len; i++) {
    line[lp++] = size_buf[i];
  }

  line[lp++] = ' ';
  line[lp++] = 'b';
  line[lp++] = 'y';
  line[lp++] = 't';
  line[lp++] = 'e';
  line[lp++] = 's';

  line[lp++] = '\n';
  line[lp] = '\0';

  write_to_stdout(line, lp);
}

void syscall_dispatcher(syscall_regs_t *regs) {
  uint32_t syscall_number = regs->eax;

  switch (syscall_number) {
  case SYS_YIELD:
    task_request_yield();
    regs->eax = 0;
    break;

  case SYS_RETURN:
    task_exit();
    regs->eax = 0;
    break;

  case SYS_OPEN: {
    const char *filename = (const char *)regs->ebx;
    if (!is_valid_user_cstr(filename, 256)) {
      regs->eax = -1;
      break;
    }

    task_t *current = task_get_current();
    vfs_node_t *target_node = vfs_find(filename);
    if (!target_node) {
      target_node = fat16_vfs_open(filename);
    }

    if (!target_node) {
      regs->eax = -1;
      break;
    }

    int free_fd = -1;
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
      if (current->fd_table[i].in_use == false) {
        free_fd = i;
        break;
      }
    }

    if (free_fd == -1) {
      if (target_node->type == VFS_FILE)
        kmem_free(target_node);
      regs->eax = -1;
      break;
    }

    current->fd_table[free_fd].in_use = true;
    strcpy(current->fd_table[free_fd].filename, filename);
    current->fd_table[free_fd].file_size = target_node->length;
    current->fd_table[free_fd].current_offset = 0;
    current->fd_table[free_fd].node = target_node;
    regs->eax = free_fd;
    break;
  }

  case SYS_CLOSE: {
    int fd = (int)regs->ebx;
    task_t *current = task_get_current();

    if (fd < 0 || fd >= MAX_OPEN_FILES || !current->fd_table[fd].in_use) {
      regs->eax = -1;
      break;
    }

    vfs_node_t *node = current->fd_table[fd].node;
    if (node && node->type == VFS_FILE) {
      kmem_free(node);
    }

    current->fd_table[fd].in_use = false;
    current->fd_table[fd].node = NULL;
    memset(current->fd_table[fd].filename, 0, 32);
    current->fd_table[fd].current_offset = 0;
    regs->eax = 0;
    break;
  }

  case SYS_READ: {
    int fd = (int)regs->ebx;
    uint8_t *buffer = (uint8_t *)regs->ecx;
    uint32_t bytes_to_read = regs->edx;

    if (!is_valid_user_ptr(buffer, bytes_to_read)) {
      regs->eax = -1;
      break;
    }

    task_t *current = task_get_current();
    if (fd < 0 || fd >= MAX_OPEN_FILES || !current->fd_table[fd].in_use) {
      regs->eax = -1;
      break;
    }

    vfs_node_t *node = current->fd_table[fd].node;
    uint32_t off = current->fd_table[fd].current_offset;
    uint32_t bytes_read = vfs_read(node, off, bytes_to_read, buffer);
    current->fd_table[fd].current_offset += bytes_read;

    if (fd == 0 && bytes_read > 0) {
      write_to_stdout((const char *)buffer, bytes_read);
    }

    regs->eax = bytes_read;
    break;
  }

  case SYS_WRITE: {
    int fd = (int)regs->ebx;
    uint8_t *buffer = (uint8_t *)regs->ecx;
    uint32_t bytes_to_write = regs->edx;

    if (!is_valid_user_ptr(buffer, bytes_to_write)) {
      regs->eax = -1;
      break;
    }

    task_t *current = task_get_current();
    if (fd < 0 || fd >= MAX_OPEN_FILES || !current->fd_table[fd].in_use) {
      regs->eax = -1;
      break;
    }

    vfs_node_t *node = current->fd_table[fd].node;
    uint32_t off = current->fd_table[fd].current_offset;
    uint32_t bytes_written = vfs_write(node, off, bytes_to_write, buffer);
    current->fd_table[fd].current_offset += bytes_written;
    regs->eax = bytes_written;
    break;
  }

  case SYS_FORMAT: {
    if (!task_is_root()) {
      regs->eax = (uint32_t)-1;
      break;
    }
    fat16_format_drive(0x80, 0, NULL, true);
    regs->eax = 0;
    break;
  }

  case SYS_LIST_DIR: {
    const char *path = (const char *)regs->ebx;
    if (!is_valid_user_cstr(path, 256)) {
      regs->eax = -1;
      break;
    }

    fat16_chdir(path);
    fat16_list(sys_dir_visitor_callback);

    regs->eax = 0;
    break;
  }

  case SYS_MKDIR: {
    const char *path = (const char *)regs->ebx;
    if (!is_valid_user_cstr(path, 256)) {
      regs->eax = -1;
      break;
    }

    char path_copy[256];
    strcpy(path_copy, path);

    // If absolute, split into parent and basename
    if (path_copy[0] == '/') {
      char *last_slash = strrchr(path_copy, '/');
      if (last_slash == NULL) {
        regs->eax = -1;
        break;
      }

      // Extract parent directory (if last_slash == path_copy, parent is "/")
      char parent[256];
      char basename[256];
      if (last_slash == path_copy) {
        strcpy(parent, "/");
        strcpy(basename, last_slash + 1);
      } else {
        *last_slash = '\0';
        strcpy(parent, path_copy);
        strcpy(basename, last_slash + 1);
      }

      // Save current cwd
      char old_cwd[256];
      strcpy(old_cwd, fat16_get_current_path());

      if (fat16_chdir(parent) !=
          0) { // need return value; current fat16_chdir is void
        regs->eax = -1;
        fat16_chdir(old_cwd);
        break;
      }

      fat16_create_dir(basename);

      fat16_chdir(old_cwd);
      regs->eax = 0;
    } else {
      fat16_create_dir(path);
      regs->eax = 0;
    }
    break;
  }

  case SYS_RMDIR: {
    const char *path = (const char *)regs->ebx;
    if (!is_valid_user_cstr(path, 256)) {
      regs->eax = -1;
      break;
    }
    fat16_delete_dir(path);
    regs->eax = 0;
    break;
  }

  case SYS_REMOVE: {
    const char *path = (const char *)regs->ebx;
    if (!is_valid_user_cstr(path, 256)) {
      regs->eax = -1;
      break;
    }
    fat16_delete_file(path);
    regs->eax = 0;
    break;
  }

  case SYS_CREATE: {
    const char *path = (const char *)regs->ebx;
    if (!is_valid_user_cstr(path, 256)) {
      regs->eax = -1;
      break;
    }

    // Create file with one placeholder byte so size != 0
    fat16_write_file(path, (uint8_t *)" ", 1);

    task_t *current = task_get_current();
    vfs_node_t *target_node = fat16_vfs_open(path);
    if (!target_node) {
      regs->eax = -1;
      break;
    }

    int free_fd = -1;
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
      if (current->fd_table[i].in_use == false) {
        free_fd = i;
        break;
      }
    }

    if (free_fd == -1) {
      kmem_free(target_node);
      regs->eax = -1;
      break;
    }

    current->fd_table[free_fd].in_use = true;
    strcpy(current->fd_table[free_fd].filename, path);
    current->fd_table[free_fd].file_size = target_node->length;
    current->fd_table[free_fd].current_offset = 0;
    current->fd_table[free_fd].node = target_node;
    regs->eax = free_fd;
    break;
  }

  case SYS_UPTIME: {
    regs->eax = get_ticks();
    break;
  }

  case SYS_ALLOC_PAGE: {
    task_t *current = task_get_current();
    uint32_t page_dir_phys = current->page_directory;

    void *phys = pmm_alloc_block();
    if (!phys) {
      regs->eax = 0;
      break;
    }

    static uint32_t next_user_page = 0xD0000000;
    uint32_t virt = next_user_page;
    next_user_page += 4096;

    vmm_map_page_in_directory(page_dir_phys, phys, (void *)virt,
                              PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    regs->eax = virt;
    break;
  }

  case SYS_FREE_PAGE: {
    regs->eax = 0;
    break;
  }

  case SYS_CHDIR: {
    const char *path = (const char *)regs->ebx;
    if (!is_valid_user_cstr(path, 256)) {
      regs->eax = -1;
      break;
    }
    fat16_chdir(path);
    regs->eax = 0;
    break;
  }

  case SYS_COPY_FILE: {
    const char *src = (const char *)regs->ebx;
    const char *dst = (const char *)regs->ecx;
    if (!is_valid_user_cstr(src, 256) || !is_valid_user_cstr(dst, 256)) {
      regs->eax = -1;
      break;
    }

    regs->eax = fat16_copy_file(src, dst);
    break;
  }

  default:
    regs->eax = -1;
    break;
  }
}