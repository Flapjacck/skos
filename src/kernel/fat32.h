#ifndef FAT32_H
#define FAT32_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/*------------------------------------------------------------------------------
 * FAT32 File System Implementation
 *------------------------------------------------------------------------------
 * Based on the Microsoft FAT32 specification and OSDev wiki documentation.
 * This implementation provides basic file system operations for reading
 * and writing files on FAT32 formatted storage devices.
 *------------------------------------------------------------------------------
 */

/* FAT32 Boot Sector Structure */
typedef struct {
    uint8_t  bootjmp[3];        /* Boot jump instruction */
    uint8_t  oem_name[8];       /* OEM name */
    uint16_t bytes_per_sector;  /* Bytes per sector (usually 512) */
    uint8_t  sectors_per_cluster; /* Sectors per cluster */
    uint16_t reserved_sectors;  /* Reserved sectors */
    uint8_t  num_fats;         /* Number of FATs (usually 2) */
    uint16_t root_entries;     /* Root directory entries (0 for FAT32) */
    uint16_t total_sectors_16; /* Total sectors (0 for FAT32) */
    uint8_t  media_type;       /* Media type */
    uint16_t fat_size_16;      /* FAT size in sectors (0 for FAT32) */
    uint16_t sectors_per_track; /* Sectors per track */
    uint16_t num_heads;        /* Number of heads */
    uint32_t hidden_sectors;   /* Hidden sectors */
    uint32_t total_sectors_32; /* Total sectors */
    
    /* FAT32 Extended Boot Record */
    uint32_t fat_size_32;      /* FAT size in sectors */
    uint16_t ext_flags;        /* Extended flags */
    uint16_t fs_version;       /* File system version */
    uint32_t root_cluster;     /* Root directory cluster */
    uint16_t fs_info;          /* FS info sector */
    uint16_t backup_boot_sec;  /* Backup boot sector */
    uint8_t  reserved[12];     /* Reserved */
    uint8_t  drive_number;     /* Drive number */
    uint8_t  reserved1;        /* Reserved */
    uint8_t  boot_signature;   /* Boot signature */
    uint32_t volume_id;        /* Volume ID */
    uint8_t  volume_label[11]; /* Volume label */
    uint8_t  fs_type[8];       /* File system type */
    uint8_t  boot_code[420];   /* Boot code */
    uint16_t boot_sector_signature; /* Boot sector signature (0xAA55) */
} __attribute__((packed)) fat32_boot_sector_t;

/* FAT32 Directory Entry Structure */
typedef struct {
    uint8_t  name[11];         /* 8.3 filename */
    uint8_t  attributes;       /* File attributes */
    uint8_t  reserved;         /* Reserved */
    uint8_t  creation_time_tenth; /* Creation time (tenth of second) */
    uint16_t creation_time;    /* Creation time */
    uint16_t creation_date;    /* Creation date */
    uint16_t last_access_date; /* Last access date */
    uint16_t first_cluster_high; /* High 16 bits of first cluster */
    uint16_t last_write_time;  /* Last write time */
    uint16_t last_write_date;  /* Last write date */
    uint16_t first_cluster_low; /* Low 16 bits of first cluster */
    uint32_t file_size;        /* File size in bytes */
} __attribute__((packed)) fat32_dir_entry_t;

/* FAT32 Long File Name Entry */
typedef struct {
    uint8_t  order;            /* Order of this entry in the LFN sequence */
    uint16_t name1[5];         /* First 5 UTF-16 characters */
    uint8_t  attributes;       /* Attributes (always 0x0F for LFN) */
    uint8_t  type;             /* Type (always 0 for LFN) */
    uint8_t  checksum;         /* Checksum of short name */
    uint16_t name2[6];         /* Next 6 UTF-16 characters */
    uint16_t first_cluster_low; /* Always 0 for LFN */
    uint16_t name3[2];         /* Last 2 UTF-16 characters */
} __attribute__((packed)) fat32_lfn_entry_t;

/* File Attributes */
#define FAT_ATTR_READ_ONLY   0x01
#define FAT_ATTR_HIDDEN      0x02
#define FAT_ATTR_SYSTEM      0x04
#define FAT_ATTR_VOLUME_ID   0x08
#define FAT_ATTR_DIRECTORY   0x10
#define FAT_ATTR_ARCHIVE     0x20
#define FAT_ATTR_LONG_NAME   0x0F

/* FAT32 Constants */
#define FAT32_EOC           0x0FFFFFF8  /* End of cluster chain */
#define FAT32_BAD_CLUSTER   0x0FFFFFF7  /* Bad cluster */
#define FAT32_FREE_CLUSTER  0x00000000  /* Free cluster */

/* Maximum file name length */
#define FAT32_MAX_FILENAME  255

/* File handle structure */
typedef struct {
    uint32_t first_cluster;    /* First cluster of file */
    uint32_t current_cluster;  /* Current cluster being read/written */
    uint32_t file_size;        /* Size of file in bytes */
    uint32_t position;         /* Current position in file */
    uint8_t  attributes;       /* File attributes */
    bool     is_open;          /* Whether file is open */
    char     filename[FAT32_MAX_FILENAME + 1]; /* File name */
} fat32_file_t;

/* Directory handle structure */
typedef struct {
    uint32_t cluster;          /* Current cluster being read */
    uint32_t entry_index;      /* Current entry index within cluster */
    bool     is_open;          /* Whether directory is open */
} fat32_dir_t;

/* File system information structure */
typedef struct {
    fat32_boot_sector_t boot_sector;  /* Boot sector data */
    uint32_t fat_start_sector;        /* First sector of FAT */
    uint32_t data_start_sector;       /* First sector of data area */
    uint32_t sectors_per_cluster;     /* Sectors per cluster */
    uint32_t bytes_per_cluster;       /* Bytes per cluster */
    uint32_t total_clusters;          /* Total number of clusters */
    uint32_t root_dir_cluster;        /* Root directory cluster */
    bool     initialized;             /* Whether file system is initialized */
} fat32_fs_info_t;

/* Function prototypes */

/* Initialize FAT32 file system */
bool fat32_init(void);

/* Read a sector from the storage device */
bool fat32_read_sector(uint32_t sector, void* buffer);

/* Write a sector to the storage device */
bool fat32_write_sector(uint32_t sector, const void* buffer);

/* Get the next cluster in a cluster chain */
uint32_t fat32_get_next_cluster(uint32_t cluster);

/* Set the next cluster in a cluster chain */
bool fat32_set_next_cluster(uint32_t cluster, uint32_t next_cluster);

/* Find a free cluster */
uint32_t fat32_find_free_cluster(void);

/* Convert cluster number to sector number */
uint32_t fat32_cluster_to_sector(uint32_t cluster);

/* File operations */
fat32_file_t* fat32_open(const char* filename);
void fat32_close(fat32_file_t* file);
size_t fat32_read(fat32_file_t* file, void* buffer, size_t size);
size_t fat32_write(fat32_file_t* file, const void* buffer, size_t size);
bool fat32_seek(fat32_file_t* file, uint32_t position);
uint32_t fat32_tell(fat32_file_t* file);

/* Directory operations */
fat32_dir_t* fat32_opendir(const char* path);
void fat32_closedir(fat32_dir_t* dir);
fat32_dir_entry_t* fat32_readdir(fat32_dir_t* dir);

/* Utility functions */
void fat32_convert_filename(const char* input, char* output);
bool fat32_compare_filename(const char* name1, const char* name2);
void fat32_print_file_info(const fat32_dir_entry_t* entry);

/* Get file system information */
fat32_fs_info_t* fat32_get_fs_info(void);

/* Cleanup FAT32 file system */
void fat32_cleanup(void);

#endif /* FAT32_H */
