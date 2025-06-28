/*------------------------------------------------------------------------------
 * ATA/IDE Driver Implementation
 *------------------------------------------------------------------------------
 * This file implements basic ATA/IDE hard disk support for SKOS.
 * Based on the OSDev wiki ATA documentation.
 *------------------------------------------------------------------------------
 */

#include "ata.h"
#include "../kernel/debug.h"
#include "../kernel/kernel.h"
#include <stdbool.h>
#include <stdint.h>

/* ATA device instances */
static ata_device_t primary_master;
static ata_device_t primary_slave;
static ata_device_t secondary_master;
static ata_device_t secondary_slave;

/* Current selected device for each controller */
static uint8_t current_primary_drive = 0xFF;
static uint8_t current_secondary_drive = 0xFF;

/* I/O port functions */
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t val) {
    asm volatile ("outw %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    asm volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* Delay function */
static void ata_delay(ata_device_t* device) {
    /* Read status register 4 times for ~400ns delay */
    for (int i = 0; i < 4; i++) {
        inb(device->ctrl_base + ATA_REG_ALT_STATUS);
    }
}

/* Select drive */
static void ata_select_drive(ata_device_t* device) {
    uint8_t drive_head = (device->drive == 0) ? ATA_DRIVE_MASTER : ATA_DRIVE_SLAVE;
    
    /* Check if we need to switch drives */
    if (device->io_base == ATA_PRIMARY_IO_BASE) {
        if (current_primary_drive != device->drive) {
            outb(device->io_base + ATA_REG_DRIVE_HEAD, drive_head);
            ata_delay(device);
            current_primary_drive = device->drive;
        }
    } else {
        if (current_secondary_drive != device->drive) {
            outb(device->io_base + ATA_REG_DRIVE_HEAD, drive_head);
            ata_delay(device);
            current_secondary_drive = device->drive;
        }
    }
}

/* Wait for drive to be ready */
bool ata_wait_ready(ata_device_t* device) {
    uint8_t status;
    int timeout = 10000;
    
    while (timeout--) {
        status = inb(device->io_base + ATA_REG_STATUS);
        
        /* Check for error */
        if (status & ATA_STATUS_ERR) {
            return false;
        }
        
        /* Check if ready and not busy */
        if ((status & ATA_STATUS_RDY) && !(status & ATA_STATUS_BSY)) {
            return true;
        }
        
        /* Small delay */
        for (volatile int i = 0; i < 100; i++);
    }
    
    return false;
}

/* Wait for data request */
bool ata_wait_drq(ata_device_t* device) {
    uint8_t status;
    int timeout = 10000;
    
    while (timeout--) {
        status = inb(device->io_base + ATA_REG_STATUS);
        
        /* Check for error */
        if (status & ATA_STATUS_ERR) {
            return false;
        }
        
        /* Check if data is ready */
        if (status & ATA_STATUS_DRQ) {
            return true;
        }
        
        /* Small delay */
        for (volatile int i = 0; i < 100; i++);
    }
    
    return false;
}

/* Initialize ATA device structure */
static void ata_init_device(ata_device_t* device, uint16_t io_base, uint16_t ctrl_base, uint8_t drive) {
    device->io_base = io_base;
    device->ctrl_base = ctrl_base;
    device->drive = drive;
    device->present = false;
    device->sectors = 0;
    
    for (int i = 0; i < 41; i++) {
        device->model[i] = 0;
    }
}

/* Identify a drive */
bool ata_identify(ata_device_t* device) {
    uint16_t identify_data[256];
    
    /* Select the drive */
    ata_select_drive(device);
    
    /* Send IDENTIFY command */
    outb(device->io_base + ATA_REG_COMMAND, ATA_CMD_IDENTIFY);
    
    /* Check if drive exists */
    uint8_t status = inb(device->io_base + ATA_REG_STATUS);
    if (status == 0) {
        return false;  /* Drive does not exist */
    }
    
    /* Wait for the drive to respond */
    if (!ata_wait_drq(device)) {
        return false;
    }
    
    /* Read the identification data */
    for (int i = 0; i < 256; i++) {
        identify_data[i] = inw(device->io_base + ATA_REG_DATA);
    }
    
    /* Extract model string (words 27-46) */
    for (int i = 0; i < 20; i++) {
        uint16_t word = identify_data[27 + i];
        device->model[i * 2] = (word >> 8) & 0xFF;
        device->model[i * 2 + 1] = word & 0xFF;
    }
    device->model[40] = '\0';
    
    /* Trim trailing spaces */
    for (int i = 39; i >= 0 && device->model[i] == ' '; i--) {
        device->model[i] = '\0';
    }
    
    /* Get number of sectors (LBA28) */
    device->sectors = (uint32_t)identify_data[60] | ((uint32_t)identify_data[61] << 16);
    
    device->present = true;
    return true;
}

/* Read sectors from drive */
bool ata_read_sectors(ata_device_t* device, uint32_t lba, uint8_t sector_count, void* buffer) {
    if (!device->present || sector_count == 0) {
        return false;
    }
    
    uint16_t* buf = (uint16_t*)buffer;
    
    /* Select the drive */
    ata_select_drive(device);
    
    /* Wait for drive to be ready */
    if (!ata_wait_ready(device)) {
        debug_print("ATA: Drive not ready for read");
        return false;
    }
    
    /* Set up LBA addressing */
    uint8_t drive_head = ((device->drive == 0) ? ATA_DRIVE_MASTER : ATA_DRIVE_SLAVE) | 
                        ((lba >> 24) & 0x0F);
    
    outb(device->io_base + ATA_REG_SECTOR_COUNT, sector_count);
    outb(device->io_base + ATA_REG_LBA_LOW, lba & 0xFF);
    outb(device->io_base + ATA_REG_LBA_MID, (lba >> 8) & 0xFF);
    outb(device->io_base + ATA_REG_LBA_HIGH, (lba >> 16) & 0xFF);
    outb(device->io_base + ATA_REG_DRIVE_HEAD, drive_head);
    
    /* Send READ SECTORS command */
    outb(device->io_base + ATA_REG_COMMAND, ATA_CMD_READ_SECTORS);
    
    /* Read each sector */
    for (int sector = 0; sector < sector_count; sector++) {
        /* Wait for data to be ready */
        if (!ata_wait_drq(device)) {
            debug_print("ATA: Timeout waiting for sector data");
            return false;
        }
        
        /* Read 256 words (512 bytes) */
        for (int i = 0; i < 256; i++) {
            buf[sector * 256 + i] = inw(device->io_base + ATA_REG_DATA);
        }
    }
    
    return true;
}

/* Write sectors to drive */
bool ata_write_sectors(ata_device_t* device, uint32_t lba, uint8_t sector_count, const void* buffer) {
    if (!device->present || sector_count == 0) {
        return false;
    }
    
    const uint16_t* buf = (const uint16_t*)buffer;
    
    /* Select the drive */
    ata_select_drive(device);
    
    /* Wait for drive to be ready */
    if (!ata_wait_ready(device)) {
        debug_print("ATA: Drive not ready for write");
        return false;
    }
    
    /* Set up LBA addressing */
    uint8_t drive_head = ((device->drive == 0) ? ATA_DRIVE_MASTER : ATA_DRIVE_SLAVE) | 
                        ((lba >> 24) & 0x0F);
    
    outb(device->io_base + ATA_REG_SECTOR_COUNT, sector_count);
    outb(device->io_base + ATA_REG_LBA_LOW, lba & 0xFF);
    outb(device->io_base + ATA_REG_LBA_MID, (lba >> 8) & 0xFF);
    outb(device->io_base + ATA_REG_LBA_HIGH, (lba >> 16) & 0xFF);
    outb(device->io_base + ATA_REG_DRIVE_HEAD, drive_head);
    
    /* Send WRITE SECTORS command */
    outb(device->io_base + ATA_REG_COMMAND, ATA_CMD_WRITE_SECTORS);
    
    /* Write each sector */
    for (int sector = 0; sector < sector_count; sector++) {
        /* Wait for drive to be ready for data */
        if (!ata_wait_drq(device)) {
            debug_print("ATA: Timeout waiting to write sector data");
            return false;
        }
        
        /* Write 256 words (512 bytes) */
        for (int i = 0; i < 256; i++) {
            outw(device->io_base + ATA_REG_DATA, buf[sector * 256 + i]);
        }
    }
    
    /* Wait for write to complete */
    return ata_wait_ready(device);
}

/* Initialize ATA subsystem */
bool ata_init(void) {
    debug_print("ATA: Initializing ATA/IDE subsystem...");
    
    /* Initialize device structures */
    ata_init_device(&primary_master, ATA_PRIMARY_IO_BASE, ATA_PRIMARY_CTRL_BASE, 0);
    ata_init_device(&primary_slave, ATA_PRIMARY_IO_BASE, ATA_PRIMARY_CTRL_BASE, 1);
    ata_init_device(&secondary_master, ATA_SECONDARY_IO_BASE, ATA_SECONDARY_CTRL_BASE, 0);
    ata_init_device(&secondary_slave, ATA_SECONDARY_IO_BASE, ATA_SECONDARY_CTRL_BASE, 1);
    
    /* Reset current drive selections */
    current_primary_drive = 0xFF;
    current_secondary_drive = 0xFF;
    
    /* Identify drives */
    bool found_drives = false;
    
    debug_print("ATA: Detecting primary master...");
    if (ata_identify(&primary_master)) {
        debug_print("ATA: Primary master detected");
        ata_print_device_info(&primary_master);
        found_drives = true;
    }
    
    debug_print("ATA: Detecting primary slave...");
    if (ata_identify(&primary_slave)) {
        debug_print("ATA: Primary slave detected");
        ata_print_device_info(&primary_slave);
        found_drives = true;
    }
    
    debug_print("ATA: Detecting secondary master...");
    if (ata_identify(&secondary_master)) {
        debug_print("ATA: Secondary master detected");
        ata_print_device_info(&secondary_master);
        found_drives = true;
    }
    
    debug_print("ATA: Detecting secondary slave...");
    if (ata_identify(&secondary_slave)) {
        debug_print("ATA: Secondary slave detected");
        ata_print_device_info(&secondary_slave);
        found_drives = true;
    }
    
    if (found_drives) {
        debug_print("ATA: Initialization complete");
    } else {
        debug_print("ATA: No drives detected");
    }
    
    return found_drives;
}

/* Get primary master device */
ata_device_t* ata_get_primary_master(void) {
    return primary_master.present ? &primary_master : NULL;
}

/* Get primary slave device */
ata_device_t* ata_get_primary_slave(void) {
    return primary_slave.present ? &primary_slave : NULL;
}

/* Print device information */
void ata_print_device_info(const ata_device_t* device) {
    if (!device || !device->present) {
        return;
    }
    
    terminal_writestring("  Model: ");
    terminal_writestring(device->model);
    terminal_writestring("\n  Sectors: ");
    
    /* Print sector count */
    char sectors_str[16];
    uint32_t sectors = device->sectors;
    int digits = 0;
    
    if (sectors == 0) {
        sectors_str[0] = '0';
        digits = 1;
    } else {
        while (sectors > 0) {
            sectors_str[digits++] = '0' + (sectors % 10);
            sectors /= 10;
        }
    }
    
    /* Reverse the string */
    for (int i = 0; i < digits / 2; i++) {
        char temp = sectors_str[i];
        sectors_str[i] = sectors_str[digits - 1 - i];
        sectors_str[digits - 1 - i] = temp;
    }
    sectors_str[digits] = '\0';
    
    terminal_writestring(sectors_str);
    
    /* Calculate and display capacity in MB */
    uint32_t capacity_mb = (device->sectors * 512) / (1024 * 1024);
    terminal_writestring(" (");
    
    digits = 0;
    if (capacity_mb == 0) {
        sectors_str[0] = '0';
        digits = 1;
    } else {
        while (capacity_mb > 0) {
            sectors_str[digits++] = '0' + (capacity_mb % 10);
            capacity_mb /= 10;
        }
    }
    
    /* Reverse the string */
    for (int i = 0; i < digits / 2; i++) {
        char temp = sectors_str[i];
        sectors_str[i] = sectors_str[digits - 1 - i];
        sectors_str[digits - 1 - i] = temp;
    }
    sectors_str[digits] = '\0';
    
    terminal_writestring(sectors_str);
    terminal_writestring(" MB)\n");
}
