#ifndef KERNEL_SERVICES_H
#define KERNEL_SERVICES_H

#include <stdint.h>
#include <stddef.h>

void ksleep(uint32_t ms);
void kprintf(const char* fmt, ...);
void kpanic(const char* message);
void* kmem_alloc(size_t size);
void kmem_free(void* ptr);

#endif