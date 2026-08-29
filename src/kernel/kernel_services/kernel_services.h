#ifndef KERNEL_SERVICES_H
#define KERNEL_SERVICES_H

#include <stddef.h>
#include <stdint.h>

void ksleep(uint32_t ms);
void kprintf(const char *fmt, ...);
void kpanic(const char *message);

void *kmem_alloc(size_t size);
void *kmem_zalloc(size_t size);
void kmem_free(void *ptr);

void kdisk_write_sector(uint32_t lba, uint8_t *buffer);
void kdisk_read_sector(uint32_t lba, uint8_t *buffer);

void kplay_sound_file(const char *path);
void ksound_notify(void);
void ksound_error(void);
void ksound_boot(void);

#endif