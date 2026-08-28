#include "syscall.h"
#include "../arch/x86/task/task.h"
#include "../drivers/pci/pci.h"
#include "../drivers/acpi/acpi.h"
#include "../drivers/mouse/mouse.h"
#include "../fs/fat_16/fat16.h"
#include "../fs/fat_16/fat16_vfs.h"
#include "../kernel_services/kernel_services.h"
#include "../lib/integer_ascii_converters/itoa.h"
#include "../lib/path/resolve_path.h"
#include "../lib/string/string.h"
#include "../mem/physical_memory_manager/pmm.h"
#include "../mem/virtual_memory_manager/vmm.h"
#include "../drivers/cmos/rtc.h"
#include "../progs/elf/elf.h"
#include "../security/auth/auth.h"
#include "../utils/logging/logger.h"
#include "../fs/pipe/pipe.h"
#include "../net/net.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

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
  if (ptr == NULL)
    return false;

  uint32_t addr = (uint32_t)ptr;
  if (addr < USER_ADDR_MIN || addr >= KERNEL_USER_BOUNDARY)
    return false;

  if (len == 0)
    return true;

  uint32_t end = addr + len;
  if (end < addr || end >= KERNEL_USER_BOUNDARY)
    return false;

  return true;
}

static bool is_valid_user_cstr(const char *ptr, uint32_t max_len) {
  if (ptr == NULL)
    return false;

  uint32_t addr = (uint32_t)ptr;
  for (uint32_t i = 0; i < max_len; i++) {
    uint32_t current = addr + i;
    if (current < USER_ADDR_MIN || current >= KERNEL_USER_BOUNDARY) {
      return false;
    }
    char c = *((volatile char *)current);
    if (c == '\0') {
      return true;
    }
  }
  return false;
}

static bool normalize_user_path(const char *user_path, char *out,
                                size_t out_size) {
  if (!is_valid_user_cstr(user_path, 256))
    return false;

  char cwd[256];
  strncpy(cwd, fat16_get_current_path(), sizeof(cwd) - 1);
  cwd[sizeof(cwd) - 1] = '\0';

  return resolve_path(cwd, user_path, out, out_size);
}

static void write_to_stdout(const char *data, uint32_t len) {
  task_t *current = task_get_current();
  if (current != NULL) {
    current->last_output_tick = get_ticks();
  }

  if (current != NULL && current->fd_table[1].in_use && current->fd_table[1].node != NULL) {
    uint32_t w = vfs_write(current->fd_table[1].node, current->fd_table[1].current_offset, len, (uint8_t *)data);
    current->fd_table[1].current_offset += w;
  } else {
    vfs_node_t *stdout_node = vfs_find("stdout");
    if (stdout_node) {
      vfs_write(stdout_node, 0, len, (uint8_t *)data);
    }
  }
}

static void sys_dir_visitor_callback(fat16_dir_entry_t *entry, const char *lfn_name) {
  if (entry->attributes == 0x0F || (uint8_t)entry->filename[0] == 0xE5) {
    return;
  }
  
  char line[128];
  int lp = 0;
  
  int name_len = strlen(lfn_name);
  for (int i = 0; i < name_len && i < 64; i++) {
    line[lp++] = lfn_name[i];
  }
  
  if (entry->attributes & 0x10) {
    line[lp++] = '/';
  }
  
  while (lp < 32) line[lp++] = ' ';
  
  char size_buf[32];
  itoa((int)entry->file_size, size_buf, 10);
  int size_len = strlen(size_buf);
  for (int i = 0; i < size_len; i++) line[lp++] = size_buf[i];
  
  line[lp++] = ' '; line[lp++] = 'b'; line[lp++] = 'y';
  line[lp++] = 't'; line[lp++] = 'e'; line[lp++] = 's';
  line[lp++] = '\n'; line[lp] = '\0';
  
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
    int exit_code = (int)regs->ebx;
    task_exit_with_status(exit_code);
    regs->eax = 0;
    break;

  case SYS_OPEN: {
    const char *user_filename = (const char *)regs->ebx;
    char filename[256];

    if (!normalize_user_path(user_filename, filename, sizeof(filename))) {
      regs->eax = -1;
      break;
    }

    task_t *current = task_get_current();
    vfs_node_t *target_node = vfs_open(filename);
    if (!target_node) {
      target_node = fat16_vfs_open(filename);
    }

    if (!target_node) {
      regs->eax = -1;
      break;
    }

    if (!vfs_check_access(target_node, current, VFS_ACCESS_READ)) {
      vfs_close(target_node);
      regs->eax = -1;
      break;
    }

    if (vfs_is_device(target_node) && !task_has_cap(current, CAP_DEV_OPEN)) {
      vfs_close(target_node);
      regs->eax = -1;
      break;
    }

    int free_fd = -1;
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
      if (!current->fd_table[i].in_use) {
        free_fd = i;
        break;
      }
    }

    if (free_fd == -1) {
      vfs_close(target_node);
      regs->eax = -1;
      break;
    }

    current->fd_table[free_fd].in_use = true;
    strncpy(current->fd_table[free_fd].filename, filename,
            sizeof(current->fd_table[free_fd].filename) - 1);
    current->fd_table[free_fd].filename[sizeof(current->fd_table[free_fd].filename) - 1] = '\0';
    current->fd_table[free_fd].file_size = target_node->length;
    current->fd_table[free_fd].current_offset = 0;
    current->fd_table[free_fd].node = target_node;

    regs->eax = free_fd;
    break;
  }

  case SYS_CLOSE: {
    int fd = (int)regs->ebx;
    task_t *current_task_close = task_get_current();

    if (fd < 0 || fd >= MAX_OPEN_FILES || !current_task_close->fd_table[fd].in_use) {
      regs->eax = -1;
      break;
    }

    vfs_node_t *node = current_task_close->fd_table[fd].node;
    if (node != NULL) {
      vfs_close(node);
    }

    current_task_close->fd_table[fd].in_use = false;
    current_task_close->fd_table[fd].node = NULL;
    memset(current_task_close->fd_table[fd].filename, 0,
           sizeof(current_task_close->fd_table[fd].filename));
    current_task_close->fd_table[fd].current_offset = 0;
    current_task_close->fd_table[fd].file_size = 0;

    regs->eax = 0;
    break;
  }

  case SYS_READ: {
    int read_fd = (int)regs->ebx;
    uint8_t *buffer = (uint8_t *)regs->ecx;
    uint32_t bytes_to_read = regs->edx;

    if (!is_valid_user_ptr(buffer, bytes_to_read)) {
      regs->eax = -1;
      break;
    }

    task_t *current_task_read = task_get_current();

    if (read_fd < 0 || read_fd >= MAX_OPEN_FILES || !current_task_read->fd_table[read_fd].in_use) {
      regs->eax = -1;
      break;
    }

    vfs_node_t *read_node = current_task_read->fd_table[read_fd].node;
    uint32_t off = current_task_read->fd_table[read_fd].current_offset;

    if (read_node->type == VFS_FILE && off >= read_node->length) {
      regs->eax = 0;
      break;
    }

    uint32_t bytes_read = vfs_read(read_node, off, bytes_to_read, buffer);
    current_task_read->fd_table[read_fd].current_offset += bytes_read;

    if (read_fd == 0 && bytes_read > 0 && read_node != NULL && strcmp(read_node->name, "stdin") == 0) {
      write_to_stdout((const char *)buffer, bytes_read);
      current_task_read->last_output_tick = get_ticks();
    }

    regs->eax = bytes_read;
    break;
  }

  case SYS_WRITE: {
    int write_fd = (int)regs->ebx;
    uint8_t *w_buffer = (uint8_t *)regs->ecx;
    uint32_t bytes_to_write = regs->edx;

    if (!is_valid_user_ptr(w_buffer, bytes_to_write)) {
      regs->eax = -1;
      break;
    }

    task_t *current_task_write = task_get_current();

    if (write_fd < 0 || write_fd >= MAX_OPEN_FILES || !current_task_write->fd_table[write_fd].in_use) {
      regs->eax = -1;
      break;
    }

    if (write_fd == 1 || write_fd == 2) {
      current_task_write->last_output_tick = get_ticks();
    }

    vfs_node_t *write_node = current_task_write->fd_table[write_fd].node;

    if (!vfs_check_access(write_node, current_task_write, VFS_ACCESS_WRITE)) {
      regs->eax = -1;
      break;
    }

    uint32_t w_off = current_task_write->fd_table[write_fd].current_offset;
    uint32_t bytes_written = vfs_write(write_node, w_off, bytes_to_write, w_buffer);
    current_task_write->fd_table[write_fd].current_offset += bytes_written;

    regs->eax = bytes_written;
    break;
  }

  case SYS_FORMAT: {
    if (!task_has_cap(task_get_current(), CAP_FS_FORMAT)) {
      regs->eax = -1;
      break;
    }
    fat16_format_drive(0x80, 0, NULL, true);
    regs->eax = 0;
    break;
  }

  case SYS_LIST_DIR: {
    const char *user_path = (const char *)regs->ebx;
    char path[256];

    if (!normalize_user_path(user_path, path, sizeof(path))) {
      regs->eax = -1;
      break;
    }

    char saved_path[256];
    strcpy(saved_path, fat16_get_current_path());

    if (fat16_chdir(path) != 0) {
      regs->eax = -1;
      break;
    }

    fat16_list(sys_dir_visitor_callback);
    fat16_chdir(saved_path);
    regs->eax = 0;
    break;
  }

  case SYS_MKDIR: {
    const char *mkdir_path_user = (const char *)regs->ebx;
    char mkdir_path[256];

    if (!normalize_user_path(mkdir_path_user, mkdir_path, sizeof(mkdir_path))) {
      regs->eax = -1;
      break;
    }

    if (!task_has_cap(task_get_current(), CAP_FS_WRITE)) {
      regs->eax = -1;
      break;
    }

    regs->eax = fat16_create_dir(mkdir_path);
    break;
  }

  case SYS_RMDIR: {
    const char *rmdir_path_user = (const char *)regs->ebx;
    char rmdir_path[256];

    if (!normalize_user_path(rmdir_path_user, rmdir_path, sizeof(rmdir_path))) {
      regs->eax = -1;
      break;
    }

    if (!task_has_cap(task_get_current(), CAP_FS_WRITE)) {
      regs->eax = -1;
      break;
    }

    fat16_delete_dir(rmdir_path);
    regs->eax = 0;
    break;
  }

  case SYS_REMOVE: {
    const char *remove_path_user = (const char *)regs->ebx;
    char remove_path[256];

    if (!normalize_user_path(remove_path_user, remove_path, sizeof(remove_path))) {
      regs->eax = -1;
      break;
    }

    if (!task_has_cap(task_get_current(), CAP_FS_WRITE)) {
      regs->eax = -1;
      break;
    }

    fat16_delete_file(remove_path);
    regs->eax = 0;
    break;
  }

  case SYS_CREATE: {
    const char *create_path_user = (const char *)regs->ebx;
    char create_path[256];

    if (!normalize_user_path(create_path_user, create_path, sizeof(create_path))) {
      regs->eax = -1;
      break;
    }

    if (!task_has_cap(task_get_current(), CAP_FS_WRITE)) {
      regs->eax = -1;
      break;
    }

    fat16_write_file(create_path, (uint8_t *)" ", 1);

    task_t *current_task_create = task_get_current();
    vfs_node_t *target_node_create = fat16_vfs_open(create_path);

    if (!target_node_create) {
      regs->eax = -1;
      break;
    }

    int free_fd_create = -1;
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
      if (current_task_create->fd_table[i].in_use == false) {
        free_fd_create = i;
        break;
      }
    }

    if (free_fd_create == -1) {
      vfs_close(target_node_create);
      regs->eax = -1;
      break;
    }

    current_task_create->fd_table[free_fd_create].in_use = true;
    strncpy(current_task_create->fd_table[free_fd_create].filename, create_path,
            sizeof(current_task_create->fd_table[free_fd_create].filename) - 1);
    current_task_create->fd_table[free_fd_create].filename[sizeof(current_task_create->fd_table[free_fd_create].filename) - 1] = '\0';
    current_task_create->fd_table[free_fd_create].file_size = target_node_create->length;
    current_task_create->fd_table[free_fd_create].current_offset = 0;
    current_task_create->fd_table[free_fd_create].node = target_node_create;

    regs->eax = free_fd_create;
    break;
  }

  case SYS_UPTIME:
    regs->eax = get_ticks();
    break;

  case SYS_ALLOC_PAGE: {
    task_t *current_task_alloc = task_get_current();
    if (!current_task_alloc) {
      regs->eax = 0;
      break;
    }

    uint32_t page_dir_phys = current_task_alloc->page_directory;
    void *phys = pmm_alloc_block();
    if (!phys) {
      regs->eax = 0;
      break;
    }

    uint32_t virt = current_task_alloc->next_user_vaddr;
    if (virt < USER_VIRT_MIN || virt >= USER_STACK_TOP) {
      pmm_free_block(phys);
      regs->eax = 0;
      break;
    }

    current_task_alloc->next_user_vaddr += PAGE_SIZE;
    if (current_task_alloc->next_user_vaddr >= USER_STACK_TOP) {
      current_task_alloc->next_user_vaddr = USER_STACK_TOP;
    }

    vmm_map_page_in_directory(page_dir_phys, phys, (void *)virt,
                              PAGE_PRESENT | PAGE_WRITE | PAGE_USER);

    regs->eax = virt;
    break;
  }

  case SYS_FREE_PAGE: {
    task_t *current_task_free = task_get_current();
    void *virt_to_free = (void *)regs->ebx;

    if (!current_task_free || virt_to_free == NULL || (uint32_t)virt_to_free < USER_VIRT_MIN ||
        (uint32_t)virt_to_free >= KERNEL_VIRT_START || ((uint32_t)virt_to_free & 0xFFF) != 0) {
      regs->eax = -1;
      break;
    }

    uint32_t virt_addr = (uint32_t)virt_to_free;
    uint32_t pd_index = virt_addr >> 22;
    uint32_t pt_index = (virt_addr >> 12) & 0x3FF;

    uint32_t *page_directory = (uint32_t *)current_task_free->page_directory;

    if (!(page_directory[pd_index] & PAGE_PRESENT)) {
      regs->eax = -1;
      break;
    }

    uint32_t *page_table = (uint32_t *)(page_directory[pd_index] & ~0xFFF);
    uint32_t pte = page_table[pt_index];

    if (!(pte & PAGE_PRESENT)) {
      regs->eax = -1;
      break;
    }

    pmm_free_block((void *)(pte & ~0xFFF));
    page_table[pt_index] = 0;

    regs->eax = 0;
    break;
  }

  case SYS_CHDIR: {
    const char *chdir_path_user = (const char *)regs->ebx;
    char chdir_path[256];

    if (!normalize_user_path(chdir_path_user, chdir_path, sizeof(chdir_path))) {
      regs->eax = -1;
      break;
    }

    regs->eax = fat16_chdir(chdir_path);
    break;
  }

  case SYS_COPY_FILE: {
    const char *src_user = (const char *)regs->ebx;
    const char *dst_user = (const char *)regs->ecx;
    char src[256];
    char dst[256];

    if (!normalize_user_path(src_user, src, sizeof(src)) ||
        !normalize_user_path(dst_user, dst, sizeof(dst))) {
      regs->eax = -1;
      break;
    }

    if (!task_has_cap(task_get_current(), CAP_FS_WRITE)) {
      regs->eax = -1;
      break;
    }

    regs->eax = fat16_copy_file(src, dst);
    break;
  }

  case SYS_LSEEK: {
    int seek_fd = (int)regs->ebx;
    int32_t offset = (int32_t)regs->ecx;
    uint32_t whence = regs->edx;

    task_t *current_seek = task_get_current();

    if (seek_fd < 0 || seek_fd >= MAX_OPEN_FILES || !current_seek->fd_table[seek_fd].in_use) {
      regs->eax = -1;
      break;
    }

    file_descriptor_t *fdp = &current_seek->fd_table[seek_fd];
    int64_t new_off = 0;

    if (whence == SEEK_SET) {
      new_off = offset;
    } else if (whence == SEEK_CUR) {
      new_off = (int64_t)fdp->current_offset + offset;
    } else if (whence == SEEK_END) {
      new_off = (int64_t)fdp->file_size + offset;
    } else {
      regs->eax = -1;
      break;
    }

    if (new_off < 0) {
      regs->eax = -1;
      break;
    }

    fdp->current_offset = (uint32_t)new_off;
    regs->eax = fdp->current_offset;
    break;
  }

  case SYS_STAT: {
    const char *stat_path_user = (const char *)regs->ebx;
    sys_stat_t *stat_buf = (sys_stat_t *)regs->ecx;

    if (!is_valid_user_ptr(stat_buf, sizeof(sys_stat_t))) {
      regs->eax = -1;
      break;
    }

    char stat_path[256];
    if (!normalize_user_path(stat_path_user, stat_path, sizeof(stat_path))) {
      regs->eax = -1;
      break;
    }

    vfs_node_t *node_stat = fat16_vfs_open(stat_path);
    if (node_stat == NULL) {
      regs->eax = -1;
      break;
    }

    stat_buf->size = node_stat->length;
    stat_buf->type = node_stat->type;
    stat_buf->uid = node_stat->uid;
    stat_buf->gid = node_stat->gid;
    stat_buf->mode = node_stat->mode;

    vfs_close(node_stat);
    regs->eax = 0;
    break;
  }

  case SYS_GETCWD: {
    char *cwd_buf = (char *)regs->ebx;
    uint32_t size = regs->ecx;

    if (!is_valid_user_ptr(cwd_buf, size)) {
      regs->eax = -1;
      break;
    }

    const char *cwd = fat16_get_current_path();
    uint32_t len = strlen(cwd);
    if (len >= size) {
      len = size - 1;
    }
    memcpy(cwd_buf, cwd, len);
    cwd_buf[len] = '\0';
    regs->eax = (uint32_t)len;
    break;
  }

  case SYS_DUP: {
    int old_fd = (int)regs->ebx;
    task_t *current_dup = task_get_current();

    if (old_fd < 0 || old_fd >= MAX_OPEN_FILES ||
        !current_dup->fd_table[old_fd].in_use) {
      regs->eax = -1;
      break;
    }

    int new_fd = -1;
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
      if (!current_dup->fd_table[i].in_use) {
        new_fd = i;
        break;
      }
    }

    if (new_fd == -1) {
      regs->eax = -1;
      break;
    }

    file_descriptor_t *src = &current_dup->fd_table[old_fd];
    file_descriptor_t *dst = &current_dup->fd_table[new_fd];

    dst->in_use = true;
    strncpy(dst->filename, src->filename, sizeof(dst->filename) - 1);
    dst->filename[sizeof(dst->filename) - 1] = '\0';
    dst->current_offset = src->current_offset;
    dst->file_size = src->file_size;
    dst->node = vfs_retain(src->node);

    regs->eax = new_fd;
    break;
  }

  case SYS_DUP2: {
    int old_fd = (int)regs->ebx;
    int new_fd = (int)regs->ecx;
    task_t *current_dup2 = task_get_current();

    if (old_fd < 0 || old_fd >= MAX_OPEN_FILES ||
        !current_dup2->fd_table[old_fd].in_use ||
        new_fd < 0 || new_fd >= MAX_OPEN_FILES) {
      regs->eax = -1;
      break;
    }

    if (old_fd == new_fd) {
      regs->eax = new_fd;
      break;
    }

    file_descriptor_t *src = &current_dup2->fd_table[old_fd];
    file_descriptor_t *dst = &current_dup2->fd_table[new_fd];

    if (dst->in_use) {
      if (dst->node) {
        vfs_close(dst->node);
      }
      memset(dst->filename, 0, sizeof(dst->filename));
      dst->node = NULL;
      dst->current_offset = 0;
      dst->file_size = 0;
    }

    dst->in_use = true;
    strncpy(dst->filename, src->filename, sizeof(dst->filename) - 1);
    dst->filename[sizeof(dst->filename) - 1] = '\0';
    dst->current_offset = src->current_offset;
    dst->file_size = src->file_size;
    dst->node = vfs_retain(src->node);

    regs->eax = new_fd;
    break;
  }

  case SYS_GET_DESCRIPTION: {
    const char *user_path_desc = (const char *)regs->ebx;
    char *user_buf_desc = (char *)regs->ecx;
    uint32_t size = regs->edx;

    if (!is_valid_user_ptr(user_buf_desc, size)) {
      regs->eax = -1;
      break;
    }

    char desc_path[256];
    if (!normalize_user_path(user_path_desc, desc_path, sizeof(desc_path))) {
      regs->eax = -1;
      break;
    }

    uint32_t ret = elf_get_description(desc_path, user_buf_desc, size);
    regs->eax = ret ? 0 : -1;
    break;
  }

  case SYS_SYSLOG: {
    char *user_buf_syslog = (char *)regs->ebx;
    uint32_t size = regs->ecx;

    if (!is_valid_user_ptr(user_buf_syslog, size)) {
      regs->eax = -1;
      break;
    }

    regs->eax = logger_read_log(user_buf_syslog, size);
    break;
  }

  case SYS_GET_MEM_INFO: {
    sys_mem_info_t *info = (sys_mem_info_t *)regs->ebx;
    
    if (!is_valid_user_ptr(info, sizeof(sys_mem_info_t))) {
      regs->eax = -1;
      break;
    }
    
    pmm_get_memory_info(&info->total_bytes, &info->used_bytes, &info->free_bytes);
    regs->eax = 0;
    break;
  }

  case SYS_GET_PROCESS_INFO: {
    sys_process_info_t *proc_buf = (sys_process_info_t *)regs->ebx;
    uint32_t max_entries = regs->ecx;
    
    if (!is_valid_user_ptr(proc_buf, sizeof(sys_process_info_t) * max_entries)) {
      regs->eax = -1;
      break;
    }
    
    regs->eax = task_get_process_info(proc_buf, max_entries);
    break;
  }

  case SYS_SET_PRIORITY: {
    uint32_t pid_prio = regs->ebx;
    uint8_t prio = (uint8_t)regs->ecx;
    task_set_priority(pid_prio, prio);
    regs->eax = 0;
    break;
  }

  case SYS_GET_PRIORITY: {
    uint32_t pid_gprio = regs->ebx;
    regs->eax = task_get_priority(pid_gprio);
    break;
  }

  case SYS_GET_TIME: {
    sys_time_t *time_buf = (sys_time_t *)regs->ebx;
    if (!is_valid_user_ptr(time_buf, sizeof(sys_time_t))) {
      regs->eax = -1;
      break;
    }
    rtc_time_t rtc;
    rtc_get_time(&rtc);
    time_buf->second = rtc.second;
    time_buf->minute = rtc.minute;
    time_buf->hour   = rtc.hour;
    time_buf->day    = rtc.day;
    time_buf->month  = rtc.month;
    time_buf->year   = rtc.year;
    regs->eax = 0;
    break;
  }

  case SYS_SBRK: {
    int32_t increment = (int32_t)regs->ebx;
    task_t *current_sbrk = task_get_current();
    if (!current_sbrk) {
      regs->eax = (uint32_t)-1;
      break;
    }

    uint32_t old_break = current_sbrk->heap_break;
    if (old_break == 0) {
      old_break = current_sbrk->heap_start ? current_sbrk->heap_start : 0x10000000;
      current_sbrk->heap_start = old_break;
      current_sbrk->heap_break = old_break;
    }

    if (increment == 0) {
      regs->eax = old_break;
      break;
    }

    uint32_t new_break = old_break + increment;
    if (increment < 0) {
      if (new_break < current_sbrk->heap_start) {
        regs->eax = (uint32_t)-1;
        break;
      }
    } else {
      if (new_break >= 0xB0000000u || new_break < old_break) {
        regs->eax = (uint32_t)-1;
        break;
      }

      uint32_t start_page = (old_break + 0xFFF) & ~0xFFF;
      uint32_t end_page = (new_break + 0xFFF) & ~0xFFF;
      bool failed = false;

      for (uint32_t page = start_page; page < end_page; page += 4096) {
        void *phys_block = pmm_alloc_block();
        if (phys_block == NULL) {
          failed = true;
          break;
        }
        memset(phys_block, 0, 4096);
        vmm_map_page_in_directory(current_sbrk->page_directory, phys_block, (void *)page,
                                  PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
      }
      if (failed) {
        regs->eax = (uint32_t)-1;
        break;
      }
    }

    current_sbrk->heap_break = new_break;
    regs->eax = old_break;
    break;
  }

  case SYS_LOG: {
    int level = (int)regs->ebx;
    const char *user_module = (const char *)regs->ecx;
    const char *user_msg = (const char *)regs->edx;

    if (!is_valid_user_cstr(user_module, 64) || !is_valid_user_cstr(user_msg, 1024)) {
      regs->eax = -1;
      break;
    }

    switch (level) {
    case LOG_LEVEL_TRACE:
      log_trace(user_module, "%s", user_msg);
      break;
    case LOG_LEVEL_DEBUG:
      log_debug(user_module, "%s", user_msg);
      break;
    case LOG_LEVEL_INFO:
      log_info(user_module, "%s", user_msg);
      break;
    case LOG_LEVEL_WARNING:
      log_warning(user_module, "%s", user_msg);
      break;
    case LOG_LEVEL_ERROR:
      log_error(user_module, "%s", user_msg);
      break;
    default:
      log_debug(user_module, "%s", user_msg);
      break;
    }
    regs->eax = 0;
    break;
  }

  case SYS_PIPE: {
    int *user_pipefd = (int *)regs->ebx;

    if (!is_valid_user_ptr(user_pipefd, sizeof(int) * 2)) {
      regs->eax = -1;
      break;
    }

    vfs_node_t *read_node_p = NULL;
    vfs_node_t *write_node_p = NULL;

    if (pipe_create(&read_node_p, &write_node_p) < 0) {
      regs->eax = -1;
      break;
    }

    task_t *current_pipe = task_get_current();
    int r_fd = -1;
    int w_fd = -1;

    for (int i = 0; i < MAX_OPEN_FILES; i++) {
      if (!current_pipe->fd_table[i].in_use) {
        if (r_fd == -1) {
          r_fd = i;
        } else {
          w_fd = i;
          break;
        }
      }
    }

    if (r_fd == -1 || w_fd == -1) {
      vfs_close(read_node_p);
      vfs_close(write_node_p);
      regs->eax = -1;
      break;
    }

    current_pipe->fd_table[r_fd].in_use = true;
    strcpy(current_pipe->fd_table[r_fd].filename, "pipe:[read]");
    current_pipe->fd_table[r_fd].current_offset = 0;
    current_pipe->fd_table[r_fd].file_size = 0;
    current_pipe->fd_table[r_fd].node = read_node_p;

    current_pipe->fd_table[w_fd].in_use = true;
    strcpy(current_pipe->fd_table[w_fd].filename, "pipe:[write]");
    current_pipe->fd_table[w_fd].current_offset = 0;
    current_pipe->fd_table[w_fd].file_size = 0;
    current_pipe->fd_table[w_fd].node = write_node_p;

    user_pipefd[0] = r_fd;
    user_pipefd[1] = w_fd;
    regs->eax = 0;
    break;
  }

  case SYS_KILL: {
    uint32_t target_pid = (uint32_t)regs->ebx;
    int signum = (int)regs->ecx;
    regs->eax = task_send_signal(target_pid, signum);
    break;
  }

  case SYS_SIGNAL: {
    int signum = (int)regs->ebx;
    sighandler_t handler = (sighandler_t)regs->ecx;
    regs->eax = (uint32_t)task_set_signal_handler(signum, handler);
    break;
  }

  case SYS_SET_BUSY: {
    bool is_busy = (bool)regs->ebx;
    task_t *cur = task_get_current();
    if (cur) {
      cur->wants_spinner = is_busy;
      cur->last_output_tick = get_ticks();
    }
    regs->eax = 0;
    break;
  }

  case SYS_GET_PCI_DEVICES: {
    sys_pci_device_t *pci_buf = (sys_pci_device_t *)regs->ebx;
    uint32_t max_entries = regs->ecx;
    if (!is_valid_user_ptr(pci_buf, sizeof(sys_pci_device_t) * max_entries)) {
      regs->eax = -1;
      break;
    }
    regs->eax = pci_get_devices((pci_device_t *)pci_buf, max_entries);
    break;
  }

  case SYS_POWEROFF: {
    acpi_poweroff();
    regs->eax = 0;
    break;
  }

  case SYS_REBOOT: {
    acpi_reboot();
    regs->eax = 0;
    break;
  }

  case SYS_PING: {
    uint32_t target_ip = regs->ebx;
    uint32_t *latency_out = (uint32_t *)regs->ecx;
    if (!is_valid_user_ptr(latency_out, sizeof(uint32_t))) {
      log_error(MODULE, "Invalid user pointer provided for ping latency output");
      regs->eax = -1;
      break;
    }
    log_trace(MODULE, "Handling SYS_PING for target IP: %u.%u.%u.%u", 
              target_ip & 0xFF, (target_ip >> 8) & 0xFF, (target_ip >> 16) & 0xFF, (target_ip >> 24) & 0xFF);
    regs->eax = net_ping(target_ip, latency_out);
    break;
  }

  case SYS_GET_MOUSE: {
    sys_mouse_state_t *user_state = (sys_mouse_state_t *)regs->ebx;
    if (!is_valid_user_ptr(user_state, sizeof(sys_mouse_state_t))) {
      log_error(MODULE, "Invalid user pointer provided for mouse state query");
      regs->eax = -1;
      break;
    }
    mouse_state_t mstate;
    mouse_get_state(&mstate);
    user_state->x = mstate.x;
    user_state->y = mstate.y;
    user_state->buttons = mstate.buttons;
    log_trace(MODULE, "SYS_GET_MOUSE returned pos=(%d, %d), buttons=0x%X", mstate.x, mstate.y, mstate.buttons);
    regs->eax = 0;
    break;
  }

  default:
    log_warning(MODULE, "Unhandled syscall number: %d", syscall_number);
    regs->eax = -1;
    break;
  }

  /* Process any queued signals prior to returning to user execution */
  task_t *cur = task_get_current();
  if (cur != NULL && cur->is_user) {
    task_check_signals();
  }
}