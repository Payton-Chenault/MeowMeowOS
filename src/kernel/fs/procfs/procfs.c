#include "procfs.h"
#include "../../kernel_services/kernel_services.h"
#include "../../lib/string/string.h"
#include "../../arch/x86/pit/pit.h"
#include "../../arch/x86/task/task.h"
#include "../../mem/physical_memory_manager/pmm.h"
#include "../../lib/integer_ascii_converters/itoa.h"
#include "../../utils/logging/logger.h"

#define MODULE "PROCFS"

static uint32_t procfs_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    log_trace(MODULE, "procfs_read: node='%s', offset=%u, size=%u", node->name, offset, size);
    if (offset >= node->length) return 0;
    uint32_t available = node->length - offset;
    uint32_t to_copy = (size < available) ? size : available;
    memcpy(buffer, (uint8_t *)node->ptr + offset, to_copy);
    return to_copy;
}

static void procfs_close(vfs_node_t *node) {
    log_debug(MODULE, "procfs_close: releasing dynamic buffer for node '%s'", node->name);
    if (node->ptr) kmem_free(node->ptr);
    kmem_free(node);
}

static void append_num_padded(char *buf, int *pos, int max, int num, int width) {
    char tmp[16];
    itoa(num, tmp, 10);
    int len = strlen(tmp);
    if (*pos < max - 1) {
        strcpy(buf + *pos, tmp);
        *pos += len;
        for (int i = 0; i < width - len && *pos < max - 1; i++) buf[(*pos)++] = ' ';
    }
}

static void append_unum_padded(char *buf, int *pos, int max, uint32_t num, int width) {
    char tmp[16];
    utoa(num, tmp, 10);
    int len = strlen(tmp);
    if (*pos < max - 1) {
        strcpy(buf + *pos, tmp);
        *pos += len;
        for (int i = 0; i < width - len && *pos < max - 1; i++) buf[(*pos)++] = ' ';
    }
}

static void append_str_padded(char *buf, int *pos, int max, const char *str, int width) {
    int len = strlen(str);
    if (*pos < max - 1) {
        strcpy(buf + *pos, str);
        *pos += len;
        for (int i = 0; i < width - len && *pos < max - 1; i++) buf[(*pos)++] = ' ';
    }
}

static void append_str(char *buf, int *pos, int max, const char *str) {
    int len = strlen(str);
    if (*pos + len < max - 1) {
        strcpy(buf + *pos, str);
        *pos += len;
    }
}

static vfs_node_t *procfs_open(const char *path) {
    log_trace(MODULE, "procfs_open: checking path='%s'", path);
    const char *name = path;
    if (strncmp(path, "/proc/", 6) == 0) name += 6;

    vfs_node_t *node = kmem_zalloc(sizeof(vfs_node_t));
    if (!node) {
        log_error(MODULE, "procfs_open: failed to allocate memory for node");
        return NULL;
    }
    
    node->type = VFS_FILE;
    node->read = procfs_read;
    node->close = procfs_close;
    node->persistent = false;
    node->ref_count = 1;
    strcpy(node->name, name);

    if (strcmp(name, "meminfo") == 0) {
        log_debug(MODULE, "procfs_open: generating /proc/meminfo snapshot");
        uint32_t total, used, free_mem;
        pmm_get_memory_info(&total, &used, &free_mem);
        char *buf = kmem_zalloc(512);
        int pos = 0;
        append_str(buf, &pos, 512, "total(KB)   used(KB)    free(KB)\n");
        append_num_padded(buf, &pos, 512, total / 1024, 12);
        append_num_padded(buf, &pos, 512, used / 1024, 12);
        append_num_padded(buf, &pos, 512, free_mem / 1024, 12);
        append_str(buf, &pos, 512, "\n");
        node->ptr = buf;
        node->length = pos;
        return node;
    } else if (strcmp(name, "uptime") == 0) {
        log_debug(MODULE, "procfs_open: generating /proc/uptime snapshot");
        char *buf = kmem_zalloc(64);
        int pos = 0;
        append_unum_padded(buf, &pos, 64, get_ticks(), 0);
        append_str(buf, &pos, 64, "\n");
        node->ptr = buf;
        node->length = pos;
        return node;
    } else if (strcmp(name, "tasks") == 0) {
        log_debug(MODULE, "procfs_open: generating /proc/tasks snapshot");
        sys_process_info_t *procs = kmem_zalloc(sizeof(sys_process_info_t) * 32);
        int count = task_get_process_info(procs, 32);
        char *buf = kmem_zalloc(4096);
        int pos = 0;
        append_str(buf, &pos, 4096, "PID   PPID  PRIO  DYN   STATE   TICKS      COMMAND\n");
        append_str(buf, &pos, 4096, "--------------------------------------------------\n");
        const char *state_names[] = {"READY", "RUN", "WAIT", "BLOCK", "SLEEP", "DEAD"};
        for (int i = 0; i < count; i++) {
            const char *state_str = "UNKNW";
            if (procs[i].state <= 5) state_str = state_names[procs[i].state];
            append_num_padded(buf, &pos, 4096, procs[i].pid, 6);
            append_num_padded(buf, &pos, 4096, procs[i].parent_pid, 6);
            append_num_padded(buf, &pos, 4096, procs[i].base_priority, 6);
            append_num_padded(buf, &pos, 4096, procs[i].dynamic_priority, 6);
            append_str_padded(buf, &pos, 4096, state_str, 8);
            append_num_padded(buf, &pos, 4096, procs[i].cpu_ticks, 11);
            append_str(buf, &pos, 4096, procs[i].name);
            append_str(buf, &pos, 4096, "\n");
        }
        append_str(buf, &pos, 4096, "--------------------------------------------------\n");
        append_str(buf, &pos, 4096, "Total tasks: ");
        append_num_padded(buf, &pos, 4096, count, 0);
        append_str(buf, &pos, 4096, "\n");
        kmem_free(procs);
        node->ptr = buf;
        node->length = pos;
        return node;
    }

    log_trace(MODULE, "procfs_open: unrecognized node '%s'", name);
    kmem_free(node);
    return NULL;
}

static fs_driver_t procfs_driver;

void procfs_initialize(void) {
    log_debug(MODULE, "Initializing ProcFS driver and dynamic handlers");
    memset(&procfs_driver, 0, sizeof(fs_driver_t));
    strcpy(procfs_driver.name, "procfs");
    procfs_driver.open = procfs_open;
    vfs_register_driver(&procfs_driver);
    vfs_mount("/proc", "procfs");
    log_info(MODULE, "Initialized ProcFS with /proc/meminfo, /proc/uptime, /proc/tasks");
}