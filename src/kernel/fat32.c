/*------------------------------------------------------------------------------
 * FAT32 File System Implementation
 *------------------------------------------------------------------------------
 * This file implements the FAT32 file system for SKOS.
 * Based on the Microsoft FAT32 specification and OSDev wiki documentation.
 *------------------------------------------------------------------------------
 */

#include "fat32.h"
#include "memory.h"
#include "debug.h"
#include "kernel.h"
#include "../drivers/ata.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Global file system information */
static fat32_fs_info_t fs_info;

/* Primary storage device */
static ata_device_t* storage_device = NULL;

/* Temporary sector buffer for I/O operations */
static uint8_t sector_buffer[512] __attribute__((aligned(4)));

/* File handle pool */
#define MAX_OPEN_FILES 16
static fat32_file_t file_handles[MAX_OPEN_FILES];

/* Directory handle pool */
#define MAX_OPEN_DIRS 8
static fat32_dir_t dir_handles[MAX_OPEN_DIRS];

/*------------------------------------------------------------------------------
 * Low-level disk I/O functions
 * These are placeholder implementations that need to be replaced with
 * actual ATA/IDE driver calls when available.
 *------------------------------------------------------------------------------
 */

/* Read a sector from the storage device */
bool fat32_read_sector(uint32_t sector, void* buffer) {
    if (!storage_device) {
        debug_print("FAT32: No storage device available");
        return false;
    }
    
    return ata_read_sectors(storage_device, sector, 1, buffer);
}

/* Write a sector to the storage device */
bool fat32_write_sector(uint32_t sector, const void* buffer) {
    if (!storage_device) {
        debug_print("FAT32: No storage device available");
        return false;
    }
    
    return ata_write_sectors(storage_device, sector, 1, buffer);
}

/*------------------------------------------------------------------------------
 * FAT32 Core Functions
 *------------------------------------------------------------------------------
 */

/* Initialize FAT32 file system */
bool fat32_init(void) {
    debug_print("FAT32: Initializing file system...");
    
    /* Clear file system info */
    for (size_t i = 0; i < sizeof(fat32_fs_info_t); i++) {
        ((uint8_t*)&fs_info)[i] = 0;
    }
    
    /* Initialize file handle pool */
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        file_handles[i].is_open = false;
    }
    
    /* Initialize directory handle pool */
    for (int i = 0; i < MAX_OPEN_DIRS; i++) {
        dir_handles[i].is_open = false;
    }
    
    /* Get the primary storage device */
    storage_device = ata_get_primary_master();
    if (!storage_device) {
        storage_device = ata_get_primary_slave();
    }
    
    if (!storage_device) {
        debug_print("FAT32: No storage device found");
        return false;
    }
    
    debug_print("FAT32: Using storage device");
    
    /* Try to read the boot sector */
    if (!fat32_read_sector(0, &fs_info.boot_sector)) {
        debug_print("FAT32: Failed to read boot sector");
        return false;
    }
    
    /* Verify this is a valid FAT32 boot sector */
    if (fs_info.boot_sector.boot_sector_signature != 0xAA55) {
        debug_print("FAT32: Invalid boot sector signature");
        return false;
    }
    
    /* Verify FAT32 signature */
    if (fs_info.boot_sector.fat_size_16 != 0 || 
        fs_info.boot_sector.root_entries != 0 ||
        fs_info.boot_sector.total_sectors_16 != 0) {
        debug_print("FAT32: Not a FAT32 file system");
        return false;
    }
    
    /* Calculate file system parameters */
    fs_info.fat_start_sector = fs_info.boot_sector.reserved_sectors;
    fs_info.data_start_sector = fs_info.fat_start_sector + 
                               (fs_info.boot_sector.num_fats * fs_info.boot_sector.fat_size_32);
    fs_info.sectors_per_cluster = fs_info.boot_sector.sectors_per_cluster;
    fs_info.bytes_per_cluster = fs_info.sectors_per_cluster * fs_info.boot_sector.bytes_per_sector;
    fs_info.root_dir_cluster = fs_info.boot_sector.root_cluster;
    
    /* Calculate total number of data clusters */
    uint32_t data_sectors = fs_info.boot_sector.total_sectors_32 - fs_info.data_start_sector;
    fs_info.total_clusters = data_sectors / fs_info.sectors_per_cluster;
    
    /* Mark as initialized */
    fs_info.initialized = true;
    
    debug_print("FAT32: File system initialized successfully");
    
    return true;
}

/* Get the next cluster in a cluster chain */
uint32_t fat32_get_next_cluster(uint32_t cluster) {
    if (!fs_info.initialized || cluster < 2) {
        return FAT32_EOC;
    }
    
    /* Calculate FAT sector and offset */
    uint32_t fat_offset = cluster * 4;  /* 4 bytes per FAT32 entry */
    uint32_t fat_sector = fs_info.fat_start_sector + (fat_offset / fs_info.boot_sector.bytes_per_sector);
    uint32_t entry_offset = fat_offset % fs_info.boot_sector.bytes_per_sector;
    
    /* Read the FAT sector */
    if (!fat32_read_sector(fat_sector, sector_buffer)) {
        return FAT32_EOC;
    }
    
    /* Extract the cluster value */
    uint32_t next_cluster = *(uint32_t*)(sector_buffer + entry_offset) & 0x0FFFFFFF;
    
    return next_cluster;
}

/* Set the next cluster in a cluster chain */
bool fat32_set_next_cluster(uint32_t cluster, uint32_t next_cluster) {
    if (!fs_info.initialized || cluster < 2) {
        return false;
    }
    
    /* Calculate FAT sector and offset */
    uint32_t fat_offset = cluster * 4;  /* 4 bytes per FAT32 entry */
    uint32_t fat_sector = fs_info.fat_start_sector + (fat_offset / fs_info.boot_sector.bytes_per_sector);
    uint32_t entry_offset = fat_offset % fs_info.boot_sector.bytes_per_sector;
    
    /* Read the FAT sector */
    if (!fat32_read_sector(fat_sector, sector_buffer)) {
        return false;
    }
    
    /* Update the cluster value (preserve upper 4 bits) */
    uint32_t* fat_entry = (uint32_t*)(sector_buffer + entry_offset);
    *fat_entry = (*fat_entry & 0xF0000000) | (next_cluster & 0x0FFFFFFF);
    
    /* Write back the FAT sector */
    return fat32_write_sector(fat_sector, sector_buffer);
}

/* Find a free cluster */
uint32_t fat32_find_free_cluster(void) {
    if (!fs_info.initialized) {
        return 0;
    }
    
    /* Start searching from cluster 2 */
    for (uint32_t cluster = 2; cluster < fs_info.total_clusters + 2; cluster++) {
        uint32_t next = fat32_get_next_cluster(cluster);
        if (next == FAT32_FREE_CLUSTER) {
            return cluster;
        }
    }
    
    return 0;  /* No free clusters found */
}

/* Convert cluster number to sector number */
uint32_t fat32_cluster_to_sector(uint32_t cluster) {
    if (!fs_info.initialized || cluster < 2) {
        return 0;
    }
    
    return fs_info.data_start_sector + ((cluster - 2) * fs_info.sectors_per_cluster);
}

/*------------------------------------------------------------------------------
 * File Operations
 *------------------------------------------------------------------------------
 */

/* Find a directory entry by name */
static fat32_dir_entry_t* fat32_find_entry(uint32_t dir_cluster, const char* filename) {
    static fat32_dir_entry_t found_entry;
    uint32_t current_cluster = dir_cluster;
    
    while (current_cluster < FAT32_EOC) {
        uint32_t sector = fat32_cluster_to_sector(current_cluster);
        
        /* Read all sectors in this cluster */
        for (uint32_t i = 0; i < fs_info.sectors_per_cluster; i++) {
            if (!fat32_read_sector(sector + i, sector_buffer)) {
                return NULL;
            }
            
            /* Check all directory entries in this sector */
            fat32_dir_entry_t* entries = (fat32_dir_entry_t*)sector_buffer;
            uint32_t entries_per_sector = fs_info.boot_sector.bytes_per_sector / sizeof(fat32_dir_entry_t);
            
            for (uint32_t j = 0; j < entries_per_sector; j++) {
                /* Skip deleted entries */
                if (entries[j].name[0] == 0xE5) {
                    continue;
                }
                
                /* End of directory */
                if (entries[j].name[0] == 0x00) {
                    return NULL;
                }
                
                /* Skip long file name entries */
                if (entries[j].attributes == FAT_ATTR_LONG_NAME) {
                    continue;
                }
                
                /* Convert and compare filename */
                char entry_name[13];
                fat32_convert_filename((char*)entries[j].name, entry_name);
                
                if (fat32_compare_filename(filename, entry_name)) {
                    found_entry = entries[j];
                    return &found_entry;
                }
            }
        }
        
        /* Move to next cluster */
        current_cluster = fat32_get_next_cluster(current_cluster);
    }
    
    return NULL;
}

/* Open a file */
fat32_file_t* fat32_open(const char* filename) {
    if (!fs_info.initialized || !filename) {
        return NULL;
    }
    
    /* Find a free file handle */
    fat32_file_t* file = NULL;
    for (int i = 0; i < MAX_OPEN_FILES; i++) {
        if (!file_handles[i].is_open) {
            file = &file_handles[i];
            break;
        }
    }
    
    if (!file) {
        debug_print("FAT32: No free file handles");
        return NULL;
    }
    
    /* Find the file in the root directory */
    fat32_dir_entry_t* entry = fat32_find_entry(fs_info.root_dir_cluster, filename);
    if (!entry) {
        debug_print("FAT32: File not found");
        return NULL;
    }
    
    /* Initialize file handle */
    file->first_cluster = ((uint32_t)entry->first_cluster_high << 16) | entry->first_cluster_low;
    file->current_cluster = file->first_cluster;
    file->file_size = entry->file_size;
    file->position = 0;
    file->attributes = entry->attributes;
    file->is_open = true;
    
    /* Copy filename */
    size_t len = 0;
    while (filename[len] && len < FAT32_MAX_FILENAME) {
        file->filename[len] = filename[len];
        len++;
    }
    file->filename[len] = '\0';
    
    debug_print("FAT32: Opened file successfully");
    
    return file;
}

/* Close a file */
void fat32_close(fat32_file_t* file) {
    if (file && file->is_open) {
        file->is_open = false;
        debug_print("FAT32: Closed file");
    }
}

/* Read from a file */
size_t fat32_read(fat32_file_t* file, void* buffer, size_t size) {
    if (!file || !file->is_open || !buffer || size == 0) {
        return 0;
    }
    
    /* Don't read past end of file */
    if (file->position >= file->file_size) {
        return 0;
    }
    
    if (file->position + size > file->file_size) {
        size = file->file_size - file->position;
    }
    
    size_t bytes_read = 0;
    uint8_t* dest = (uint8_t*)buffer;
    
    while (bytes_read < size && file->current_cluster < FAT32_EOC) {
        uint32_t cluster_offset = file->position % fs_info.bytes_per_cluster;
        uint32_t bytes_in_cluster = fs_info.bytes_per_cluster - cluster_offset;
        uint32_t bytes_to_read = (size - bytes_read < bytes_in_cluster) ? 
                                (size - bytes_read) : bytes_in_cluster;
        
        /* Read the cluster */
        uint32_t sector = fat32_cluster_to_sector(file->current_cluster);
        uint32_t sector_offset = cluster_offset / fs_info.boot_sector.bytes_per_sector;
        uint32_t byte_offset = cluster_offset % fs_info.boot_sector.bytes_per_sector;
        
        /* Handle reading within a sector */
        while (bytes_to_read > 0 && sector_offset < fs_info.sectors_per_cluster) {
            if (!fat32_read_sector(sector + sector_offset, sector_buffer)) {
                break;
            }
            
            uint32_t bytes_in_sector = fs_info.boot_sector.bytes_per_sector - byte_offset;
            uint32_t copy_size = (bytes_to_read < bytes_in_sector) ? bytes_to_read : bytes_in_sector;
            
            /* Copy data from sector buffer */
            for (uint32_t i = 0; i < copy_size; i++) {
                dest[bytes_read + i] = sector_buffer[byte_offset + i];
            }
            
            bytes_read += copy_size;
            bytes_to_read -= copy_size;
            file->position += copy_size;
            
            sector_offset++;
            byte_offset = 0;
        }
        
        /* Move to next cluster if we've read the entire current cluster */
        if (cluster_offset + (size - bytes_read) >= fs_info.bytes_per_cluster) {
            file->current_cluster = fat32_get_next_cluster(file->current_cluster);
        }
    }
    
    return bytes_read;
}

/* Write to a file (basic implementation) */
size_t fat32_write(fat32_file_t* file, const void* buffer, size_t size) {
    /* TODO: Implement file writing */
    (void)file;    /* Suppress unused parameter warning */
    (void)buffer;  /* Suppress unused parameter warning */
    (void)size;    /* Suppress unused parameter warning */
    debug_print("FAT32: Write operation not yet implemented");
    return 0;
}

/* Seek to a position in the file */
bool fat32_seek(fat32_file_t* file, uint32_t position) {
    if (!file || !file->is_open) {
        return false;
    }
    
    if (position > file->file_size) {
        position = file->file_size;
    }
    
    /* Reset to beginning of file */
    file->current_cluster = file->first_cluster;
    file->position = 0;
    
    /* Skip clusters to reach the desired position */
    while (file->position + fs_info.bytes_per_cluster <= position && 
           file->current_cluster < FAT32_EOC) {
        file->position += fs_info.bytes_per_cluster;
        file->current_cluster = fat32_get_next_cluster(file->current_cluster);
    }
    
    file->position = position;
    return true;
}

/* Get current position in file */
uint32_t fat32_tell(fat32_file_t* file) {
    if (!file || !file->is_open) {
        return 0;
    }
    
    return file->position;
}

/*------------------------------------------------------------------------------
 * Directory Operations
 *------------------------------------------------------------------------------
 */

/* Open a directory */
fat32_dir_t* fat32_opendir(const char* path) {
    if (!fs_info.initialized || !path) {
        return NULL;
    }
    
    /* Find a free directory handle */
    fat32_dir_t* dir = NULL;
    for (int i = 0; i < MAX_OPEN_DIRS; i++) {
        if (!dir_handles[i].is_open) {
            dir = &dir_handles[i];
            break;
        }
    }
    
    if (!dir) {
        debug_print("FAT32: No free directory handles");
        return NULL;
    }
    
    /* For now, only support root directory */
    if (path[0] == '/' && path[1] == '\0') {
        dir->cluster = fs_info.root_dir_cluster;
        dir->entry_index = 0;
        dir->is_open = true;
        return dir;
    }
    
    debug_print("FAT32: Directory path not supported");
    return NULL;
}

/* Close a directory */
void fat32_closedir(fat32_dir_t* dir) {
    if (dir && dir->is_open) {
        dir->is_open = false;
    }
}

/* Read a directory entry */
fat32_dir_entry_t* fat32_readdir(fat32_dir_t* dir) {
    static fat32_dir_entry_t current_entry;
    
    if (!dir || !dir->is_open || !fs_info.initialized) {
        return NULL;
    }
    
    uint32_t current_cluster = dir->cluster;
    uint32_t entry_index = dir->entry_index;
    uint32_t entries_per_cluster = fs_info.bytes_per_cluster / sizeof(fat32_dir_entry_t);
    
    while (current_cluster < FAT32_EOC) {
        /* Calculate which sector and entry within the cluster */
        uint32_t sector_in_cluster = (entry_index * sizeof(fat32_dir_entry_t)) / fs_info.boot_sector.bytes_per_sector;
        uint32_t entry_in_sector = ((entry_index * sizeof(fat32_dir_entry_t)) % fs_info.boot_sector.bytes_per_sector) / sizeof(fat32_dir_entry_t);
        
        /* Read the sector */
        uint32_t sector = fat32_cluster_to_sector(current_cluster) + sector_in_cluster;
        if (!fat32_read_sector(sector, sector_buffer)) {
            return NULL;
        }
        
        fat32_dir_entry_t* entries = (fat32_dir_entry_t*)sector_buffer;
        fat32_dir_entry_t* entry = &entries[entry_in_sector];
        
        /* Check for end of directory */
        if (entry->name[0] == 0x00) {
            return NULL;
        }
        
        /* Skip deleted entries */
        if (entry->name[0] == 0xE5) {
            entry_index++;
            if (entry_index >= entries_per_cluster) {
                current_cluster = fat32_get_next_cluster(current_cluster);
                entry_index = 0;
            }
            continue;
        }
        
        /* Skip long file name entries */
        if (entry->attributes == FAT_ATTR_LONG_NAME) {
            entry_index++;
            if (entry_index >= entries_per_cluster) {
                current_cluster = fat32_get_next_cluster(current_cluster);
                entry_index = 0;
            }
            continue;
        }
        
        /* Skip volume label entries */
        if (entry->attributes & FAT_ATTR_VOLUME_ID) {
            entry_index++;
            if (entry_index >= entries_per_cluster) {
                current_cluster = fat32_get_next_cluster(current_cluster);
                entry_index = 0;
            }
            continue;
        }
        
        /* Valid entry found */
        current_entry = *entry;
        
        /* Advance to next entry */
        entry_index++;
        if (entry_index >= entries_per_cluster) {
            current_cluster = fat32_get_next_cluster(current_cluster);
            entry_index = 0;
        }
        
        /* Update directory handle */
        dir->cluster = current_cluster;
        dir->entry_index = entry_index;
        
        return &current_entry;
    }
    
    return NULL;
}

/* Convert FAT 8.3 filename to normal string */
void fat32_convert_filename(const char* input, char* output) {
    int out_pos = 0;
    
    /* Copy the base name (8 characters) */
    for (int i = 0; i < 8 && input[i] != ' '; i++) {
        output[out_pos++] = input[i];
    }
    
    /* Add extension if present */
    if (input[8] != ' ') {
        output[out_pos++] = '.';
        for (int i = 8; i < 11 && input[i] != ' '; i++) {
            output[out_pos++] = input[i];
        }
    }
    
    output[out_pos] = '\0';
}

/* Compare two filenames (case insensitive) */
bool fat32_compare_filename(const char* name1, const char* name2) {
    int i = 0;
    while (name1[i] && name2[i]) {
        char c1 = name1[i];
        char c2 = name2[i];
        
        /* Convert to uppercase for comparison */
        if (c1 >= 'a' && c1 <= 'z') c1 -= 32;
        if (c2 >= 'a' && c2 <= 'z') c2 -= 32;
        
        if (c1 != c2) {
            return false;
        }
        i++;
    }
    
    return name1[i] == name2[i];  /* Both should be null terminators */
}

/* Print file information */
void fat32_print_file_info(const fat32_dir_entry_t* entry) {
    if (!entry) return;
    
    char filename[13];
    fat32_convert_filename((char*)entry->name, filename);
    
    terminal_writestring("File: ");
    terminal_writestring(filename);
    terminal_writestring(" Size: ");
    
    /* Print file size */
    char size_str[16];
    uint32_t size = entry->file_size;
    int digits = 0;
    
    if (size == 0) {
        size_str[0] = '0';
        digits = 1;
    } else {
        while (size > 0) {
            size_str[digits++] = '0' + (size % 10);
            size /= 10;
        }
    }
    
    /* Reverse the string */
    for (int i = 0; i < digits / 2; i++) {
        char temp = size_str[i];
        size_str[i] = size_str[digits - 1 - i];
        size_str[digits - 1 - i] = temp;
    }
    size_str[digits] = '\0';
    
    terminal_writestring(size_str);
    terminal_writestring(" bytes");
    
    if (entry->attributes & FAT_ATTR_DIRECTORY) {
        terminal_writestring(" [DIR]");
    }
    
    terminal_writestring("\n");
}

/* Get file system information */
fat32_fs_info_t* fat32_get_fs_info(void) {
    return fs_info.initialized ? &fs_info : NULL;
}
