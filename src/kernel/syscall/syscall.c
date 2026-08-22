#include "syscall.h"
#include "../arch/x86/task/task.h"
#include "../fs/fat_16/fat16.h"
#include "../fs/fat_16/fat16_vfs.h"
#include "../kernel_services/kernel_services.h"
#include "../lib/integer_ascii_converters/itoa.h"
#include "../lib/path/resolve_path.h"
#include "../lib/string/string.h"
#include "../mem/physical_memory_manager/pmm.h"
#include "../mem/virtual_memory_manager/vmm.h"
#include "../security/auth/auth.h"
#include "../utils/logging/logger.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

extern uint32_t get_ticks(void);

#define MODULE "SYSCALL"

typedef struct
{
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

static bool is_valid_user_ptr(const void *ptr, uint32_t len)
{
  if (ptr == NULL)
  {
    return false;
  }

  uint32_t addr = (uint32_t)ptr;
  if (addr < USER_ADDR_MIN || addr >= KERNEL_USER_BOUNDARY)
  {
    return false;
  }

  if (len == 0)
  {
    return true;
  }

  uint32_t end = addr + len;
  if (end < addr || end >= KERNEL_USER_BOUNDARY)
  {
    return false;
  }

  return true;
}

static bool is_valid_user_cstr(const char *ptr, uint32_t max_len)
{
  if (ptr == NULL)
  {
    return false;
  }

  uint32_t addr = (uint32_t)ptr;

  for (uint32_t i = 0; i < max_len; i++)
  {
    uint32_t current = addr + i;

    if (current < USER_ADDR_MIN || current >= KERNEL_USER_BOUNDARY)
    {
      return false;
    }

    char c = *((volatile char *)current);
    if (c == '\0')
    {
      return true;
    }
  }

  return false;
}

static bool normalize_user_path(const char *user_path, char *out,
                                size_t out_size)
{
  if (!is_valid_user_cstr(user_path, 256))
  {
    return false;
  }

  char cwd[256];
  strncpy(cwd, fat16_get_current_path(), sizeof(cwd) - 1);
  cwd[sizeof(cwd) - 1] = '\0';

  return resolve_path(cwd, user_path, out, out_size);
}

static void write_to_stdout(const char *data, uint32_t len)
{
  vfs_node_t *stdout_node = vfs_find("stdout");
  if (stdout_node)
  {
    vfs_write(stdout_node, 0, len, (uint8_t *)data);
  }
}

static void sys_dir_visitor_callback(fat16_dir_entry_t *entry)
{
  if (entry->attributes == 0x0F || entry->filename[0] == 0xE5)
  {
    return;
  }

  char name[13];
  int pos = 0;

  for (int i = 0; i < 8; i++)
  {
    char c = entry->filename[i];
    if (c == ' ')
      break;
    name[pos++] = c;
  }

  bool has_ext = false;
  for (int i = 8; i < 11; i++)
  {
    if (entry->filename[i] != ' ')
    {
      has_ext = true;
      break;
    }
  }

  if (has_ext)
  {
    name[pos++] = '.';
    for (int i = 8; i < 11; i++)
    {
      char c = entry->filename[i];
      if (c == ' ')
        break;
      name[pos++] = c;
    }
  }

  if (entry->attributes & 0x10)
  {
    name[pos++] = '/';
  }
  name[pos] = '\0';

  char line[128];
  int lp = 0;

  int name_len = strlen(name);
  for (int i = 0; i < name_len; i++)
  {
    line[lp++] = name[i];
  }
  while (lp < 24)
  {
    line[lp++] = ' ';
  }

  char size_buf[32];
  itoa((int)entry->file_size, size_buf, 10);
  int size_len = strlen(size_buf);

  for (int i = 0; i < 8 - size_len; i++)
  {
    line[lp++] = ' ';
  }
  for (int i = 0; i < size_len; i++)
  {
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

void syscall_dispatcher(syscall_regs_t *regs)
{
  uint32_t syscall_number = regs->eax;

  switch (syscall_number)
  {
  case SYS_YIELD:
  {
    task_request_yield();
    regs->eax = 0;
    break;
  }

  case SYS_RETURN:
  {
    int exit_code = (int)regs->ebx;
    task_exit_with_status(exit_code);
    regs->eax = 0;
    break;
  }

  case SYS_OPEN:
  {
    const char *user_filename = (const char *)regs->ebx;
    char filename[256];

    if (!normalize_user_path(user_filename, filename, sizeof(filename)))
    {
      regs->eax = -1;
      break;
    }

    task_t *current = task_get_current();

    vfs_node_t *target_node = vfs_open(filename);
    if (!target_node)
    {
      target_node = fat16_vfs_open(filename);
    }

    if (!target_node)
    {
      regs->eax = -1;
      break;
    }

    if (!vfs_check_access(target_node, current, VFS_ACCESS_READ))
    {
      vfs_close(target_node);
      regs->eax = -1;
      break;
    }

    if (vfs_is_device(target_node) && !task_has_cap(current, CAP_DEV_OPEN))
    {
      vfs_close(target_node);
      regs->eax = -1;
      break;
    }

    int free_fd = -1;
    for (int i = 0; i < MAX_OPEN_FILES; i++)
    {
      if (!current->fd_table[i].in_use)
      {
        free_fd = i;
        break;
      }
    }

    if (free_fd == -1)
    {
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

  case SYS_CLOSE:
  {
    int fd = (int)regs->ebx;
    task_t *current = task_get_current();

    if (fd < 0 || fd >= MAX_OPEN_FILES || !current->fd_table[fd].in_use)
    {
      regs->eax = -1;
      break;
    }

    vfs_node_t *node = current->fd_table[fd].node;
    if (node != NULL)
    {
      vfs_close(node);
    }

    current->fd_table[fd].in_use = false;
    current->fd_table[fd].node = NULL;
    memset(current->fd_table[fd].filename, 0,
           sizeof(current->fd_table[fd].filename));
    current->fd_table[fd].current_offset = 0;
    current->fd_table[fd].file_size = 0;
    regs->eax = 0;
    break;
  }

  case SYS_READ:
  {
    int fd = (int)regs->ebx;
    uint8_t *buffer = (uint8_t *)regs->ecx;
    uint32_t bytes_to_read = regs->edx;

    if (!is_valid_user_ptr(buffer, bytes_to_read))
    {
      regs->eax = -1;
      break;
    }

    task_t *current = task_get_current();
    if (fd < 0 || fd >= MAX_OPEN_FILES || !current->fd_table[fd].in_use)
    {
      regs->eax = -1;
      break;
    }

    vfs_node_t *node = current->fd_table[fd].node;
    uint32_t off = current->fd_table[fd].current_offset;

    if (node->type == VFS_FILE && off >= node->length)
    {
      regs->eax = 0;
      break;
    }

    uint32_t bytes_read = vfs_read(node, off, bytes_to_read, buffer);
    current->fd_table[fd].current_offset += bytes_read;

    if (fd == 0 && bytes_read > 0)
    {
      write_to_stdout((const char *)buffer, bytes_read);
    }

    regs->eax = bytes_read;
    break;
  }

  case SYS_WRITE:
  {
    int fd = (int)regs->ebx;
    uint8_t *buffer = (uint8_t *)regs->ecx;
    uint32_t bytes_to_write = regs->edx;

    if (!is_valid_user_ptr(buffer, bytes_to_write))
    {
      regs->eax = -1;
      break;
    }

    task_t *current = task_get_current();
    if (fd < 0 || fd >= MAX_OPEN_FILES || !current->fd_table[fd].in_use)
    {
      regs->eax = -1;
      break;
    }

    vfs_node_t *node = current->fd_table[fd].node;

    if (!vfs_check_access(node, current, VFS_ACCESS_WRITE))
    {
      regs->eax = -1;
      break;
    }

    uint32_t off = current->fd_table[fd].current_offset;
    uint32_t bytes_written = vfs_write(node, off, bytes_to_write, buffer);
    current->fd_table[fd].current_offset += bytes_written;
    regs->eax = bytes_written;
    break;
  }

  case SYS_FORMAT:
  {
    if (!task_has_cap(task_get_current(), CAP_FS_FORMAT))
    {
      regs->eax = -1;
      break;
    }
    fat16_format_drive(0x80, 0, NULL, true);
    regs->eax = 0;
    break;
  }

  case SYS_LIST_DIR:
  {
    const char *user_path = (const char *)regs->ebx;
    char path[256];

    if (!normalize_user_path(user_path, path, sizeof(path)))
    {
      regs->eax = -1;
      break;
    }

    char saved_path[256];
    strcpy(saved_path, fat16_get_current_path());
    if (fat16_chdir(path) != 0)
    {
      regs->eax = -1;
      break;
    }

    fat16_list(sys_dir_visitor_callback);
    fat16_chdir(saved_path);

    regs->eax = 0;
    break;
  }

  case SYS_MKDIR:
  {
    const char *user_path = (const char *)regs->ebx;
    char path[256];

    if (!normalize_user_path(user_path, path, sizeof(path)))
    {
      regs->eax = -1;
      break;
    }

    if (!task_has_cap(task_get_current(), CAP_FS_WRITE))
    {
      regs->eax = -1;
      break;
    }

    regs->eax = fat16_create_dir(path);
    break;
  }

  case SYS_RMDIR:
  {
    const char *user_path = (const char *)regs->ebx;
    char path[256];

    if (!normalize_user_path(user_path, path, sizeof(path)))
    {
      regs->eax = -1;
      break;
    }

    if (!task_has_cap(task_get_current(), CAP_FS_WRITE))
    {
      regs->eax = -1;
      break;
    }

    fat16_delete_dir(path);
    regs->eax = 0;
    break;
  }

  case SYS_REMOVE:
  {
    const char *user_path = (const char *)regs->ebx;
    char path[256];

    if (!normalize_user_path(user_path, path, sizeof(path)))
    {
      regs->eax = -1;
      break;
    }

    if (!task_has_cap(task_get_current(), CAP_FS_WRITE))
    {
      regs->eax = -1;
      break;
    }

    fat16_delete_file(path);
    regs->eax = 0;
    break;
  }

  case SYS_CREATE:
  {
    const char *user_path = (const char *)regs->ebx;
    char path[256];

    if (!normalize_user_path(user_path, path, sizeof(path)))
    {
      regs->eax = -1;
      break;
    }

    if (!task_has_cap(task_get_current(), CAP_FS_WRITE))
    {
      regs->eax = -1;
      break;
    }

    fat16_write_file(path, (uint8_t *)" ", 1);

    task_t *current = task_get_current();
    vfs_node_t *target_node = fat16_vfs_open(path);
    if (!target_node)
    {
      regs->eax = -1;
      break;
    }

    int free_fd = -1;
    for (int i = 0; i < MAX_OPEN_FILES; i++)
    {
      if (current->fd_table[i].in_use == false)
      {
        free_fd = i;
        break;
      }
    }

    if (free_fd == -1)
    {
      vfs_close(target_node);
      regs->eax = -1;
      break;
    }

    current->fd_table[free_fd].in_use = true;
    strncpy(current->fd_table[free_fd].filename, path,
            sizeof(current->fd_table[free_fd].filename) - 1);
    current->fd_table[free_fd].filename[sizeof(current->fd_table[free_fd].filename) - 1] = '\0';
    current->fd_table[free_fd].file_size = target_node->length;
    current->fd_table[free_fd].current_offset = 0;
    current->fd_table[free_fd].node = target_node;
    regs->eax = free_fd;
    break;
  }

  case SYS_UPTIME:
  {
    regs->eax = get_ticks();
    break;
  }

  case SYS_ALLOC_PAGE:
  {
    task_t *current = task_get_current();
    if (!current)
    {
      regs->eax = 0;
      break;
    }

    uint32_t page_dir_phys = current->page_directory;
    void *phys = pmm_alloc_block();
    if (!phys)
    {
      regs->eax = 0;
      break;
    }

    uint32_t virt = current->next_user_vaddr;
    if (virt < USER_VIRT_MIN || virt >= USER_STACK_TOP)
    {
      pmm_free_block(phys);
      regs->eax = 0;
      break;
    }

    current->next_user_vaddr += PAGE_SIZE;
    if (current->next_user_vaddr >= USER_STACK_TOP)
    {
      current->next_user_vaddr = USER_STACK_TOP;
    }

    vmm_map_page_in_directory(page_dir_phys, phys, (void *)virt,
                              PAGE_PRESENT | PAGE_WRITE | PAGE_USER);
    regs->eax = virt;
    break;
  }

  case SYS_FREE_PAGE:
  {
    task_t *current = task_get_current();
    void *virt = (void *)regs->ebx;

    if (!current || virt == NULL || (uint32_t)virt < USER_VIRT_MIN ||
        (uint32_t)virt >= KERNEL_VIRT_START || ((uint32_t)virt & 0xFFF) != 0)
    {
      regs->eax = -1;
      break;
    }

    uint32_t virt_addr = (uint32_t)virt;
    uint32_t pd_index = virt_addr >> 22;
    uint32_t pt_index = (virt_addr >> 12) & 0x3FF;
    uint32_t *page_directory = (uint32_t *)current->page_directory;

    if (!(page_directory[pd_index] & PAGE_PRESENT))
    {
      regs->eax = -1;
      break;
    }

    uint32_t *page_table = (uint32_t *)(page_directory[pd_index] & ~0xFFF);
    uint32_t pte = page_table[pt_index];
    if (!(pte & PAGE_PRESENT))
    {
      regs->eax = -1;
      break;
    }

    pmm_free_block((void *)(pte & ~0xFFF));
    page_table[pt_index] = 0;
    regs->eax = 0;
    break;
  }

  case SYS_CHDIR:
  {
    const char *user_path = (const char *)regs->ebx;
    char path[256];

    if (!normalize_user_path(user_path, path, sizeof(path)))
    {
      regs->eax = -1;
      break;
    }

    regs->eax = fat16_chdir(path);
    break;
  }

  case SYS_COPY_FILE:
  {
    const char *src_user = (const char *)regs->ebx;
    const char *dst_user = (const char *)regs->ecx;
    char src[256];
    char dst[256];

    if (!normalize_user_path(src_user, src, sizeof(src)) ||
        !normalize_user_path(dst_user, dst, sizeof(dst)))
    {
      regs->eax = -1;
      break;
    }

    if (!task_has_cap(task_get_current(), CAP_FS_WRITE))
    {
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