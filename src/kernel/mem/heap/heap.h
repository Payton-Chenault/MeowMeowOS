#ifndef HEAP_H
#define HEAP_H

#include <stddef.h>
#include <stdint.h>

#define HEAP_MAGIC 0x12345678

typedef struct heap_block {
  uint32_t magic;
  uint32_t size;
  uint8_t is_free;
  struct heap_block *next;
  struct heap_block *prev;
} heap_block_t;

void heap_initialize(uint32_t start_addr, uint32_t size);
void *mem_alloc(size_t size);
void *mem_zalloc(size_t size);
void mem_free(void *ptr);

#endif