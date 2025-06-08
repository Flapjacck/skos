#ifndef IDT_H
#define IDT_H

#include <stdint.h>

/*------------------------------------------------------------------------------
 * Interrupt Descriptor Table (IDT) Implementation
 *------------------------------------------------------------------------------
 * The IDT is a fundamental x86 data structure that defines interrupt and 
 * exception handlers. It's the protected mode counterpart to the real mode
 * Interrupt Vector Table (IVT).
 * 
 * Each IDT entry (gate descriptor) is 8 bytes and defines:
 * - Handler address: Address of the Interrupt Service Routine (ISR)
 * - Segment selector: Code segment where the handler resides
 * - Gate type: Interrupt gate, trap gate, or task gate
 * - Privilege level: Ring level required to access via INT instruction
 * - Present bit: Whether the descriptor is valid
 *------------------------------------------------------------------------------
 */

/*------------------------------------------------------------------------------
 * IDT Constants and Definitions
 *------------------------------------------------------------------------------
 */

/* Number of IDT entries (standard x86 supports 256 interrupt vectors) */
#define IDT_ENTRIES 256

/* Standard interrupt vector numbers for CPU exceptions */
#define IDT_DIVIDE_ERROR            0   /* #DE - Divide by zero */
#define IDT_DEBUG_EXCEPTION         1   /* #DB - Debug exception */
#define IDT_NMI_INTERRUPT           2   /* NMI - Non-maskable interrupt */
#define IDT_BREAKPOINT              3   /* #BP - Breakpoint (INT3) */
#define IDT_OVERFLOW                4   /* #OF - Overflow (INTO) */
#define IDT_BOUND_RANGE_EXCEEDED    5   /* #BR - BOUND range exceeded */
#define IDT_INVALID_OPCODE          6   /* #UD - Invalid/undefined opcode */
#define IDT_DEVICE_NOT_AVAILABLE    7   /* #NM - Device not available */
#define IDT_DOUBLE_FAULT            8   /* #DF - Double fault */
#define IDT_COPROCESSOR_OVERRUN     9   /* Coprocessor segment overrun */
#define IDT_INVALID_TSS            10   /* #TS - Invalid TSS */
#define IDT_SEGMENT_NOT_PRESENT    11   /* #NP - Segment not present */
#define IDT_STACK_SEGMENT_FAULT    12   /* #SS - Stack segment fault */
#define IDT_GENERAL_PROTECTION     13   /* #GP - General protection fault */
#define IDT_PAGE_FAULT             14   /* #PF - Page fault */
#define IDT_RESERVED_15            15   /* Reserved by Intel */
#define IDT_FPU_ERROR              16   /* #MF - x87 FPU floating-point error */
#define IDT_ALIGNMENT_CHECK        17   /* #AC - Alignment check */
#define IDT_MACHINE_CHECK          18   /* #MC - Machine check */
#define IDT_SIMD_EXCEPTION         19   /* #XM - SIMD floating-point exception */
#define IDT_VIRTUALIZATION_EXCEPTION 20 /* #VE - Virtualization exception */
#define IDT_CONTROL_PROTECTION     21   /* #CP - Control protection exception */

/* Interrupt vectors 22-31 are reserved for future Intel use */
/* Interrupt vectors 32-255 are available for external interrupts */
#define IDT_IRQ_BASE               32   /* Base vector for hardware IRQs */

/*------------------------------------------------------------------------------
 * IDT Gate Type Definitions
 *------------------------------------------------------------------------------
 * The gate type field in the type_attributes byte defines what kind of gate
 * this IDT entry represents:
 * 
 * - Task Gate (0x5): Used for hardware task switching (not commonly used)
 * - 16-bit Interrupt Gate (0x6): For 16-bit interrupt handlers
 * - 16-bit Trap Gate (0x7): For 16-bit trap handlers  
 * - 32-bit Interrupt Gate (0xE): For 32-bit interrupt handlers (most common)
 * - 32-bit Trap Gate (0xF): For 32-bit trap handlers (exceptions)
 *------------------------------------------------------------------------------
 */

#define IDT_GATE_TASK_32           0x5   /* 32-bit Task Gate */
#define IDT_GATE_INTERRUPT_16      0x6   /* 16-bit Interrupt Gate */
#define IDT_GATE_TRAP_16           0x7   /* 16-bit Trap Gate */
#define IDT_GATE_INTERRUPT_32      0xE   /* 32-bit Interrupt Gate */
#define IDT_GATE_TRAP_32           0xF   /* 32-bit Trap Gate */

/*------------------------------------------------------------------------------
 * IDT Access Flags
 *------------------------------------------------------------------------------
 * The type_attributes byte contains several fields:
 * 
 * Bit 7: Present (P) - Must be 1 for valid descriptors
 * Bits 6-5: Descriptor Privilege Level (DPL) - Ring level (0-3)
 * Bit 4: Storage Segment (S) - Always 0 for interrupt/trap gates
 * Bits 3-0: Gate Type - Defines the type of gate (see above)
 *------------------------------------------------------------------------------
 */

#define IDT_FLAG_PRESENT           0x80  /* Present bit (required) */
#define IDT_FLAG_RING0             0x00  /* Privilege level 0 (kernel) */
#define IDT_FLAG_RING1             0x20  /* Privilege level 1 */
#define IDT_FLAG_RING2             0x40  /* Privilege level 2 */
#define IDT_FLAG_RING3             0x60  /* Privilege level 3 (user) */
#define IDT_FLAG_STORAGE_SEGMENT   0x10  /* Storage segment (always 0 for gates) */

/* Combined flags for common gate types */
#define IDT_FLAGS_INTERRUPT_GATE   (IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_GATE_INTERRUPT_32)
#define IDT_FLAGS_TRAP_GATE        (IDT_FLAG_PRESENT | IDT_FLAG_RING0 | IDT_GATE_TRAP_32)
#define IDT_FLAGS_USER_INTERRUPT   (IDT_FLAG_PRESENT | IDT_FLAG_RING3 | IDT_GATE_INTERRUPT_32)

/*------------------------------------------------------------------------------
 * IDT Data Structures
 *------------------------------------------------------------------------------
 */

/**
 * @brief IDT Entry Structure (32-bit)
 * 
 * Represents a single 8-byte IDT gate descriptor. The layout matches the x86
 * hardware requirements exactly for 32-bit protected mode.
 * 
 * Structure layout:
 * +-------------------+-------------------+
 * | Offset 15:0       | Selector 15:0     |  <- Lower 4 bytes
 * +-------------------+-------------------+
 * | Offset 31:16      | Type/Attrs | Zero |  <- Upper 4 bytes  
 * +-------------------+-------------------+
 */
struct idt_entry {
    uint16_t offset_low;        /* Lower 16 bits of handler address */
    uint16_t selector;          /* Code segment selector (from GDT) */
    uint8_t  zero;              /* Unused, must be zero */
    uint8_t  type_attributes;   /* Gate type, DPL, and present bit */
    uint16_t offset_high;       /* Upper 16 bits of handler address */
} __attribute__((packed));      /* Prevent compiler padding */

/**
 * @brief IDT Pointer Structure
 * 
 * Used by the LIDT instruction to load the IDT. Contains the size and
 * address of the IDT. Similar to the GDT pointer structure.
 */
struct idt_ptr {
    uint16_t limit;             /* Size of IDT in bytes minus 1 */
    uint32_t base;              /* Address of the IDT */
} __attribute__((packed));      /* Prevent compiler padding */

/*------------------------------------------------------------------------------
 * IDT Function Declarations
 *------------------------------------------------------------------------------
 */

/**
 * @brief Initializes the Interrupt Descriptor Table
 * 
 * This function:
 * 1. Sets up the IDT with default handlers for all 256 interrupt vectors
 * 2. Installs specific handlers for CPU exceptions (0-31)
 * 3. Sets up default handlers for hardware interrupts (32-255)
 * 4. Loads the IDT using the LIDT instruction
 * 
 * The IDT uses flat memory model with:
 * - All handlers in kernel code segment (from GDT)
 * - Interrupt gates for hardware interrupts (disable interrupts on entry)
 * - Trap gates for CPU exceptions (preserve interrupt flag)
 * - Ring 0 privilege for all handlers initially
 */
void idt_init(void);

/**
 * @brief Sets up a single IDT entry (gate)
 * 
 * This function configures one entry in the IDT by setting up the
 * handler address, code segment selector, and gate attributes.
 * 
 * @param num Interrupt vector number (0-255)
 * @param handler Address of the interrupt service routine
 * @param selector Code segment selector (usually kernel code from GDT)
 * @param flags Gate type and privilege flags
 */
void idt_set_gate(uint8_t num, uint32_t handler, uint16_t selector, uint8_t flags);

/**
 * @brief Assembly function to load the IDT
 * 
 * This function is implemented in assembly and uses the LIDT instruction
 * to load the new IDT into the CPU's IDTR register.
 * 
 * @param idt_ptr Pointer to the IDT pointer structure
 */
extern void idt_flush(uint32_t idt_ptr);

/*------------------------------------------------------------------------------
 * Default Interrupt Service Routines (ISRs)
 *------------------------------------------------------------------------------
 * These are the default handlers for CPU exceptions and interrupts.
 * They are implemented in assembly (idt.asm) and provide basic
 * interrupt handling functionality.
 *------------------------------------------------------------------------------
 */

/* CPU Exception Handlers (0-31) */
extern void isr0(void);   /* Divide Error */
extern void isr1(void);   /* Debug Exception */
extern void isr2(void);   /* NMI Interrupt */
extern void isr3(void);   /* Breakpoint */
extern void isr4(void);   /* Overflow */
extern void isr5(void);   /* BOUND Range Exceeded */
extern void isr6(void);   /* Invalid Opcode */
extern void isr7(void);   /* Device Not Available */
extern void isr8(void);   /* Double Fault */
extern void isr9(void);   /* Coprocessor Segment Overrun */
extern void isr10(void);  /* Invalid TSS */
extern void isr11(void);  /* Segment Not Present */
extern void isr12(void);  /* Stack Segment Fault */
extern void isr13(void);  /* General Protection Fault */
extern void isr14(void);  /* Page Fault */
extern void isr15(void);  /* Reserved */
extern void isr16(void);  /* x87 FPU Error */
extern void isr17(void);  /* Alignment Check */
extern void isr18(void);  /* Machine Check */
extern void isr19(void);  /* SIMD Floating-Point Exception */
extern void isr20(void);  /* Virtualization Exception */
extern void isr21(void);  /* Control Protection Exception */
extern void isr22(void);  /* Reserved */
extern void isr23(void);  /* Reserved */
extern void isr24(void);  /* Reserved */
extern void isr25(void);  /* Reserved */
extern void isr26(void);  /* Reserved */
extern void isr27(void);  /* Reserved */
extern void isr28(void);  /* Reserved */
extern void isr29(void);  /* Reserved */
extern void isr30(void);  /* Reserved */
extern void isr31(void);  /* Reserved */

/* Hardware Interrupt Handlers (32-47) - Standard PC IRQs */
extern void irq0(void);   /* Timer Interrupt */
extern void irq1(void);   /* Keyboard Interrupt */
extern void irq2(void);   /* Cascade (used internally by PICs) */
extern void irq3(void);   /* COM2 */
extern void irq4(void);   /* COM1 */
extern void irq5(void);   /* LPT2 */
extern void irq6(void);   /* Floppy Disk */
extern void irq7(void);   /* LPT1 / Unreliable "spurious" interrupt */
extern void irq8(void);   /* CMOS real-time clock */
extern void irq9(void);   /* Free for peripherals / legacy SCSI / NIC */
extern void irq10(void);  /* Free for peripherals / SCSI / NIC */
extern void irq11(void);  /* Free for peripherals / SCSI / NIC */
extern void irq12(void);  /* PS2 Mouse */
extern void irq13(void);  /* FPU / Coprocessor / Inter-processor */
extern void irq14(void);  /* Primary ATA Hard Disk */
extern void irq15(void);  /* Secondary ATA Hard Disk */

/**
 * @brief Common interrupt handler
 * 
 * This function is called by all ISRs after they've saved the processor
 * state. It provides a common entry point for interrupt handling in C.
 * 
 * @param registers Pointer to saved processor state
 */
struct interrupt_registers {
    uint32_t ds;                                    /* Data segment selector */
    uint32_t edi, esi, ebp, esp, ebx, edx, ecx, eax; /* Pushed by pusha */
    uint32_t int_no, err_code;                      /* Interrupt number and error code */
    uint32_t eip, cs, eflags, useresp, ss;          /* Pushed by the processor automatically */
};

typedef struct interrupt_registers interrupt_registers_t;

/**
 * @brief Common interrupt handler function
 * 
 * This function is called by the assembly ISR stubs and provides
 * a central point for handling all interrupts and exceptions.
 * 
 * @param regs Pointer to the saved processor state
 */
void interrupt_handler(interrupt_registers_t *regs);

#endif /* IDT_H */
