#include "kernel_services.h"
#include "../utils/console_print/kconsole.h"
#include "../arch/x86/pit/pit.h"
#include "../mem/heap/heap.h"
#include "../drivers/disk/ata.h"
#include "../lib/integer_ascii_converters/itoa.h"
#include <stdarg.h>
#include <stddef.h>
#include <stdint.h>
#include "../utils/logging/logger.h"
#include "../fs/vfs/vfs.h"
#include "../lib/string/string.h"


#define MODULE "KERNEL_SERVICES"

#define VFS_PRINT_NODE "stdout"
#define VFS_HDA_NODE "hda"

static vfs_node_t* cached_disk = NULL;
static vfs_node_t* cached_stdio = NULL;

void kprintf(const char* fmt, ...) {
    va_list args;
    va_start(args, fmt);
        if(cached_stdio == NULL) {
        cached_stdio = vfs_find(VFS_PRINT_NODE);
        if (cached_stdio == NULL) {
            log_warning(MODULE, "FAILED: kprintf cannot find vfs node %s, relying on kput_char", VFS_PRINT_NODE);
        }
    }
    
    for (const char* p = fmt; *p != '\0'; p++) {
        if (*p != '%') {
            if (cached_stdio) vfs_write(cached_stdio, 0, 1, (uint8_t*)p);
            else kput_char(*p);
            continue;
        }

        p++;
        switch (*p) {
            case 'c': {
                char c = (char)va_arg(args, int);
                if (cached_stdio) vfs_write(cached_stdio, 0, 1, (uint8_t*)&c);
                else kput_char(c);
                break;
            }
            case 's': {
                char* s = va_arg(args, char*);
                size_t len = strlen(s);
                if (cached_stdio) vfs_write(cached_stdio, 0, len, (uint8_t*)s);
                else while (*s) kput_char(*s++);
                break;
            }
            case 'd':
            case 'x': {
                int i = va_arg(args, int);
                char buffer[32];
                itoa(i, buffer, (*p == 'd') ? 10 : 16);
                size_t len = strlen(buffer);
                if (cached_stdio) vfs_write(cached_stdio, 0, len, (uint8_t*)buffer);
                else for (int j = 0; buffer[j]; j++) kput_char(buffer[j]);
                break;
            }
            case '%': {
                char c = '%';
                if (cached_stdio) vfs_write(cached_stdio, 0, 1, (uint8_t*)&c);
                else kput_char('%');
                break;
            }
            default:
                if (cached_stdio) vfs_write(cached_stdio, 0, 1, (uint8_t*)p);
                else kput_char(*p);
                break;
        }
    }

    va_end(args);
}

void kpanic(const char* msg) {
    kprintf("!!PANIC!!\nThe following might aid in the reason of this panic: [%s]\nPlease Restart System To Reboot...", msg);
    __asm__ volatile("cli");
    __asm__ volatile("hlt");
}

void ksleep(uint32_t ms) {
    volatile uint32_t system_ticks = get_ticks();
    uint32_t timer_frequency = get_system_freq();

    uint32_t wait_ticks = (ms * timer_frequency) / 1000;
    uint32_t target_ticks = system_ticks + wait_ticks;

    while (get_ticks() < target_ticks) {
        __asm__ volatile("hlt");
    }
}

void* kmem_alloc(size_t size) {
    return mem_alloc(size);
}

void* kmem_zalloc(size_t size) {
    return mem_zalloc(size);
}

void kmem_free(void* ptr) {
    mem_free(ptr);
}

void kdisk_read_sector(uint32_t lba, uint8_t *buffer) {
    if(cached_disk == NULL) {
        cached_disk = vfs_find(VFS_HDA_NODE);
        if (cached_disk == NULL) {
            kpanic("No Disk Found From VFS");
        }
    }

    vfs_read(cached_disk, (lba * 512), 512, buffer);
}

void kdisk_write_sector(uint32_t lba, uint8_t *buffer) {
    if(cached_disk == NULL) {
        cached_disk = vfs_find("hda");
        if (cached_disk == NULL) {
            kpanic("No Disk Found From VFS");
        }
    }
    
    vfs_write(cached_disk, (lba * 512), 512, buffer);
}

