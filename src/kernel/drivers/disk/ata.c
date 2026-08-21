#include "ata.h"
#include "../../arch/x86/sync/spinlock.h"
#include "../../utils/logging/logger.h"
#include "../ports/IO.h"
#include <stdint.h>

#define MODULE "ATA_DISK"

static spinlock_t ata_lock = SPINLOCK_INIT;

static bool ata_wait_busy_timeout(void) {
    for (volatile uint32_t i = 0; i < 1000000; i++) {
        if (!(inb(ATA_STATUS_PORT) & ATA_SR_BSY)) {
            return true;
        }
    }
    return false;
}

static bool ata_wait_drq_timeout(void) {
    for (volatile uint32_t i = 0; i < 1000000; i++) {
        if (inb(ATA_STATUS_PORT) & ATA_SR_DRQ) {
            return true;
        }
    }
    return false;
}

void ata_read_sector(uint32_t lba, uint8_t *buffer) {
    if (buffer == NULL) {
        return;
    }

    uint32_t flags = spinlock_acquire_irq_save(&ata_lock);

    if (!ata_wait_busy_timeout()) {
        log_error(MODULE, "Timeout waiting for ATA busy clear");
        spinlock_release_irq_restore(&ata_lock, flags);
        return;
    }

    outb(ATA_DRIVE_HEAD_PORT, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_SECTOR_COUNT_PORT, 1);
    outb(ATA_LBA_LOW_PORT, (uint8_t)lba);
    outb(ATA_LBA_MID_PORT, (uint8_t)(lba >> 8));
    outb(ATA_LBA_HIGH_PORT, (uint8_t)(lba >> 16));
    outb(ATA_COMMAND_PORT, ATA_CMD_READ_PIO);

    if (!ata_wait_busy_timeout()) {
        log_error(MODULE, "Timeout waiting for ATA busy clear");
        spinlock_release_irq_restore(&ata_lock, flags);
        return;
    }

    if (!ata_wait_drq_timeout()) {
        log_error(MODULE, "Timeout waiting for ATA DRQ");
        spinlock_release_irq_restore(&ata_lock, flags);
        return;
    }

    uint16_t *ptr = (uint16_t *)buffer;
    for (int i = 0; i < 256; i++) {
        ptr[i] = inw(ATA_DATA_PORT);
    }

    spinlock_release_irq_restore(&ata_lock, flags);
    log_debug(MODULE, "OK: Read From Sector %d", lba);
}

void ata_write_sector(uint32_t lba, uint8_t *buffer) {
    if (buffer == NULL) {
        return;
    }

    uint32_t flags = spinlock_acquire_irq_save(&ata_lock);

    if (!ata_wait_busy_timeout()) {
        log_error(MODULE, "Timeout waiting for ATA busy clear");
        spinlock_release_irq_restore(&ata_lock, flags);
        return;
    }

    outb(ATA_DRIVE_HEAD_PORT, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_SECTOR_COUNT_PORT, 1);
    outb(ATA_LBA_LOW_PORT, (uint8_t)lba);
    outb(ATA_LBA_MID_PORT, (uint8_t)(lba >> 8));
    outb(ATA_LBA_HIGH_PORT, (uint8_t)(lba >> 16));
    outb(ATA_COMMAND_PORT, ATA_CMD_WRITE_PIO);

    if (!ata_wait_busy_timeout()) {
        log_error(MODULE, "Timeout waiting for ATA busy clear");
        spinlock_release_irq_restore(&ata_lock, flags);
        return;
    }

    if (!ata_wait_drq_timeout()) {
        log_error(MODULE, "Timeout waiting for ATA DRQ");
        spinlock_release_irq_restore(&ata_lock, flags);
        return;
    }

    uint16_t *ptr = (uint16_t *)buffer;
    for (int i = 0; i < 256; i++) {
        outw(ATA_DATA_PORT, ptr[i]);
    }

    outb(ATA_COMMAND_PORT, 0xE7);

    if (!ata_wait_busy_timeout()) {
        log_error(MODULE, "Timeout waiting for ATA cache flush");
    }

    spinlock_release_irq_restore(&ata_lock, flags);
    log_debug(MODULE, "OK: Wrote To Sector %d", lba);
}

uint32_t ata_get_total_sectors(void) {
    uint32_t flags = spinlock_acquire_irq_save(&ata_lock);

    if (!ata_wait_busy_timeout()) {
        log_error(MODULE, "Timeout waiting for ATA busy clear");
        spinlock_release_irq_restore(&ata_lock, flags);
        return 0;
    }

    outb(ATA_DRIVE_HEAD_PORT, 0xA0);
    outb(ATA_COMMAND_PORT, ATA_CMD_IDENTIFY);

    uint8_t status = inb(ATA_STATUS_PORT);
    if (status == 0) {
        spinlock_release_irq_restore(&ata_lock, flags);
        return 0;
    }

    if (!ata_wait_busy_timeout()) {
        log_error(MODULE, "Timeout waiting for ATA busy clear");
        spinlock_release_irq_restore(&ata_lock, flags);
        return 0;
    }

    if (!ata_wait_drq_timeout()) {
        log_error(MODULE, "Timeout waiting for ATA DRQ");
        spinlock_release_irq_restore(&ata_lock, flags);
        return 0;
    }

    uint16_t identify_buffer[256];
    for (int i = 0; i < 256; i++) {
        identify_buffer[i] = inw(ATA_DATA_PORT);
    }

    uint32_t sectors = identify_buffer[60] | ((uint32_t)identify_buffer[61] << 16);
    spinlock_release_irq_restore(&ata_lock, flags);
    return sectors;
}