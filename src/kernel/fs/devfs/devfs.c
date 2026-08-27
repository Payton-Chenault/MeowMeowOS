#include "devfs.h"
#include "../../kernel_services/kernel_services.h"
#include "../../lib/string/string.h"
#include "../../arch/x86/pit/pit.h"
#include "../../utils/logging/logger.h"

#define MODULE "DEVFS"

typedef struct devfs_wrapper {
    vfs_node_t *node;
    struct devfs_wrapper *next;
} devfs_wrapper_t;

static devfs_wrapper_t *devfs_nodes = NULL;

void devfs_register_node(vfs_node_t *node) {
    if (!node) return;
    log_info(MODULE, "Registering device node '/dev/%s'", node->name);
    devfs_wrapper_t *w = kmem_zalloc(sizeof(devfs_wrapper_t));
    w->node = node;
    w->next = devfs_nodes;
    devfs_nodes = w;
}

static uint32_t null_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset; (void)size; (void)buffer;
    log_trace(MODULE, "null_read: returning EOF (0 bytes)");
    return 0; // EOF
}

static uint32_t null_write(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset; (void)buffer;
    log_trace(MODULE, "null_write: discarding %u bytes", size);
    return size; // Discard data
}

static uint32_t zero_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset;
    log_trace(MODULE, "zero_read: generating %u zeroed bytes", size);
    memset(buffer, 0, size);
    return size;
}

static uint32_t zero_write(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset; (void)buffer;
    log_trace(MODULE, "zero_write: sinking %u bytes", size);
    return size;
}

static uint32_t random_read(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset;
    log_trace(MODULE, "random_read: generating %u random bytes", size);
    static uint32_t seed = 0;
    if (seed == 0) {
        seed = get_ticks() ^ 0xDEADBEEF;
        log_debug(MODULE, "random_read: initialized PRNG seed to 0x%X", seed);
    }
    
    for (uint32_t i = 0; i < size; i++) {
        seed = seed * 1103515245 + 12345;
        buffer[i] = (seed >> 16) & 0xFF;
    }
    return size;
}

static uint32_t random_write(vfs_node_t *node, uint32_t offset, uint32_t size, uint8_t *buffer) {
    (void)node; (void)offset; (void)buffer;
    log_debug(MODULE, "random_write: updating entropy with %u bytes", size);
    return size;
}

static vfs_node_t *devfs_open(const char *path) {
    log_trace(MODULE, "devfs_open: requested path='%s'", path);
    const char *name = path;
    if (strncmp(path, "/dev/", 5) == 0) name += 5;
    
    devfs_wrapper_t *curr = devfs_nodes;
    while (curr) {
        if (strcmp(curr->node->name, name) == 0) {
            log_debug(MODULE, "devfs_open: successfully matched device '%s'", curr->node->name);
            return vfs_retain(curr->node);
        }
        curr = curr->next;
    }
    log_trace(MODULE, "devfs_open: device not found for '%s'", path);
    return NULL;
}

static fs_driver_t devfs_driver;

void devfs_initialize(void) {
    log_debug(MODULE, "Initializing DevFS driver and endpoints");
    memset(&devfs_driver, 0, sizeof(fs_driver_t));
    strcpy(devfs_driver.name, "devfs");
    devfs_driver.open = devfs_open;
    vfs_register_driver(&devfs_driver);
    vfs_mount("/dev", "devfs");

    vfs_node_t *n_null = kmem_zalloc(sizeof(vfs_node_t));
    strcpy(n_null->name, "null");
    n_null->type = VFS_DEVICE;
    n_null->read = null_read;
    n_null->write = null_write;
    n_null->persistent = true;
    devfs_register_node(n_null);

    vfs_node_t *n_zero = kmem_zalloc(sizeof(vfs_node_t));
    strcpy(n_zero->name, "zero");
    n_zero->type = VFS_DEVICE;
    n_zero->read = zero_read;
    n_zero->write = zero_write;
    n_zero->persistent = true;
    devfs_register_node(n_zero);

    vfs_node_t *n_random = kmem_zalloc(sizeof(vfs_node_t));
    strcpy(n_random->name, "random");
    n_random->type = VFS_DEVICE;
    n_random->read = random_read;
    n_random->write = random_write;
    n_random->persistent = true;
    devfs_register_node(n_random);

    log_info(MODULE, "Initialized DevFS with /dev/null, /dev/zero, /dev/random");
}