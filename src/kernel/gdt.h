#ifndef GDT_H
#define GDT_H

#include <stdint.h>

/*------------------------------------------------------------------------------
 * Global Descriptor Table (GDT) Implementation
 *------------------------------------------------------------------------------
 * The GDT is a fundamental x86 data structure that defines memory segments
 * and their properties. It's required for proper memory protection and 
 * privilege level management in protected mode.
 * 
 * Each GDT entry (descriptor) is 8 bytes and defines:
 * - Base address: Starting address of the segment
 * - Limit: Size of the segment 
 * - Access rights: Read/write/execute permissions
 * - Privilege level: Ring 0 (kernel) to Ring 3 (user)
 * - Granularity: Byte or 4KB page granularity
 *------------------------------------------------------------------------------
 */

/*------------------------------------------------------------------------------
 * GDT Constants and Definitions
 *------------------------------------------------------------------------------
 */

/* Number of GDT entries we'll define */
#define GDT_ENTRIES 5

/* GDT Entry indices for easy reference */
#define GDT_NULL_SEGMENT    0  /* Required null descriptor */
#define GDT_KERNEL_CODE     1  /* Kernel code segment (Ring 0) */
#define GDT_KERNEL_DATA     2  /* Kernel data segment (Ring 0) */
#define GDT_USER_CODE       3  /* User code segment (Ring 3) */
#define GDT_USER_DATA       4  /* User data segment (Ring 3) */

/* Segment selector values (index << 3 | privilege_level) */
#define KERNEL_CODE_SELECTOR 0x08  /* Index 1, Ring 0 */
#define KERNEL_DATA_SELECTOR 0x10  /* Index 2, Ring 0 */
#define USER_CODE_SELECTOR   0x1B  /* Index 3, Ring 3 */
#define USER_DATA_SELECTOR   0x23  /* Index 4, Ring 3 */

/*------------------------------------------------------------------------------
 * GDT Access Byte Flags
 *------------------------------------------------------------------------------
 * The access byte defines permissions and properties for each segment:
 * 
 * Bit 7: Present (P) - Must be 1 for valid descriptors
 * Bit 6-5: Privilege Level (DPL) - 0=Kernel, 3=User
 * Bit 4: Descriptor Type (S) - 1=Code/Data, 0=System
 * Bit 3: Executable (E) - 1=Code segment, 0=Data segment
 * Bit 2: Direction/Conforming (DC) - Growth direction for data, conforming for code
 * Bit 1: Read/Write (RW) - Read for code, write for data
 * Bit 0: Accessed (A) - Set by CPU when segment is accessed
 *------------------------------------------------------------------------------
 */

/* Base access flags */
#define GDT_ACCESS_PRESENT    0x80  /* Segment is present in memory */
#define GDT_ACCESS_RING0      0x00  /* Privilege level 0 (kernel) */
#define GDT_ACCESS_RING3      0x60  /* Privilege level 3 (user) */
#define GDT_ACCESS_SEGMENT    0x10  /* Code/data segment (not system) */
#define GDT_ACCESS_EXECUTABLE 0x08  /* Code segment */
#define GDT_ACCESS_READABLE   0x02  /* Code segment is readable */
#define GDT_ACCESS_WRITABLE   0x02  /* Data segment is writable */

/* Combined access bytes for common segment types */
#define GDT_ACCESS_KERNEL_CODE (GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 | \
                               GDT_ACCESS_SEGMENT | GDT_ACCESS_EXECUTABLE | \
                               GDT_ACCESS_READABLE)

#define GDT_ACCESS_KERNEL_DATA (GDT_ACCESS_PRESENT | GDT_ACCESS_RING0 | \
                               GDT_ACCESS_SEGMENT | GDT_ACCESS_WRITABLE)

#define GDT_ACCESS_USER_CODE   (GDT_ACCESS_PRESENT | GDT_ACCESS_RING3 | \
                               GDT_ACCESS_SEGMENT | GDT_ACCESS_EXECUTABLE | \
                               GDT_ACCESS_READABLE)

#define GDT_ACCESS_USER_DATA   (GDT_ACCESS_PRESENT | GDT_ACCESS_RING3 | \
                               GDT_ACCESS_SEGMENT | GDT_ACCESS_WRITABLE)

/*------------------------------------------------------------------------------
 * GDT Granularity Flags
 *------------------------------------------------------------------------------
 * The granularity byte controls segment properties:
 * 
 * Bit 7: Granularity (G) - 0=Byte granularity, 1=4KB page granularity
 * Bit 6: Size (DB) - 0=16-bit segment, 1=32-bit segment
 * Bit 5: Long mode (L) - 1=64-bit segment (not used in 32-bit mode)
 * Bit 4: Available (AVL) - Available for system use
 * Bits 3-0: Upper 4 bits of segment limit
 *------------------------------------------------------------------------------
 */

#define GDT_GRANULARITY_4K    0x80  /* 4KB page granularity */
#define GDT_GRANULARITY_32BIT 0x40  /* 32-bit segment */
#define GDT_GRANULARITY_AVL   0x10  /* Available for system use */

/* Standard granularity for 32-bit flat model */
#define GDT_GRANULARITY_STANDARD (GDT_GRANULARITY_4K | GDT_GRANULARITY_32BIT)

/*------------------------------------------------------------------------------
 * GDT Data Structures
 *------------------------------------------------------------------------------
 */

/**
 * @brief GDT Entry Structure
 * 
 * Represents a single 8-byte GDT descriptor. The layout matches the x86
 * hardware requirements exactly.
 */
struct gdt_entry {
    uint16_t limit_low;     /* Lower 16 bits of segment limit */
    uint16_t base_low;      /* Lower 16 bits of segment base address */
    uint8_t  base_middle;   /* Middle 8 bits of segment base address */
    uint8_t  access;        /* Access byte (permissions and type) */
    uint8_t  granularity;   /* Granularity byte and upper limit bits */
    uint8_t  base_high;     /* Upper 8 bits of segment base address */
} __attribute__((packed));  /* Prevent compiler padding */

/**
 * @brief GDT Pointer Structure
 * 
 * Used by the LGDT instruction to load the GDT. Contains the size and
 * address of the GDT.
 */
struct gdt_ptr {
    uint16_t limit;         /* Size of GDT in bytes minus 1 */
    uint32_t base;          /* Address of the GDT */
} __attribute__((packed));  /* Prevent compiler padding */

/*------------------------------------------------------------------------------
 * GDT Function Declarations
 *------------------------------------------------------------------------------
 */

/**
 * @brief Initializes the Global Descriptor Table
 * 
 * This function:
 * 1. Sets up the null descriptor (required by x86 architecture)
 * 2. Creates kernel code and data segments with Ring 0 privileges
 * 3. Creates user code and data segments with Ring 3 privileges
 * 4. Loads the GDT using the LGDT instruction
 * 5. Updates segment registers to use the new segments
 * 
 * The flat memory model is used where all segments have:
 * - Base address: 0x00000000
 * - Limit: 0xFFFFFFFF (entire 4GB address space)
 * - 4KB granularity
 * - 32-bit operation
 */
void gdt_init(void);

/**
 * @brief Sets up a single GDT entry
 * 
 * @param num Entry index in the GDT (0-4)
 * @param base Base address of the segment
 * @param limit Size of the segment
 * @param access Access byte defining permissions and type
 * @param gran Granularity byte defining size and properties
 */
void gdt_set_gate(int32_t num, uint32_t base, uint32_t limit, 
                  uint8_t access, uint8_t gran);

/**
 * @brief Assembly function to flush segment registers
 * 
 * This function is implemented in assembly and:
 * 1. Loads the new GDT using LGDT
 * 2. Performs a far jump to reload CS (code segment)
 * 3. Updates other segment registers (DS, ES, FS, GS, SS)
 * 
 * @param gdt_ptr Pointer to the GDT pointer structure
 */
extern void gdt_flush(uint32_t gdt_ptr);

#endif /* GDT_H */
