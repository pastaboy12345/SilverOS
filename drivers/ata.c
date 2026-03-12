#include "../include/ata.h"
#include "../include/io.h"
#include "../include/console.h"

static int ata_wait_bsy(void) {
    int timeout = 1000000;
    while (timeout--) {
        if (!(inb(ATA_PORT_STATUS) & ATA_SR_BSY)) return 0;
    }
    return -1; // Timeout
}

static int ata_wait_drq(void) {
    int timeout = 1000000;
    while (timeout--) {
        if (inb(ATA_PORT_STATUS) & ATA_SR_DRQ) return 0;
    }
    return -1; // Timeout
}

static void ata_delay(void) {
    inb(ATA_PORT_ALTSTATUS);
    inb(ATA_PORT_ALTSTATUS);
    inb(ATA_PORT_ALTSTATUS);
    inb(ATA_PORT_ALTSTATUS);
}

void ata_init(void) {
    kprintf("[ATA] Initializing ATA PIO (Primary IDE)...\n");
    /* Select master drive */
    outb(ATA_PORT_DRIVE, 0xA0);
    ata_delay();
    
    /* We assume drive exists and is ready for our simple emulator case */
    uint8_t status = inb(ATA_PORT_STATUS);
    if (status == 0xFF) {
        kprintf("[ATA] WARNING: No primary master drive detected (Floating bus).\n");
        return;
    }
    kprintf("[ATA] Primary master drive ready.\n");
}

void ata_read_sectors(uint32_t lba, uint8_t count, uint8_t *buffer) {
    ata_wait_bsy();
    
    outb(ATA_PORT_DRIVE, 0xE0 | ((lba >> 24) & 0x0F)); /* Master drive, LBA mode, top 4 bits */
    outb(ATA_PORT_SECCOUNT, count);
    outb(ATA_PORT_LBA_LO, (uint8_t)lba);
    outb(ATA_PORT_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_PORT_LBA_HI, (uint8_t)(lba >> 16));
    outb(ATA_PORT_COMMAND, ATA_CMD_READ_PIO);

    uint16_t *ptr = (uint16_t *)buffer;
    for (int i = 0; i < count; i++) {
        ata_wait_bsy();
        ata_wait_drq();
        for (int j = 0; j < 256; j++) {
            ptr[j] = inw(ATA_PORT_DATA);
        }
        ptr += 256;
    }
}

void ata_write_sectors(uint32_t lba, uint8_t count, const uint8_t *buffer) {
    ata_wait_bsy();

    outb(ATA_PORT_DRIVE, 0xE0 | ((lba >> 24) & 0x0F));
    outb(ATA_PORT_SECCOUNT, count);
    outb(ATA_PORT_LBA_LO, (uint8_t)lba);
    outb(ATA_PORT_LBA_MID, (uint8_t)(lba >> 8));
    outb(ATA_PORT_LBA_HI, (uint8_t)(lba >> 16));
    outb(ATA_PORT_COMMAND, ATA_CMD_WRITE_PIO);

    const uint16_t *ptr = (const uint16_t *)buffer;
    for (int i = 0; i < count; i++) {
        ata_wait_bsy();
        ata_wait_drq();
        for (int j = 0; j < 256; j++) {
            outw(ATA_PORT_DATA, ptr[j]);
        }
        ptr += 256;
    }
    
    /* Cache flush */
    outb(ATA_PORT_COMMAND, ATA_CMD_CACHE_FLUSH);
    ata_wait_bsy();
}

void ata_read_sector(uint32_t lba, uint8_t *buffer) {
    ata_read_sectors(lba, 1, buffer);
}

void ata_write_sector(uint32_t lba, const uint8_t *buffer) {
    ata_write_sectors(lba, 1, buffer);
}
