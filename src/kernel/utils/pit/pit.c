#include "pit.h"
#include <stdint.h>

#define MODULE "PIT"

static volatile uint32_t system_ticks = 0;
static uint32_t timer_frequency;

bool pit_handle_interrupt(void) {
    system_ticks++;
    // Return false because a timer tick is not a fatal panic
    return false; 
}

void pit_initialize(uint32_t frequency) {
    timer_frequency = frequency;
    uint32_t divisor = 1193182 / frequency;

    outb(PIT_COMMAND_PORT, 0x36);

    outb(PIT_CHANNEL0_PORT, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0_PORT, (uint8_t)((divisor >> 8) & 0xFF));

    register_interrupt_handler(TIMER_INTERUPT_VECTOR, pit_handle_interrupt);
    log_debug(MODULE, "PIT Initialized");
}

uint32_t get_ticks() {
    return system_ticks;
}

void sleep(uint32_t ms) {
    uint32_t wait_ticks = (ms * timer_frequency) / 1000;
    uint32_t target_ticks = system_ticks + wait_ticks;

    while (system_ticks < target_ticks) {
        __asm__ volatile("hlt");
    }
}