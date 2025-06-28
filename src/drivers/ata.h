#ifndef ATA_H
#define ATA_H

#include <stdint.h>
#include <stdbool.h>

/*------------------------------------------------------------------------------
 * ATA/IDE Driver for SKOS
 *------------------------------------------------------------------------------
 * This driver provides basic ATA/IDE hard disk support for the FAT32 file system.
 * Based on the OSDev wiki ATA documentation.
 *------------------------------------------------------------------------------
 */

/* ATA I/O Ports */
#define ATA_PRIMARY_IO_BASE     0x1F0
#define ATA_PRIMARY_CTRL_BASE   0x3F0
#define ATA_SECONDARY_IO_BASE   0x170
#define ATA_SECONDARY_CTRL_BASE 0x370

/* ATA Register Offsets */
#define ATA_REG_DATA        0x00
#define ATA_REG_ERROR       0x01
#define ATA_REG_FEATURES    0x01
#define ATA_REG_SECTOR_COUNT 0x02
#define ATA_REG_LBA_LOW     0x03
#define ATA_REG_LBA_MID     0x04
#define ATA_REG_LBA_HIGH    0x05
#define ATA_REG_DRIVE_HEAD  0x06
#define ATA_REG_STATUS      0x07
#define ATA_REG_COMMAND     0x07

/* ATA Control Register */
#define ATA_REG_ALT_STATUS  0x00
#define ATA_REG_DEVICE_CTRL 0x00

/* ATA Commands */
#define ATA_CMD_READ_SECTORS    0x20
#define ATA_CMD_WRITE_SECTORS   0x30
#define ATA_CMD_IDENTIFY        0xEC

/* ATA Status Register Bits */
#define ATA_STATUS_ERR      0x01    /* Error */
#define ATA_STATUS_DRQ      0x08    /* Data Request */
#define ATA_STATUS_SRV      0x10    /* Service Request */
#define ATA_STATUS_DF       0x20    /* Drive Fault */
#define ATA_STATUS_RDY      0x40    /* Ready */
#define ATA_STATUS_BSY      0x80    /* Busy */

/* ATA Drive Selection */
#define ATA_DRIVE_MASTER    0xE0
#define ATA_DRIVE_SLAVE     0xF0

/* ATA Device Structure */
typedef struct {
    uint16_t io_base;       /* I/O base address */
    uint16_t ctrl_base;     /* Control base address */
    uint8_t  drive;         /* Drive number (0 = master, 1 = slave) */
    bool     present;       /* Whether drive is present */
    uint32_t sectors;       /* Total number of sectors */
    char     model[41];     /* Drive model string */
} ata_device_t;

/* Function prototypes */

/* Initialize ATA subsystem */
bool ata_init(void);

/* Identify a drive */
bool ata_identify(ata_device_t* device);

/* Read sectors from drive */
bool ata_read_sectors(ata_device_t* device, uint32_t lba, uint8_t sector_count, void* buffer);

/* Write sectors to drive */
bool ata_write_sectors(ata_device_t* device, uint32_t lba, uint8_t sector_count, const void* buffer);

/* Wait for drive to be ready */
bool ata_wait_ready(ata_device_t* device);

/* Wait for data request */
bool ata_wait_drq(ata_device_t* device);

/* Get primary master device */
ata_device_t* ata_get_primary_master(void);

/* Get primary slave device */
ata_device_t* ata_get_primary_slave(void);

/* Print device information */
void ata_print_device_info(const ata_device_t* device);

#endif /* ATA_H */
