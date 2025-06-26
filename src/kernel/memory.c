/*------------------------------------------------------------------------------
 * Physical Memory Manager
 *------------------------------------------------------------------------------
 * This module implements a bitmap-based physical memory allocator for the 
 * SKOS kernel. It detects available memory using GRUB's multiboot information
 * and provides page-granular allocation of physical memory.
 *
 * The allocator uses a bitmap where each bit represents a 4KB page frame.
 * Setting a bit to 1 indicates the page is allocated, 0 indicates free.
 *------------------------------------------------------------------------------
 */

#include "memory.h"
#include "kernel.h"
#include "debug.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Global physical memory allocator */
static physical_allocator_t phys_allocator;

/* Memory map information */
static uint32_t memory_map_entries = 0;
static uint32_t total_memory_kb = 0;
static uint32_t usable_memory_kb = 0;

/* Kernel boundaries (defined in linker script) */
extern uint32_t kernel_start;
extern uint32_t kernel_end;

/* Paging structures */
static page_directory_t *kernel_directory = 0;
static page_table_t *kernel_tables[1024] = {0};

/* Current page directory */
static page_directory_t *current_directory = 0;

/*------------------------------------------------------------------------------
 * Memory detection and initialization
 *------------------------------------------------------------------------------
 */

/**
 * @brief Initialize memory management system
 * @param mboot_info Multiboot information structure from GRUB
 */
void memory_init(multiboot_info_t* mboot_info) {
    /* Initialize physical memory allocator */
    physical_memory_init(mboot_info);
    
    /* Initialize paging */
    paging_init();
    
    /* Print concise memory statistics */
    terminal_writestring(" (");
    
    /* Print total memory in MB */
    uint32_t total_mb = total_memory_kb / 1024;
    char mb_str[16];
    int i = 0;
    if (total_mb == 0) {
        mb_str[i++] = '0';
    } else {
        while (total_mb > 0) {
            mb_str[i++] = '0' + (total_mb % 10);
            total_mb /= 10;
        }
    }
    /* Reverse string */
    for (int j = 0; j < i/2; j++) {
        char temp = mb_str[j];
        mb_str[j] = mb_str[i-1-j];
        mb_str[i-1-j] = temp;
    }
    mb_str[i] = '\0';
    
    terminal_writestring(mb_str);
    terminal_writestring("MB, ");
    
    /* Print used pages */
    uint32_t used = phys_allocator.used_pages;
    char used_str[16];
    i = 0;
    if (used == 0) {
        used_str[i++] = '0';
    } else {
        while (used > 0) {
            used_str[i++] = '0' + (used % 10);
            used /= 10;
        }
    }
    for (int j = 0; j < i/2; j++) {
        char temp = used_str[j];
        used_str[j] = used_str[i-1-j];
        used_str[i-1-j] = temp;
    }
    used_str[i] = '\0';
    terminal_writestring(used_str);
    terminal_writestring(" pages used)");
}

/**
 * @brief Initialize physical memory allocator using multiboot memory map
 * @param mboot_info Multiboot information structure
 */
void physical_memory_init(multiboot_info_t* mboot_info) {
    /* Check if memory map is available */
    if (!(mboot_info->flags & 0x40)) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_RED, VGA_COLOR_BLACK));
        terminal_writestring("ERROR: No memory map available from bootloader!\n");
        while(1) asm volatile("hlt");
    }
    
    /* Calculate total memory from memory map */
    multiboot_memory_map_t* mmap = (multiboot_memory_map_t*)mboot_info->mmap_addr;
    uint32_t mmap_end = mboot_info->mmap_addr + mboot_info->mmap_length;
     uint32_t highest_address = 0;
    
    /* Process memory map without printing details for cleaner output */
    while ((uint32_t)mmap < mmap_end) {
        uint64_t base_addr = ((uint64_t)mmap->base_addr_high << 32) | mmap->base_addr_low;
        uint64_t length = ((uint64_t)mmap->length_high << 32) | mmap->length_low;
        
        /* Only consider memory below 4GB for now */
        if (base_addr < 0x100000000ULL) {
            uint32_t start = (uint32_t)base_addr;
            uint32_t size = (uint32_t)length;
            uint32_t end = start + size;
            
            if (mmap->type == MULTIBOOT_MEMORY_AVAILABLE) {
                if (end > highest_address) {
                    highest_address = end;
                }
                usable_memory_kb += size / 1024;
            }
        }
        
        memory_map_entries++;
        mmap = (multiboot_memory_map_t*)((uint32_t)mmap + mmap->size + sizeof(mmap->size));
    }
    total_memory_kb = highest_address / 1024;
    
    /* Calculate number of pages and bitmap size */
    phys_allocator.total_pages = highest_address / PAGE_SIZE;
    uint32_t bitmap_size = (phys_allocator.total_pages + 31) / 32; /* Round up to nearest uint32_t */
    
    /* Place bitmap after kernel in memory */
    uint32_t kernel_end_addr = (uint32_t)&kernel_end;
    uint32_t bitmap_addr = align_up(kernel_end_addr, sizeof(uint32_t));
    phys_allocator.bitmap = (uint32_t*)bitmap_addr;
    
    /* Clear bitmap (all pages free initially) */
    for (uint32_t i = 0; i < bitmap_size; i++) {
        phys_allocator.bitmap[i] = 0;
    }
    
    /* Mark unavailable regions as used */
    mmap = (multiboot_memory_map_t*)mboot_info->mmap_addr;
    while ((uint32_t)mmap < mmap_end) {
        uint64_t base_addr = ((uint64_t)mmap->base_addr_high << 32) | mmap->base_addr_low;
        uint64_t length = ((uint64_t)mmap->length_high << 32) | mmap->length_low;
        
        if (base_addr < 0x100000000ULL && mmap->type != MULTIBOOT_MEMORY_AVAILABLE) {
            uint32_t start_page = (uint32_t)base_addr / PAGE_SIZE;
            uint32_t num_pages = ((uint32_t)length + PAGE_SIZE - 1) / PAGE_SIZE;
            
            for (uint32_t i = 0; i < num_pages && (start_page + i) < phys_allocator.total_pages; i++) {
                uint32_t page = start_page + i;
                uint32_t bitmap_index = page / 32;
                uint32_t bit_index = page % 32;
                phys_allocator.bitmap[bitmap_index] |= (1 << bit_index);
                phys_allocator.used_pages++;
            }
        }
        
        mmap = (multiboot_memory_map_t*)((uint32_t)mmap + mmap->size + sizeof(mmap->size));
    }
    
    /* Mark kernel and bitmap area as used */
    uint32_t kernel_start_page = 0x100000 / PAGE_SIZE; /* Kernel starts at 1MB */
    uint32_t kernel_end_page = (bitmap_addr + bitmap_size * sizeof(uint32_t) + PAGE_SIZE - 1) / PAGE_SIZE;
    
    for (uint32_t page = kernel_start_page; page < kernel_end_page && page < phys_allocator.total_pages; page++) {
        uint32_t bitmap_index = page / 32;
        uint32_t bit_index = page % 32;
        if (!(phys_allocator.bitmap[bitmap_index] & (1 << bit_index))) {
            phys_allocator.bitmap[bitmap_index] |= (1 << bit_index);
            phys_allocator.used_pages++;
        }
    }
    
    /* Also mark first 1MB as used (BIOS, VGA, etc.) */
    uint32_t first_mb_pages = 0x100000 / PAGE_SIZE;
    for (uint32_t page = 0; page < first_mb_pages && page < phys_allocator.total_pages; page++) {
        uint32_t bitmap_index = page / 32;
        uint32_t bit_index = page % 32;
        if (!(phys_allocator.bitmap[bitmap_index] & (1 << bit_index))) {
            phys_allocator.bitmap[bitmap_index] |= (1 << bit_index);
            phys_allocator.used_pages++;
        }
    }
    
    phys_allocator.first_free_page = first_mb_pages;
}

/*------------------------------------------------------------------------------
 * Physical page allocation functions
 *------------------------------------------------------------------------------
 */

/**
 * @brief Allocate a single physical page
 * @return Physical address of allocated page, or 0 if out of memory
 */
uint32_t allocate_physical_page(void) {
    /* Start searching from the hint */
    for (uint32_t page = phys_allocator.first_free_page; page < phys_allocator.total_pages; page++) {
        uint32_t bitmap_index = page / 32;
        uint32_t bit_index = page % 32;
        
        if (!(phys_allocator.bitmap[bitmap_index] & (1 << bit_index))) {
            /* Found a free page */
            phys_allocator.bitmap[bitmap_index] |= (1 << bit_index);
            phys_allocator.used_pages++;
            
            /* Update hint */
            if (page == phys_allocator.first_free_page) {
                phys_allocator.first_free_page++;
            }
            
            /* Track allocation for profiling */
            debug_count_memory_alloc(PAGE_SIZE);
            
            return page * PAGE_SIZE;
        }
    }
    
    /* No free pages found */
    return 0;
}

/**
 * @brief Free a physical page
 * @param page_addr Physical address of page to free (must be page-aligned)
 */
void free_physical_page(uint32_t page_addr) {
    if (page_addr % PAGE_SIZE != 0) {
        return; /* Invalid address */
    }
    
    uint32_t page = page_addr / PAGE_SIZE;
    if (page >= phys_allocator.total_pages) {
        return; /* Out of range */
    }
    
    uint32_t bitmap_index = page / 32;
    uint32_t bit_index = page % 32;
    
    if (phys_allocator.bitmap[bitmap_index] & (1 << bit_index)) {
        /* Page was allocated, now free it */
        phys_allocator.bitmap[bitmap_index] &= ~(1 << bit_index);
        phys_allocator.used_pages--;
        
        /* Track deallocation for profiling */
        debug_count_memory_free(PAGE_SIZE);
        
        /* Update hint if this page is before current hint */
        if (page < phys_allocator.first_free_page) {
            phys_allocator.first_free_page = page;
        }
    }
}

/**
 * @brief Get total physical memory in bytes
 */
uint32_t get_total_memory(void) {
    return total_memory_kb * 1024;
}

/**
 * @brief Get used physical memory in bytes
 */
uint32_t get_used_memory(void) {
    return phys_allocator.used_pages * PAGE_SIZE;
}

/**
 * @brief Get free physical memory in bytes
 */
uint32_t get_free_memory(void) {
    return (phys_allocator.total_pages - phys_allocator.used_pages) * PAGE_SIZE;
}

/*------------------------------------------------------------------------------
 * Paging implementation
 *------------------------------------------------------------------------------
 */

/**
 * @brief Initialize paging system
 */
void paging_init(void) {
    /* Allocate page directory */
    uint32_t phys_addr = allocate_physical_page();
    if (!phys_addr) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_RED, VGA_COLOR_BLACK));
        terminal_writestring("ERROR: Cannot allocate page directory!\n");
        while(1) asm volatile("hlt");
    }
    
    kernel_directory = (page_directory_t*)phys_addr;
    
    /* Clear page directory */
    for (int i = 0; i < 1024; i++) {
        kernel_directory->tables[i] = 0;
    }
    
    /* Identity map first 4MB (for kernel) */
    uint32_t page_table_phys = allocate_physical_page();
    if (!page_table_phys) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_RED, VGA_COLOR_BLACK));
        terminal_writestring("ERROR: Cannot allocate page table!\n");
        while(1) asm volatile("hlt");
    }
    
    page_table_t *page_table = (page_table_t*)page_table_phys;
    kernel_tables[0] = page_table;
    
    /* Clear page table */
    for (int i = 0; i < 1024; i++) {
        page_table->pages[i] = 0;
    }
    
    /* Map first 4MB identity (0x0 -> 0x0) */
    for (uint32_t i = 0; i < 1024; i++) {
        uint32_t phys = i * PAGE_SIZE;
        page_table->pages[i] = phys | PAGE_PRESENT | PAGE_WRITABLE;
    }
    
    /* Add page table to directory */
    kernel_directory->tables[0] = page_table_phys | PAGE_PRESENT | PAGE_WRITABLE;
    
    /* Set page directory and enable paging */
    current_directory = kernel_directory;
    
    /* Load page directory into CR3 */
    asm volatile("mov %0, %%cr3" :: "r"(kernel_directory));
    
    /* Enable paging */
    uint32_t cr0;
    asm volatile("mov %%cr0, %0" : "=r"(cr0));
    cr0 |= 0x80000000; /* Set PG bit */
    asm volatile("mov %0, %%cr0" :: "r"(cr0));
}

/**
 * @brief Map a virtual page to a physical page
 * @param virtual_addr Virtual address (will be page-aligned)
 * @param physical_addr Physical address (will be page-aligned)
 * @param flags Page flags (present, writable, user, etc.)
 */
void map_page(uint32_t virtual_addr, uint32_t physical_addr, uint32_t flags) {
    /* Align addresses to page boundaries */
    virtual_addr &= PAGE_ALIGN_MASK;
    physical_addr &= PAGE_ALIGN_MASK;
    
    /* Get page directory and table indices */
    uint32_t pd_index = virtual_addr >> 22;
    uint32_t pt_index = (virtual_addr >> 12) & 0x3FF;
    
    /* Check if page table exists */
    if (!(kernel_directory->tables[pd_index] & PAGE_PRESENT)) {
        /* Allocate new page table */
        uint32_t page_table_phys = allocate_physical_page();
        if (!page_table_phys) {
            return; /* Out of memory */
        }
        
        page_table_t *new_table = (page_table_t*)page_table_phys;
        kernel_tables[pd_index] = new_table;
        
        /* Clear new page table */
        for (int i = 0; i < 1024; i++) {
            new_table->pages[i] = 0;
        }
        
        /* Add to page directory */
        kernel_directory->tables[pd_index] = page_table_phys | PAGE_PRESENT | PAGE_WRITABLE | (flags & PAGE_USER);
    }
    
    /* Get page table */
    page_table_t *table = kernel_tables[pd_index];
    
    /* Set page table entry */
    table->pages[pt_index] = physical_addr | flags;
    
    /* Flush TLB for this page */
    asm volatile("invlpg (%0)" :: "r"(virtual_addr) : "memory");
}

/**
 * @brief Unmap a virtual page
 * @param virtual_addr Virtual address to unmap
 */
void unmap_page(uint32_t virtual_addr) {
    virtual_addr &= PAGE_ALIGN_MASK;
    
    uint32_t pd_index = virtual_addr >> 22;
    uint32_t pt_index = (virtual_addr >> 12) & 0x3FF;
    
    if (kernel_directory->tables[pd_index] & PAGE_PRESENT) {
        page_table_t *table = kernel_tables[pd_index];
        table->pages[pt_index] = 0;
        
        /* Flush TLB */
        asm volatile("invlpg (%0)" :: "r"(virtual_addr) : "memory");
    }
}

/**
 * @brief Get physical address for a virtual address
 * @param virtual_addr Virtual address to translate
 * @return Physical address, or 0 if not mapped
 */
uint32_t get_physical_address(uint32_t virtual_addr) {
    uint32_t pd_index = virtual_addr >> 22;
    uint32_t pt_index = (virtual_addr >> 12) & 0x3FF;
    uint32_t offset = virtual_addr & PAGE_OFFSET_MASK;
    
    if (!(kernel_directory->tables[pd_index] & PAGE_PRESENT)) {
        return 0;
    }
    
    page_table_t *table = kernel_tables[pd_index];
    if (!(table->pages[pt_index] & PAGE_PRESENT)) {
        return 0;
    }
    
    return (table->pages[pt_index] & PAGE_ALIGN_MASK) + offset;
}

/**
 * @brief Check if a page is present
 * @param virtual_addr Virtual address to check
 * @return true if page is present, false otherwise
 */
bool is_page_present(uint32_t virtual_addr) {
    return get_physical_address(virtual_addr) != 0;
}

/**
 * @brief Handle page faults
 * @param error_code Error code from page fault interrupt
 */
void page_fault_handler(uint32_t error_code) {
    /* Get faulting address from CR2 */
    uint32_t fault_addr;
    asm volatile("mov %%cr2, %0" : "=r"(fault_addr));
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_RED, VGA_COLOR_BLACK));
    terminal_writestring("PAGE FAULT! Error code: ");
    
    /* Print error details */
    if (error_code & 0x1) {
        terminal_writestring("Page protection violation ");
    } else {
        terminal_writestring("Page not present ");
    }
    
    if (error_code & 0x2) {
        terminal_writestring("(write) ");
    } else {
        terminal_writestring("(read) ");
    }
    
    if (error_code & 0x4) {
        terminal_writestring("(user mode)");
    } else {
        terminal_writestring("(kernel mode)");
    }
    
    terminal_writestring("\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    
    /* For now, halt system */
    while(1) asm volatile("hlt");
}

/*------------------------------------------------------------------------------
 * Utility functions
 *------------------------------------------------------------------------------
 */

/**
 * @brief Align address up to specified alignment
 */
uint32_t align_up(uint32_t addr, uint32_t alignment) {
    return (addr + alignment - 1) & ~(alignment - 1);
}

/**
 * @brief Align address down to specified alignment
 */
uint32_t align_down(uint32_t addr, uint32_t alignment) {
    return addr & ~(alignment - 1);
}

/**
 * @brief Convert virtual address to physical (identity mapping for now)
 */
uint32_t virtual_to_physical(uint32_t virtual_addr) {
    return get_physical_address(virtual_addr);
}

/**
 * @brief Convert physical address to virtual (identity mapping for now)
 */
uint32_t physical_to_virtual(uint32_t physical_addr) {
    return physical_addr; /* Identity mapping */
}

/*------------------------------------------------------------------------------
 * Debug functions
 *------------------------------------------------------------------------------
 */

/**
 * @brief Print memory statistics
 */
void memory_print_stats(void) {
    terminal_writestring("\nMemory Statistics:\n");
    terminal_writestring("  Total memory: ");
    
    /* Simple number to string conversion for total memory in MB */
    uint32_t total_mb = total_memory_kb / 1024;
    char mb_str[16];
    int i = 0;
    if (total_mb == 0) {
        mb_str[i++] = '0';
    } else {
        while (total_mb > 0) {
            mb_str[i++] = '0' + (total_mb % 10);
            total_mb /= 10;
        }
    }
    /* Reverse string */
    for (int j = 0; j < i/2; j++) {
        char temp = mb_str[j];
        mb_str[j] = mb_str[i-1-j];
        mb_str[i-1-j] = temp;
    }
    mb_str[i] = '\0';
    
    terminal_writestring(mb_str);
    terminal_writestring(" MB\n");
    
    terminal_writestring("  Used pages: ");
    uint32_t used = phys_allocator.used_pages;
    char used_str[16];
    i = 0;
    if (used == 0) {
        used_str[i++] = '0';
    } else {
        while (used > 0) {
            used_str[i++] = '0' + (used % 10);
            used /= 10;
        }
    }
    for (int j = 0; j < i/2; j++) {
        char temp = used_str[j];
        used_str[j] = used_str[i-1-j];
        used_str[i-1-j] = temp;
    }
    used_str[i] = '\0';
    terminal_writestring(used_str);
    
    terminal_writestring(" / ");
    
    uint32_t total = phys_allocator.total_pages;
    char total_str[16];
    i = 0;
    if (total == 0) {
        total_str[i++] = '0';
    } else {
        while (total > 0) {
            total_str[i++] = '0' + (total % 10);
            total /= 10;
        }
    }
    for (int j = 0; j < i/2; j++) {
        char temp = total_str[j];
        total_str[j] = total_str[i-1-j];
        total_str[i-1-j] = temp;
    }
    total_str[i] = '\0';
    terminal_writestring(total_str);
    terminal_writestring("\n");
}

/**
 * @brief Print memory map (simplified)
 */
void memory_print_map(void) {
    terminal_writestring("Memory map entries: ");
    char entries_str[16];
    uint32_t entries = memory_map_entries;
    int i = 0;
    if (entries == 0) {
        entries_str[i++] = '0';
    } else {
        while (entries > 0) {
            entries_str[i++] = '0' + (entries % 10);
            entries /= 10;
        }
    }
    for (int j = 0; j < i/2; j++) {
        char temp = entries_str[j];
        entries_str[j] = entries_str[i-1-j];
        entries_str[i-1-j] = temp;
    }
    entries_str[i] = '\0';
    terminal_writestring(entries_str);
    terminal_writestring("\n");
}
