#include "ata.h"
#include "../../utils/logging/logger.h"
#include "../ports/IO.h"
#include <stdint.h>

#define MODULE "ATA_DISK"

/**
 * @brief Wait for disk to give the ready to transfer
 *
 */
static void ata_wait_busy() {
  while (inb(ATA_STATUS_PORT) & ATA_SR_BSY)
    ;
}

static void ata_wait_drq() {
  while (!(inb(ATA_STATUS_PORT) & ATA_SR_DRQ))
    ;
}

/**
 * @brief Reads a sector from the disk to the buffer
 *
 * @param lba the logical block address (sector number)
 * @param buffer a pointer to an array of at least 512 bytes
 */
void ata_read_sector(uint32_t lba, uint8_t *buffer) {
  ata_wait_busy();

  outb(ATA_DRIVE_HEAD_PORT, 0xE0 | ((lba >> 24) & 0x0F));
  outb(ATA_SECTOR_COUNT_PORT, 1);
  outb(ATA_LBA_LOW_PORT, (uint8_t)lba);
  outb(ATA_LBA_MID_PORT, (uint8_t)(lba >> 8));
  outb(ATA_LBA_HIGH_PORT, (uint8_t)(lba >> 16));

  outb(ATA_COMMAND_PORT, ATA_CMD_READ_PIO);

  ata_wait_busy();
  ata_wait_drq();

  uint16_t *ptr = (uint16_t *)buffer;
  for (int i = 0; i < 256; i++) {
    ptr[i] = inw(ATA_DATA_PORT);
  }

  log_debug(MODULE, "OK: Read From Sector %d", lba);
}

void ata_write_sector(uint32_t lba, uint8_t *buffer) {
  ata_wait_busy();

  outb(ATA_DRIVE_HEAD_PORT, 0xE0 | ((lba >> 24) & 0x0F));
  outb(ATA_SECTOR_COUNT_PORT, 1);
  outb(ATA_LBA_LOW_PORT, (uint8_t)lba);
  outb(ATA_LBA_MID_PORT, (uint8_t)(lba >> 8));
  outb(ATA_LBA_HIGH_PORT, (uint8_t)(lba >> 16));

  outb(ATA_COMMAND_PORT, ATA_CMD_WRITE_PIO);

  ata_wait_busy();
  ata_wait_drq();

  uint16_t *ptr = (uint16_t *)buffer;
  for (int i = 0; i < 256; i++) {
    outw(ATA_DATA_PORT, ptr[i]);
  }

  outb(ATA_COMMAND_PORT, 0xE7);
  ata_wait_busy();

  log_debug(MODULE, "OK: Wrote To Sector %d", lba);
}

uint32_t ata_get_total_sectors() {
  ata_wait_busy();

  outb(ATA_DRIVE_HEAD_PORT, 0xA0);
  outb(ATA_COMMAND_PORT, ATA_CMD_IDENTIFY);

  uint8_t status = inb(ATA_STATUS_PORT);
  if (status == 0)
    return 0;

  ata_wait_busy();
  ata_wait_drq();

  uint16_t identify_buffer[256];

  for (int i = 0; i < 256; i++) {
    identify_buffer[i] = inw(ATA_DATA_PORT);
  }

  uint32_t sectors = identify_buffer[60] | (uint32_t)identify_buffer[61] << 16;
  return sectors;
}