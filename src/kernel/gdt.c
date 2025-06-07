/*------------------------------------------------------------------------------
 * Global Descriptor Table (GDT) Implementation
 *------------------------------------------------------------------------------
 * This file implements the Global Descriptor Table setup for the SKOS kernel.
 * The GDT is essential for x86 protected mode operation and memory protection.
 * 
 * We implement a flat memory model where:
 * - All segments cover the entire 4GB address space
 * - Segmentation is minimal (only for privilege separation)
 * - Real memory protection comes from paging (to be implemented later)
 *------------------------------------------------------------------------------
 */

#include "gdt.h"

/*------------------------------------------------------------------------------
 * GDT Global Variables
 *------------------------------------------------------------------------------
 */

/* The actual GDT array - 5 entries of 8 bytes each */
static struct gdt_entry gdt_entries[GDT_ENTRIES];

/* GDT pointer structure for LGDT instruction */
static struct gdt_ptr gdt_pointer;

/*------------------------------------------------------------------------------
 * GDT Implementation Functions
 *------------------------------------------------------------------------------
 */

/**
 * @brief Sets up a single GDT entry with specified parameters
 * 
 * This function configures one entry in the GDT by breaking down the
 * 32-bit base address and limit into the required bit fields of the
 * 8-byte GDT descriptor format.
 * 
 * @param num Entry number (0-4) to configure
 * @param base 32-bit base address of the segment
 * @param limit 32-bit limit (size) of the segment
 * @param access Access byte containing permissions and segment type
 * @param gran Granularity byte containing size flags and upper limit bits
 */
void gdt_set_gate(int32_t num, uint32_t base, uint32_t limit, 
                  uint8_t access, uint8_t gran)
{
    /* Validate entry number to prevent buffer overflow */
    if (num < 0 || num >= GDT_ENTRIES) {
        return; /* Invalid entry number, ignore silently */
    }

    /*
     * Set the base address (split across 3 fields):
     * - base_low: bits 0-15 of base address
     * - base_middle: bits 16-23 of base address  
     * - base_high: bits 24-31 of base address
     */
    gdt_entries[num].base_low    = (base & 0xFFFF);        /* Lower 16 bits */
    gdt_entries[num].base_middle = (base >> 16) & 0xFF;    /* Middle 8 bits */
    gdt_entries[num].base_high   = (base >> 24) & 0xFF;    /* Upper 8 bits */

    /*
     * Set the segment limit (split across 2 fields):
     * - limit_low: bits 0-15 of limit
     * - granularity: bits 16-19 of limit (stored in upper 4 bits)
     */
    gdt_entries[num].limit_low   = (limit & 0xFFFF);       /* Lower 16 bits */
    
    /*
     * Set granularity byte:
     * - Upper 4 bits: bits 16-19 of the limit
     * - Lower 4 bits: granularity flags (4K pages, 32-bit, etc.)
     */
    gdt_entries[num].granularity = (limit >> 16) & 0x0F;   /* Upper limit bits */
    gdt_entries[num].granularity |= gran & 0xF0;           /* Granularity flags */

    /* Set the access byte (permissions, privilege level, segment type) */
    gdt_entries[num].access = access;
}

/**
 * @brief Initializes the Global Descriptor Table
 * 
 * This function sets up a standard flat memory model GDT with 5 entries:
 * 1. Null descriptor (required by x86 architecture)
 * 2. Kernel code segment (Ring 0, executable, readable)
 * 3. Kernel data segment (Ring 0, writable)
 * 4. User code segment (Ring 3, executable, readable)
 * 5. User data segment (Ring 3, writable)
 * 
 * All segments use:
 * - Base address: 0x00000000 (start of memory)
 * - Limit: 0xFFFFFFFF (entire 4GB address space)
 * - 4KB granularity (limit is in 4KB pages, not bytes)
 * - 32-bit operation
 */
void gdt_init(void)
{
    /*
     * Set up the GDT pointer structure for LGDT instruction:
     * - limit: Size of GDT in bytes minus 1
     * - base: Physical address of the GDT
     */
    gdt_pointer.limit = (sizeof(struct gdt_entry) * GDT_ENTRIES) - 1;
    gdt_pointer.base  = (uint32_t)&gdt_entries;

    /*
     * Entry 0: Null Descriptor
     * The x86 architecture requires the first GDT entry to be null.
     * Any attempt to use this selector will cause a General Protection Fault.
     */
    gdt_set_gate(GDT_NULL_SEGMENT, 0, 0, 0, 0);

    /*
     * Entry 1: Kernel Code Segment (Ring 0)
     * - Base: 0x00000000 (start of memory)
     * - Limit: 0xFFFFFFFF (entire 4GB space, with 4KB granularity = 4GB * 4KB)
     * - Access: Present, Ring 0, Code segment, Executable, Readable
     * - Granularity: 4KB pages, 32-bit segment
     */
    gdt_set_gate(GDT_KERNEL_CODE, 
                 0x00000000,                    /* Base address */
                 0xFFFFFFFF,                    /* Limit (4GB) */
                 GDT_ACCESS_KERNEL_CODE,        /* Access byte */
                 GDT_GRANULARITY_STANDARD);     /* Granularity */

    /*
     * Entry 2: Kernel Data Segment (Ring 0)
     * - Base: 0x00000000 (start of memory)
     * - Limit: 0xFFFFFFFF (entire 4GB space)
     * - Access: Present, Ring 0, Data segment, Writable
     * - Granularity: 4KB pages, 32-bit segment
     */
    gdt_set_gate(GDT_KERNEL_DATA,
                 0x00000000,                    /* Base address */
                 0xFFFFFFFF,                    /* Limit (4GB) */
                 GDT_ACCESS_KERNEL_DATA,        /* Access byte */
                 GDT_GRANULARITY_STANDARD);     /* Granularity */

    /*
     * Entry 3: User Code Segment (Ring 3)
     * - Base: 0x00000000 (start of memory)
     * - Limit: 0xFFFFFFFF (entire 4GB space)
     * - Access: Present, Ring 3, Code segment, Executable, Readable
     * - Granularity: 4KB pages, 32-bit segment
     * 
     * Note: Ring 3 is the lowest privilege level for user applications
     */
    gdt_set_gate(GDT_USER_CODE,
                 0x00000000,                    /* Base address */
                 0xFFFFFFFF,                    /* Limit (4GB) */
                 GDT_ACCESS_USER_CODE,          /* Access byte */
                 GDT_GRANULARITY_STANDARD);     /* Granularity */

    /*
     * Entry 4: User Data Segment (Ring 3)
     * - Base: 0x00000000 (start of memory)
     * - Limit: 0xFFFFFFFF (entire 4GB space)
     * - Access: Present, Ring 3, Data segment, Writable
     * - Granularity: 4KB pages, 32-bit segment
     */
    gdt_set_gate(GDT_USER_DATA,
                 0x00000000,                    /* Base address */
                 0xFFFFFFFF,                    /* Limit (4GB) */
                 GDT_ACCESS_USER_DATA,          /* Access byte */
                 GDT_GRANULARITY_STANDARD);     /* Granularity */

    /*
     * Load the new GDT and update segment registers
     * This assembly function will:
     * 1. Execute LGDT to load the new GDT
     * 2. Perform a far jump to reload CS with the new kernel code selector
     * 3. Update DS, ES, FS, GS, and SS with the new kernel data selector
     */
    gdt_flush((uint32_t)&gdt_pointer);
}
