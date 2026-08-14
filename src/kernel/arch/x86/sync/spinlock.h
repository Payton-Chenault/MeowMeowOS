#ifndef SPINLOCK_H
#define SPINLOCK_H

#include "../interrupt_descriptor_table/idt.h"
#include <stdint.h>

typedef struct {
  volatile uint32_t locked;
} spinlock_t;

#define SPINLOCK_INIT {0}

static inline void spinlock_acquire(spinlock_t *lock) {
  while (__sync_lock_test_and_set(&lock->locked, 1)) {
    __asm__ volatile("pause");
  }
}

static inline void spinlock_release(spinlock_t *lock) {
  __sync_lock_release(&lock->locked);
}

static inline void spinlock_acquire_irq(spinlock_t *lock) {
  disable_interrupts();
  spinlock_acquire(lock);
}

static inline void spinlock_release_irq(spinlock_t *lock) {
  spinlock_release(lock);
  enable_interrupts();
}

// Inline assembly helpers to read/write CPU flags and control interrupts
static inline uint32_t spinlock_save_flags_and_cli(void) {
  uint32_t flags;
  __asm__ volatile("pushf\n\t"
                   "cli\n\t"
                   "pop %0"
                   : "=r"(flags)
                   :
                   : "memory");
  return flags;
}

static inline void spinlock_restore_flags(uint32_t flags) {
  // If bit 9 (IF - Interrupt Flag) was set in the saved flags, re-enable
  // interrupts
  if (flags & (1 << 9)) {
    __asm__ volatile("sti" : : : "memory");
  } else {
    __asm__ volatile("cli" : : : "memory");
  }
}

static inline uint32_t spinlock_acquire_irq_save(spinlock_t *lock) {
  uint32_t flags = spinlock_save_flags_and_cli();

  // Standard atomic spinlock acquisition loop (test-and-set or similar)
  while (__atomic_test_and_set(&(lock->locked), __ATOMIC_ACQUIRE)) {
    // Pause to prevent bus congestion while spinning
    __asm__ volatile("pause" : : : "memory");
  }

  return flags;
}

static inline void spinlock_release_irq_restore(spinlock_t *lock,
                                                uint32_t flags) {
  __atomic_clear(&(lock->locked), __ATOMIC_RELEASE);
  spinlock_restore_flags(flags);
}

#endif