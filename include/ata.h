#ifndef SILVEROS_ATA_H
#define SILVEROS_ATA_H

#include "types.h"

/* Primary ATA bus ports */
#define ATA_PORT_DATA       0x1F0
#define ATA_PORT_ERROR      0x1F1
#define ATA_PORT_FEATURES   0x1F1
#define ATA_PORT_SECCOUNT   0x1F2
#define ATA_PORT_LBA_LO     0x1F3
#define ATA_PORT_LBA_MID    0x1F4
#define ATA_PORT_LBA_HI     0x1F5
#define ATA_PORT_DRIVE      0x1F6
#define ATA_PORT_STATUS     0x1F7
#define ATA_PORT_COMMAND    0x1F7
#define ATA_PORT_ALTSTATUS  0x3F6

/* ATA Status bits */
#define ATA_SR_BSY          0x80    /* Busy */
#define ATA_SR_DRDY         0x40    /* Drive ready */
#define ATA_SR_DF           0x20    /* Drive write fault */
#define ATA_SR_DSC          0x10    /* Drive seek complete */
#define ATA_SR_DRQ          0x08    /* Data request ready */
#define ATA_SR_CORR         0x04    /* Corrected data */
#define ATA_SR_IDX          0x02    /* Index */
#define ATA_SR_ERR          0x01    /* Error */

/* ATA Commands */
#define ATA_CMD_READ_PIO    0x20
#define ATA_CMD_WRITE_PIO   0x30
#define ATA_CMD_CACHE_FLUSH 0xE7

/* Sector size */
#define ATA_SECTOR_SIZE     512

void ata_init(void);
void ata_read_sector(uint32_t lba, uint8_t *buffer);
void ata_write_sector(uint32_t lba, const uint8_t *buffer);
void ata_read_sectors(uint32_t lba, uint8_t count, uint8_t *buffer);
void ata_write_sectors(uint32_t lba, uint8_t count, const uint8_t *buffer);

#endif
