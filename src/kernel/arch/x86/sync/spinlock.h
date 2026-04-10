#ifndef SPINLOCK_H
#define SPINLOCK_H

#include <stdint.h>
#include "../interrupt_descriptor_table/idt.h"

typedef struct {
    volatile uint32_t locked;
} spinlock_t;

#define SPINLOCK_INIT {0}

static inline void spinlock_acquire(spinlock_t* lock) {
    while (__sync_lock_test_and_set(&lock->locked, 1)) {
        __asm__ volatile ("pause");
    }
}

static inline void spinlock_release(spinlock_t* lock) {
    __sync_lock_release(&lock->locked);
}

static inline void spinlock_acquire_irq(spinlock_t* lock) {
    disable_interrupts();
    spinlock_acquire(lock);
}

static inline void spinlock_release_irq(spinlock_t* lock) {
    spinlock_release(lock);
    enable_interrupts();
}

#endif