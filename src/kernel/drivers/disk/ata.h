#ifndef ATA_H
#define ATA_H

#include <stdint.h>

#define ATA_DATA_PORT 0x1F0
#define ATA_ERROR_PORT 0x1F1    // (Read)
#define ATA_FEATURES_PORT 0x1F1 // (Write)
#define ATA_SECTOR_COUNT_PORT 0x1F2
#define ATA_LBA_LOW_PORT 0x1F3
#define ATA_LBA_MID_PORT 0x1F4
#define ATA_LBA_HIGH_PORT 0x1F5
#define ATA_DRIVE_HEAD_PORT 0x1F6
#define ATA_STATUS_PORT 0x1F7  // (Read)
#define ATA_COMMAND_PORT 0x1F7 // (Write)

#define ATA_SR_BSY 0x80  // Busy
#define ATA_SR_DRDY 0x40 // Drive Ready
#define ATA_SR_DF 0x20   // Drive Write Fault
#define ATA_SR_DRQ 0x08  // Data Request Ready
#define ATA_SR_ERR 0x01  // Error

#define ATA_CMD_READ_PIO 0x20
#define ATA_CMD_WRITE_PIO 0x30
#define ATA_CMD_IDENTIFY 0xEC

void ata_read_sector(uint32_t lba, uint8_t *buffer);
void ata_write_sector(uint32_t lba, uint8_t *buffer);
uint32_t ata_get_total_sectors();

#endif