#include "heap.h"
#include <stdint.h>
#include "../../lib/string/string.h"
#include "../../utils/logging/logger.h"


#define MODULE "HEAP"

static heap_block_t* heap_start = NULL;

void heap_initialize(uint32_t start_addr, uint32_t size) {
    heap_start = (heap_block_t*)start_addr;

    heap_start->magic = HEAP_MAGIC;
    heap_start->size = size - sizeof(heap_block_t);
    heap_start->is_free = 1;
    heap_start->next = NULL;

    log_info(MODULE, "Initialized at %x (Size: %d KB)", start_addr, size / 1024);
}

void* mem_alloc(size_t size) {

    size_t formed_size = (size + 3) & ~3;

    heap_block_t* current = heap_start;

    while (current) {
        if (current->is_free && current->size >= formed_size) {
            if (current->size > formed_size + sizeof(heap_block_t) + 16) { 
                heap_block_t* new_block = (heap_block_t*)((uint8_t*)current + sizeof(heap_block_t) + formed_size);
                new_block->magic = HEAP_MAGIC;
                new_block->size = current->size - formed_size - sizeof(heap_block_t);
                new_block->is_free = 1;
                new_block->next = current->next;

                current->size = formed_size;
                current->next = new_block;
            }

            current->is_free = 0;
            return (void*)((uint8_t*)current + sizeof(heap_block_t));
        }

        current = current->next;
    }

    log_error(MODULE, "FAILED: OUT OF MEMORY! Could not allocate %d bytes", size);
    return NULL;
}

void* mem_zalloc(size_t size) {
    void* ptr = mem_alloc(size);
    if (ptr) memset(ptr, 0, size);
    return ptr;
}

void mem_free(void* ptr) {
    if(!ptr) return;

    heap_block_t* block = (heap_block_t*)((uint8_t*)ptr - sizeof(heap_block_t));

    if(block->magic != HEAP_MAGIC) {
        log_error(MODULE, "FAILED: KFREE Attempted to free invalid or corrupted memory at 0x%x", ptr);
        return;
    }

    if(block->is_free) {
        return;
    }

    block->is_free = 1;

    if(block->next && block->next->is_free) {
        log_debug(MODULE, "OK: Merging blocks at 0x%x and 0x%x", block, block->next);

        block->size += sizeof(heap_block_t) + block->next->size;

        block->next = block->next->next;
    }

    log_debug(MODULE, "OK: Freed memory at 0x%x", ptr);
}