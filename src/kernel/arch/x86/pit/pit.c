#include "pit.h"
#include <stdint.h>

#include "../../../drivers/ports/IO.h"
#include "../interrupt_descriptor_table/idt.h"
#include "../../../utils/logging/logger.h"
#include "../../../task/task.h"

#define MODULE "PIT"

static volatile uint32_t system_ticks = 0;
static uint32_t timer_frequency;

bool pit_handle_interrupt(void) {
    system_ticks++;

    outb(0x20, 0x20);
    task_yield();
    // Return false because a timer tick is not a fatal panic
    return false; 
}

void pit_initialize(uint32_t frequency) {
    timer_frequency = frequency;
    uint32_t divisor = 1193182 / frequency;

    outb(PIT_COMMAND_PORT, 0x36);

    outb(PIT_CHANNEL0_PORT, (uint8_t)(divisor & 0xFF));
    outb(PIT_CHANNEL0_PORT, (uint8_t)((divisor >> 8) & 0xFF));

    register_interrupt_handler(TIMER_INTERRUPT_VECTOR, pit_handle_interrupt);
    log_info(MODULE, "Initialized");
}

uint32_t get_ticks() {
    return system_ticks;
}

uint32_t get_system_freq() {
    return timer_frequency;
}