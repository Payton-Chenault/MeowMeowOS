#include "heap.h"
#include <stdint.h>
#include "../../lib/string/string.h"
#include "../../mem/virtual_memory_manager/vmm.h"
#include "../../mem/physical_memory_manager/pmm.h"
#include "../../utils/logging/logger.h"


#define MODULE "HEAP"

static heap_block_t* heap_start = NULL;
static uint32_t heap_current_end = 0;

void heap_initialize(uint32_t start_addr, uint32_t size) {
    heap_start = (heap_block_t*)start_addr;
    heap_current_end = start_addr + size;

    heap_start->magic = HEAP_MAGIC;
    heap_start->size = size - sizeof(heap_block_t);
    heap_start->is_free = 1;
    heap_start->next = NULL;

    log_info(MODULE, "Initialized at %x (Size: %d KB)", start_addr, size / 1024);
}

void heap_expand(uint32_t additional_size) {
    uint32_t pages_needed = (additional_size + PAGE_SIZE - 1) / PAGE_SIZE;
    uint32_t total_expansion_size = pages_needed * PAGE_SIZE;
    uint32_t expansion_start_addr = heap_current_end;

    log_warning(MODULE, "OK: Expanding heap by %d pages", pages_needed);

    for (uint32_t i = 0; i < pages_needed; i++) {
        void* phys = pmm_alloc_block();
        vmm_map_page(phys, (void*)(heap_current_end), PAGE_PRESENT | PAGE_WRITE);
        heap_current_end += PAGE_SIZE;
    }

    heap_block_t* new_big_block = (heap_block_t*)expansion_start_addr;
    new_big_block->magic = HEAP_MAGIC;
    new_big_block->size = total_expansion_size - sizeof(heap_block_t);
    new_big_block->is_free = 1;
    new_big_block->next = NULL;

    heap_block_t* current = heap_start;
    while (current->next) {
        current = current->next;
    }
    current->next = new_big_block;

    log_warning(MODULE, "OK: Heap expanded to %d KB", (heap_current_end - (uint32_t)heap_start) / 1024);
}

void* mem_alloc(size_t size) {

    size_t formed_size = (size + 3) & ~3;
    heap_block_t* current = heap_start;
    heap_block_t* last = NULL;

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

        last = current;
        current = current->next;
    }

    log_warning(MODULE, "OK: Requesting heap expansion for %d bytes", size);
    heap_expand(size + sizeof(heap_block_t));

    return mem_alloc(size);
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
        log_error(MODULE, "FAILED: KFREE Attempted to free invalid or corrupted memory at %x", ptr);
        return;
    }

    if(block->is_free) {
        return;
    }

    block->is_free = 1;

    if(block->next && block->next->is_free) {
        log_debug(MODULE, "OK: Merging blocks at %x and %x", block, block->next);

        block->size += sizeof(heap_block_t) + block->next->size;

        block->next = block->next->next;
    }

    log_debug(MODULE, "OK: Freed memory at %x", ptr);
}