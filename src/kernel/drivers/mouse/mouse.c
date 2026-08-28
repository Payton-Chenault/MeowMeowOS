#include "mouse.h"
#include "../ports/IO.h"
#include "../../utils/logging/logger.h"
#include "../../utils/console_print/kconsole.h"
#include "../../arch/x86/interrupt_descriptor_table/idt.h"
#include "../../arch/x86/sync/spinlock.h"
#include "../../kernel_services/kernel_services.h"

#define MODULE "MOUSE"

#define MOUSE_PORT_DATA    0x60
#define MOUSE_PORT_STATUS  0x64
#define MOUSE_PORT_CMD     0x64

static uint8_t mouse_cycle = 0;
static uint8_t mouse_packet[3];

static int32_t mouse_x = 512;
static int32_t mouse_y = 384;
static uint8_t mouse_btn_state = 0;

static spinlock_t mouse_lock = SPINLOCK_INIT;

static inline void mouse_wait_write(void) {
    uint32_t timeout = 100000;
    while ((inb(MOUSE_PORT_STATUS) & 0x02) && --timeout) {
        __asm__ volatile("pause");
    }
    if (timeout == 0) {
        log_warning(MODULE, "Timeout waiting for PS/2 write buffer clear");
    }
}

static inline void mouse_wait_read(void) {
    uint32_t timeout = 100000;
    while (!(inb(MOUSE_PORT_STATUS) & 0x01) && --timeout) {
        __asm__ volatile("pause");
    }
    if (timeout == 0) {
        log_warning(MODULE, "Timeout waiting for PS/2 read buffer ready");
    }
}

static void mouse_write(uint8_t data) {
    mouse_wait_write();
    outb(MOUSE_PORT_CMD, 0xD4); // Tell controller next byte is for auxiliary device
    mouse_wait_write();
    outb(MOUSE_PORT_DATA, data);
    log_trace(MODULE, "Sent byte 0x%X to auxiliary device", data);
}

static uint8_t mouse_read(void) {
    mouse_wait_read();
    uint8_t val = inb(MOUSE_PORT_DATA);
    log_trace(MODULE, "Read byte 0x%X from auxiliary device", val);
    return val;
}

bool mouse_isr(void) {
    uint8_t status = inb(MOUSE_PORT_STATUS);
    if (!(status & 0x20)) {
        // Output buffer data is not from auxiliary PS/2 device
        return false;
    }

    uint8_t data = inb(MOUSE_PORT_DATA);

    switch (mouse_cycle) {
    case 0:
        // Packet byte 0: Bit 3 must always be 1 in standard PS/2 packets
        if (!(data & 0x08)) {
            log_trace(MODULE, "Discarded desynchronized mouse byte: 0x%X", data);
            return false;
        }
        mouse_packet[0] = data;
        mouse_cycle = 1;
        break;

    case 1:
        mouse_packet[1] = data;
        mouse_cycle = 2;
        break;

    case 2: {
        mouse_packet[2] = data;
        mouse_cycle = 0;

        uint8_t flags = mouse_packet[0];
        int32_t delta_x = (int32_t)mouse_packet[1];
        int32_t delta_y = (int32_t)mouse_packet[2];

        // Sign extensions for 9-bit relative movements
        if (flags & 0x10) {
            delta_x |= 0xFFFFFF00;
        }
        if (flags & 0x20) {
            delta_y |= 0xFFFFFF00;
        }

        // Discard packet on buffer overflow condition
        if (flags & 0xC0) {
            log_trace(MODULE, "Packet dropped due to X/Y overflow flag (0x%X)", flags);
            break;
        }

        uint32_t irq_flags = spinlock_acquire_irq_save(&mouse_lock);

        mouse_x += delta_x;
        // PS/2 Y delta is positive upwards; screen Y is positive downwards
        mouse_y -= delta_y;

        uint32_t max_width = kconsole_get_width();
        uint32_t max_height = kconsole_get_height();

        if (max_width == 0) max_width = 1024;
        if (max_height == 0) max_height = 768;

        if (mouse_x < 0) mouse_x = 0;
        if (mouse_x >= (int32_t)max_width) mouse_x = max_width - 1;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_y >= (int32_t)max_height) mouse_y = max_height - 1;

        mouse_btn_state = flags & 0x07;

        kconsole_draw_mouse_cursor(mouse_x, mouse_y);

        spinlock_release_irq_restore(&mouse_lock, irq_flags);

        log_trace(MODULE, "Mouse moved: dx=%d, dy=%d -> Pos=(%d, %d), Buttons=0x%X",
                  delta_x, delta_y, mouse_x, mouse_y, mouse_btn_state);
        break;
    }
    }
    return false;
}

void mouse_get_state(mouse_state_t *state) {
    if (!state) return;
    uint32_t flags = spinlock_acquire_irq_save(&mouse_lock);
    state->x = mouse_x;
    state->y = mouse_y;
    state->buttons = mouse_btn_state;
    spinlock_release_irq_restore(&mouse_lock, flags);
}

void mouse_initialize(void) {
    log_info(MODULE, "Initializing Dual-Channel PS/2 Controller Auxiliary Device...");

    // 1. Enable auxiliary PS/2 device
    mouse_wait_write();
    outb(MOUSE_PORT_CMD, 0xA8);

    // 2. Read current controller command byte
    mouse_wait_write();
    outb(MOUSE_PORT_CMD, 0x20);
    uint8_t status = mouse_read();

    // Enable IRQ 12 (Bit 1) and enable clock line (Clear bit 5)
    status |= 0x02;
    status &= ~0x20;

    // 3. Write updated command byte back to controller
    mouse_wait_write();
    outb(MOUSE_PORT_CMD, 0x60);
    mouse_wait_write();
    outb(MOUSE_PORT_DATA, status);

    // 4. Reset to default configuration
    mouse_write(0xF6);
    mouse_read(); // Acknowledge 0xFA

    // 5. Enable data packet streaming
    mouse_write(0xF4);
    mouse_read(); // Acknowledge 0xFA

    // Position mouse initially at screen center
    mouse_x = kconsole_get_width() > 0 ? kconsole_get_width() / 2 : 512;
    mouse_y = kconsole_get_height() > 0 ? kconsole_get_height() / 2 : 384;
    mouse_cycle = 0;
    mouse_btn_state = 0;

    // Hook IRQ 12 vector in IDT and unmask slave PIC line
    uint8_t vector = IRQ_TO_VECTOR(12);
    register_interrupt_handler(vector, mouse_isr);
    pic_unmask(12);

    kconsole_draw_mouse_cursor(mouse_x, mouse_y);
    log_info(MODULE, "PS/2 Mouse Driver Activated (Initial Pos: %d, %d, Vector: 0x%X)", mouse_x, mouse_y, vector);
}