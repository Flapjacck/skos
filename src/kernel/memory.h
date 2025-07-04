#ifndef MEMORY_H
#define MEMORY_H

#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

/* Memory layout constants */
#define PAGE_SIZE        4096        /* Standard page size: 4 KiB */
#define PAGE_ALIGN_MASK  0xFFFFF000  /* Mask to align to page boundary */
#define PAGE_OFFSET_MASK 0x00000FFF  /* Mask to get offset within page */

/* Virtual memory layout */
#define KERNEL_VIRTUAL_BASE   0xC0000000  /* 3GB - where kernel lives in virtual memory */
#define KERNEL_PAGE_NUMBER    (KERNEL_VIRTUAL_BASE >> 22)  /* Page directory index */

/* Page flags */
#define PAGE_PRESENT     0x1    /* Page is present in memory */
#define PAGE_WRITABLE    0x2    /* Page is writable */
#define PAGE_USER        0x4    /* Page is accessible from user mode */
#define PAGE_WRITETHROUGH 0x8   /* Write-through caching */
#define PAGE_NOCACHE     0x10   /* Cache disabled */
#define PAGE_ACCESSED    0x20   /* Set by processor when page is accessed */
#define PAGE_DIRTY       0x40   /* Set by processor when page is written to */

/* Memory types from multiboot */
#define MULTIBOOT_MEMORY_AVAILABLE        1
#define MULTIBOOT_MEMORY_RESERVED         2
#define MULTIBOOT_MEMORY_ACPI_RECLAIMABLE 3
#define MULTIBOOT_MEMORY_NVS              4
#define MULTIBOOT_MEMORY_BADRAM           5

/* Multiboot structures */
typedef struct multiboot_memory_map {
    uint32_t size;
    uint32_t base_addr_low;
    uint32_t base_addr_high;
    uint32_t length_low;
    uint32_t length_high;
    uint32_t type;
} __attribute__((packed)) multiboot_memory_map_t;

typedef struct multiboot_info {
    uint32_t flags;
    uint32_t mem_lower;
    uint32_t mem_upper;
    uint32_t boot_device;
    uint32_t cmdline;
    uint32_t mods_count;
    uint32_t mods_addr;
    uint32_t num;
    uint32_t size;
    uint32_t addr;
    uint32_t shndx;
    uint32_t mmap_length;
    uint32_t mmap_addr;
    /* ... other fields not needed for basic memory management */
} __attribute__((packed)) multiboot_info_t;

/* Physical memory allocator */
typedef struct {
    uint32_t *bitmap;           /* Bitmap of free/used pages */
    uint32_t total_pages;       /* Total number of pages */
    uint32_t used_pages;        /* Number of used pages */
    uint32_t first_free_page;   /* Hint for first potentially free page */
} physical_allocator_t;

/* Page directory and page table structures */
typedef struct {
    uint32_t pages[1024];
} page_table_t;

typedef struct {
    uint32_t tables[1024];
} page_directory_t;

/* Memory management initialization */
void memory_init(multiboot_info_t* mboot_info);

/* Physical memory allocator functions */
void physical_memory_init(multiboot_info_t* mboot_info);
uint32_t allocate_physical_page(void);
void free_physical_page(uint32_t page_addr);
uint32_t get_total_memory(void);
uint32_t get_used_memory(void);
uint32_t get_free_memory(void);

/* Paging functions */
void paging_init(void);
void map_page(uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags);
void unmap_page(uint32_t virtual_addr);
uint32_t get_physical_address(uint32_t virtual_addr);
bool is_page_present(uint32_t virtual_addr);

/* Page fault handler */
void page_fault_handler(uint32_t error_code);

/* Utility functions */
uint32_t align_up(uint32_t addr, uint32_t alignment);
uint32_t align_down(uint32_t addr, uint32_t alignment);
uint32_t virtual_to_physical(uint32_t virtual_addr);
uint32_t physical_to_virtual(uint32_t physical_addr);

/* Memory debugging functions */
void memory_print_stats(void);
void memory_print_map(void);

/* Heap allocator constants */
#define HEAP_START_ADDR     0xC0400000  /* Start heap at 4MB in kernel space */
#define HEAP_INITIAL_SIZE   0x100000    /* Initial heap size: 1MB */
#define HEAP_MAX_SIZE       0x1000000   /* Maximum heap size: 16MB */
#define HEAP_BLOCK_MAGIC    0xDEADBEEF  /* Magic number for heap blocks */

/* Heap block structure */
typedef struct heap_block {
    uint32_t magic;                 /* Magic number for validation */
    uint32_t size;                  /* Size of this block (including header) */
    bool is_free;                   /* Whether this block is free */
    struct heap_block* next;        /* Next block in the list */
    struct heap_block* prev;        /* Previous block in the list */
} __attribute__((packed)) heap_block_t;

/* Heap allocator structure */
typedef struct {
    uint32_t start_addr;            /* Virtual start address of heap */
    uint32_t end_addr;              /* Virtual end address of heap */
    uint32_t size;                  /* Current heap size */
    heap_block_t* first_block;      /* First block in the heap */
    bool initialized;               /* Whether heap is initialized */
} heap_info_t;

/* Heap allocator functions */
void heap_init(void);
void* kmalloc(size_t size);
void* kcalloc(size_t count, size_t size);
void* krealloc(void* ptr, size_t size);
void kfree(void* ptr);
size_t heap_get_allocated_size(void* ptr);
void heap_print_stats(void);
void heap_validate(void);

#endif /* MEMORY_H */
