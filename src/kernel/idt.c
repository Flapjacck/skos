/*------------------------------------------------------------------------------
 * Interrupt Descriptor Table (IDT) Implementation
 *------------------------------------------------------------------------------
 * This file implements the Interrupt Descriptor Table setup for the SKOS kernel.
 * The IDT is essential for handling CPU exceptions and hardware interrupts
 * in x86 protected mode.
 * 
 * We implement a comprehensive IDT that covers:
 * - All 32 CPU exception vectors (0-31)
 * - Hardware interrupt vectors (32-255) for IRQs
 * - Default handlers that provide debug information
 * - Framework for adding custom interrupt handlers
 *------------------------------------------------------------------------------
 */

#include "idt.h"
#include "gdt.h"     /* For KERNEL_CODE_SELECTOR */
#include "kernel.h"  /* For terminal output functions */
#include "pic.h"     /* For PIC EOI handling */

/*------------------------------------------------------------------------------
 * IDT Global Variables
 *------------------------------------------------------------------------------
 */

/* The actual IDT array - 256 entries of 8 bytes each */
static struct idt_entry idt_entries[IDT_ENTRIES];

/* IDT pointer structure for LIDT instruction */
static struct idt_ptr idt_pointer;

/*------------------------------------------------------------------------------
 * Exception Names for Debug Output
 *------------------------------------------------------------------------------
 * These strings provide human-readable names for CPU exceptions,
 * making debugging much easier when exceptions occur.
 *------------------------------------------------------------------------------
 */

static const char* exception_messages[] = {
    "Division By Zero",                 /* 0:  #DE */
    "Debug Exception",                  /* 1:  #DB */
    "Non Maskable Interrupt",           /* 2:  NMI */
    "Breakpoint Exception",             /* 3:  #BP */
    "Into Detected Overflow",           /* 4:  #OF */
    "Out of Bounds Exception",          /* 5:  #BR */
    "Invalid Opcode Exception",         /* 6:  #UD */
    "No Coprocessor Exception",         /* 7:  #NM */
    "Double Fault",                     /* 8:  #DF */
    "Coprocessor Segment Overrun",      /* 9:  Legacy */
    "Bad TSS",                          /* 10: #TS */
    "Segment Not Present",              /* 11: #NP */
    "Stack Fault",                      /* 12: #SS */
    "General Protection Fault",         /* 13: #GP */
    "Page Fault",                       /* 14: #PF */
    "Unknown Interrupt Exception",      /* 15: Reserved */
    "Coprocessor Fault",                /* 16: #MF */
    "Alignment Check Exception",        /* 17: #AC */
    "Machine Check Exception",          /* 18: #MC */
    "SIMD Floating-Point Exception",    /* 19: #XM */
    "Virtualization Exception",         /* 20: #VE */
    "Control Protection Exception",     /* 21: #CP */
    "Reserved",                         /* 22: Reserved */
    "Reserved",                         /* 23: Reserved */
    "Reserved",                         /* 24: Reserved */
    "Reserved",                         /* 25: Reserved */
    "Reserved",                         /* 26: Reserved */
    "Reserved",                         /* 27: Reserved */
    "Reserved",                         /* 28: Reserved */
    "Reserved",                         /* 29: Reserved */
    "Reserved",                         /* 30: Reserved */
    "Reserved"                          /* 31: Reserved */
};

/*------------------------------------------------------------------------------
 * IDT Implementation Functions
 *------------------------------------------------------------------------------
 */

/**
 * @brief Sets up a single IDT entry with specified parameters
 * 
 * This function configures one entry in the IDT by setting up the
 * interrupt handler address, code segment selector, and gate flags.
 * The 32-bit handler address is split across two 16-bit fields.
 *  * @param num Interrupt vector number (0-255)
 * @param handler Address of the interrupt service routine
 * @param selector Code segment selector (usually KERNEL_CODE_SELECTOR)
 * @param flags Gate type and access flags
 */
void idt_set_gate(int num, uint32_t handler, uint16_t selector, uint8_t flags)
{
    /* Validate vector number to prevent buffer overflow */
    if (num < 0 || num >= IDT_ENTRIES) {
        return; /* Invalid vector number, ignore silently */
    }

    /*
     * Set the handler address (split across 2 fields):
     * - offset_low: bits 0-15 of handler address
     * - offset_high: bits 16-31 of handler address
     */
    idt_entries[num].offset_low  = handler & 0xFFFF;        /* Lower 16 bits */
    idt_entries[num].offset_high = (handler >> 16) & 0xFFFF; /* Upper 16 bits */

    /*
     * Set the code segment selector:
     * This must point to a valid code segment in the GDT.
     * Typically this is the kernel code segment selector.
     */
    idt_entries[num].selector = selector;

    /*
     * Set the reserved field to zero:
     * This field is unused and must always be zero according to Intel specs.
     */
    idt_entries[num].zero = 0;

    /*
     * Set the type and attributes:
     * This byte contains the gate type, privilege level, and present bit.
     * It determines how the CPU handles this interrupt.
     */
    idt_entries[num].type_attributes = flags;
}

/**
 * @brief Initializes the Interrupt Descriptor Table
 * 
 * This function sets up a comprehensive IDT with handlers for all
 * interrupt vectors. It configures:
 * 
 * 1. CPU Exception handlers (vectors 0-31)
 * 2. Hardware IRQ handlers (vectors 32-47)
 * 3. Default handlers for remaining vectors (48-255)
 * 
 * Exception handlers use trap gates (interrupts remain enabled)
 * IRQ handlers use interrupt gates (interrupts disabled on entry)
 */
void idt_init(void)
{
    /*
     * Set up the IDT pointer structure for LIDT instruction:
     * - limit: Size of IDT in bytes minus 1 (256 entries * 8 bytes - 1)
     * - base: Physical address of the IDT
     */
    idt_pointer.limit = (sizeof(struct idt_entry) * IDT_ENTRIES) - 1;
    idt_pointer.base  = (uint32_t)&idt_entries;

    /*
     * Clear the entire IDT first:
     * Initialize all entries to zero to ensure clean state.
     * Invalid entries will generate General Protection Faults if accessed.
     */
    for (int i = 0; i < IDT_ENTRIES; i++) {
        idt_entries[i].offset_low     = 0;
        idt_entries[i].offset_high    = 0;
        idt_entries[i].selector       = 0;
        idt_entries[i].zero           = 0;
        idt_entries[i].type_attributes = 0;
    }

    /*--------------------------------------------------------------------------
     * Install CPU Exception Handlers (Vectors 0-31)
     *--------------------------------------------------------------------------
     * These handle CPU-generated exceptions like divide by zero, page faults,
     * general protection faults, etc. We use trap gates so that interrupts
     * remain enabled during exception handling (except for critical ones).
     *--------------------------------------------------------------------------
     */
    
    /* Vector 0: Divide Error (#DE) */
    idt_set_gate(IDT_DIVIDE_ERROR, (uint32_t)isr0, KERNEL_CODE_SELECTOR, IDT_FLAGS_TRAP_GATE);
    
    /* Vector 1: Debug Exception (#DB) */
    idt_set_gate(IDT_DEBUG_EXCEPTION, (uint32_t)isr1, KERNEL_CODE_SELECTOR, IDT_FLAGS_TRAP_GATE);
    
    /* Vector 2: NMI Interrupt - Use interrupt gate (mask interrupts) */
    idt_set_gate(IDT_NMI_INTERRUPT, (uint32_t)isr2, KERNEL_CODE_SELECTOR, IDT_FLAGS_INTERRUPT_GATE);
    
    /* Vector 3: Breakpoint (#BP) */
    idt_set_gate(IDT_BREAKPOINT, (uint32_t)isr3, KERNEL_CODE_SELECTOR, IDT_FLAGS_TRAP_GATE);
    
    /* Vector 4: Overflow (#OF) */
    idt_set_gate(IDT_OVERFLOW, (uint32_t)isr4, KERNEL_CODE_SELECTOR, IDT_FLAGS_TRAP_GATE);
    
    /* Vector 5: BOUND Range Exceeded (#BR) */
    idt_set_gate(IDT_BOUND_RANGE_EXCEEDED, (uint32_t)isr5, KERNEL_CODE_SELECTOR, IDT_FLAGS_TRAP_GATE);
    
    /* Vector 6: Invalid Opcode (#UD) */
    idt_set_gate(IDT_INVALID_OPCODE, (uint32_t)isr6, KERNEL_CODE_SELECTOR, IDT_FLAGS_TRAP_GATE);
    
    /* Vector 7: Device Not Available (#NM) */
    idt_set_gate(IDT_DEVICE_NOT_AVAILABLE, (uint32_t)isr7, KERNEL_CODE_SELECTOR, IDT_FLAGS_TRAP_GATE);
    
    /* Vector 8: Double Fault (#DF) - Critical, use interrupt gate */
    idt_set_gate(IDT_DOUBLE_FAULT, (uint32_t)isr8, KERNEL_CODE_SELECTOR, IDT_FLAGS_INTERRUPT_GATE);
    
    /* Vector 9: Coprocessor Segment Overrun (Legacy) */
    idt_set_gate(IDT_COPROCESSOR_OVERRUN, (uint32_t)isr9, KERNEL_CODE_SELECTOR, IDT_FLAGS_TRAP_GATE);
    
    /* Vector 10: Invalid TSS (#TS) */
    idt_set_gate(IDT_INVALID_TSS, (uint32_t)isr10, KERNEL_CODE_SELECTOR, IDT_FLAGS_TRAP_GATE);
    
    /* Vector 11: Segment Not Present (#NP) */
    idt_set_gate(IDT_SEGMENT_NOT_PRESENT, (uint32_t)isr11, KERNEL_CODE_SELECTOR, IDT_FLAGS_TRAP_GATE);
    
    /* Vector 12: Stack Segment Fault (#SS) */
    idt_set_gate(IDT_STACK_SEGMENT_FAULT, (uint32_t)isr12, KERNEL_CODE_SELECTOR, IDT_FLAGS_TRAP_GATE);
    
    /* Vector 13: General Protection Fault (#GP) */
    idt_set_gate(IDT_GENERAL_PROTECTION, (uint32_t)isr13, KERNEL_CODE_SELECTOR, IDT_FLAGS_TRAP_GATE);
    
    /* Vector 14: Page Fault (#PF) */
    idt_set_gate(IDT_PAGE_FAULT, (uint32_t)isr14, KERNEL_CODE_SELECTOR, IDT_FLAGS_TRAP_GATE);
    
    /* Vector 15: Reserved by Intel */
    idt_set_gate(IDT_RESERVED_15, (uint32_t)isr15, KERNEL_CODE_SELECTOR, IDT_FLAGS_TRAP_GATE);
    
    /* Vector 16: x87 FPU Floating-Point Error (#MF) */
    idt_set_gate(IDT_FPU_ERROR, (uint32_t)isr16, KERNEL_CODE_SELECTOR, IDT_FLAGS_TRAP_GATE);
    
    /* Vector 17: Alignment Check (#AC) */
    idt_set_gate(IDT_ALIGNMENT_CHECK, (uint32_t)isr17, KERNEL_CODE_SELECTOR, IDT_FLAGS_TRAP_GATE);
    
    /* Vector 18: Machine Check (#MC) - Critical, use interrupt gate */
    idt_set_gate(IDT_MACHINE_CHECK, (uint32_t)isr18, KERNEL_CODE_SELECTOR, IDT_FLAGS_INTERRUPT_GATE);
    
    /* Vector 19: SIMD Floating-Point Exception (#XM) */
    idt_set_gate(IDT_SIMD_EXCEPTION, (uint32_t)isr19, KERNEL_CODE_SELECTOR, IDT_FLAGS_TRAP_GATE);
    
    /* Vector 20: Virtualization Exception (#VE) */
    idt_set_gate(IDT_VIRTUALIZATION_EXCEPTION, (uint32_t)isr20, KERNEL_CODE_SELECTOR, IDT_FLAGS_TRAP_GATE);
    
    /* Vector 21: Control Protection Exception (#CP) */
    idt_set_gate(IDT_CONTROL_PROTECTION, (uint32_t)isr21, KERNEL_CODE_SELECTOR, IDT_FLAGS_TRAP_GATE);
    
    /* Vectors 22-31: Reserved by Intel */
    idt_set_gate(22, (uint32_t)isr22, KERNEL_CODE_SELECTOR, IDT_FLAGS_TRAP_GATE);
    idt_set_gate(23, (uint32_t)isr23, KERNEL_CODE_SELECTOR, IDT_FLAGS_TRAP_GATE);
    idt_set_gate(24, (uint32_t)isr24, KERNEL_CODE_SELECTOR, IDT_FLAGS_TRAP_GATE);
    idt_set_gate(25, (uint32_t)isr25, KERNEL_CODE_SELECTOR, IDT_FLAGS_TRAP_GATE);
    idt_set_gate(26, (uint32_t)isr26, KERNEL_CODE_SELECTOR, IDT_FLAGS_TRAP_GATE);
    idt_set_gate(27, (uint32_t)isr27, KERNEL_CODE_SELECTOR, IDT_FLAGS_TRAP_GATE);
    idt_set_gate(28, (uint32_t)isr28, KERNEL_CODE_SELECTOR, IDT_FLAGS_TRAP_GATE);
    idt_set_gate(29, (uint32_t)isr29, KERNEL_CODE_SELECTOR, IDT_FLAGS_TRAP_GATE);
    idt_set_gate(30, (uint32_t)isr30, KERNEL_CODE_SELECTOR, IDT_FLAGS_TRAP_GATE);
    idt_set_gate(31, (uint32_t)isr31, KERNEL_CODE_SELECTOR, IDT_FLAGS_TRAP_GATE);

    /*--------------------------------------------------------------------------
     * Install Hardware IRQ Handlers (Vectors 32-47)
     *--------------------------------------------------------------------------
     * These handle hardware interrupts from devices like keyboard, timer,
     * hard drives, etc. We use interrupt gates so that interrupts are
     * automatically disabled during IRQ handling to prevent re-entrance.
     *--------------------------------------------------------------------------
     */
    
    /* IRQ 0 (Vector 32): Programmable Interval Timer */
    idt_set_gate(32, (uint32_t)irq0, KERNEL_CODE_SELECTOR, IDT_FLAGS_INTERRUPT_GATE);
    
    /* IRQ 1 (Vector 33): Keyboard */
    idt_set_gate(33, (uint32_t)irq1, KERNEL_CODE_SELECTOR, IDT_FLAGS_INTERRUPT_GATE);
    
    /* IRQ 2 (Vector 34): Cascade (used internally by interrupt controllers) */
    idt_set_gate(34, (uint32_t)irq2, KERNEL_CODE_SELECTOR, IDT_FLAGS_INTERRUPT_GATE);
    
    /* IRQ 3 (Vector 35): COM2 Serial Port */
    idt_set_gate(35, (uint32_t)irq3, KERNEL_CODE_SELECTOR, IDT_FLAGS_INTERRUPT_GATE);
    
    /* IRQ 4 (Vector 36): COM1 Serial Port */
    idt_set_gate(36, (uint32_t)irq4, KERNEL_CODE_SELECTOR, IDT_FLAGS_INTERRUPT_GATE);
    
    /* IRQ 5 (Vector 37): LPT2 Parallel Port */
    idt_set_gate(37, (uint32_t)irq5, KERNEL_CODE_SELECTOR, IDT_FLAGS_INTERRUPT_GATE);
    
    /* IRQ 6 (Vector 38): Floppy Disk Controller */
    idt_set_gate(38, (uint32_t)irq6, KERNEL_CODE_SELECTOR, IDT_FLAGS_INTERRUPT_GATE);
    
    /* IRQ 7 (Vector 39): LPT1 Parallel Port */
    idt_set_gate(39, (uint32_t)irq7, KERNEL_CODE_SELECTOR, IDT_FLAGS_INTERRUPT_GATE);
    
    /* IRQ 8 (Vector 40): CMOS Real-Time Clock */
    idt_set_gate(40, (uint32_t)irq8, KERNEL_CODE_SELECTOR, IDT_FLAGS_INTERRUPT_GATE);
    
    /* IRQ 9 (Vector 41): Free for peripherals / legacy SCSI / NIC */
    idt_set_gate(41, (uint32_t)irq9, KERNEL_CODE_SELECTOR, IDT_FLAGS_INTERRUPT_GATE);
    
    /* IRQ 10 (Vector 42): Free for peripherals / SCSI / NIC */
    idt_set_gate(42, (uint32_t)irq10, KERNEL_CODE_SELECTOR, IDT_FLAGS_INTERRUPT_GATE);
    
    /* IRQ 11 (Vector 43): Free for peripherals / SCSI / NIC */
    idt_set_gate(43, (uint32_t)irq11, KERNEL_CODE_SELECTOR, IDT_FLAGS_INTERRUPT_GATE);
    
    /* IRQ 12 (Vector 44): PS/2 Mouse */
    idt_set_gate(44, (uint32_t)irq12, KERNEL_CODE_SELECTOR, IDT_FLAGS_INTERRUPT_GATE);
    
    /* IRQ 13 (Vector 45): FPU / Coprocessor / Inter-processor */
    idt_set_gate(45, (uint32_t)irq13, KERNEL_CODE_SELECTOR, IDT_FLAGS_INTERRUPT_GATE);
    
    /* IRQ 14 (Vector 46): Primary ATA Hard Disk */
    idt_set_gate(46, (uint32_t)irq14, KERNEL_CODE_SELECTOR, IDT_FLAGS_INTERRUPT_GATE);
    
    /* IRQ 15 (Vector 47): Secondary ATA Hard Disk */
    idt_set_gate(47, (uint32_t)irq15, KERNEL_CODE_SELECTOR, IDT_FLAGS_INTERRUPT_GATE);

    /*
     * Load the new IDT using LIDT instruction:
     * This assembly function will execute LIDT to load the new IDT
     * into the CPU's IDTR register, making it active.
     */
    idt_flush((uint32_t)&idt_pointer);
}

/**
 * @brief Common interrupt handler
 * 
 * This function is called by all ISR assembly stubs after they've saved
 * the processor state. It provides a central point for handling interrupts
 * and exceptions, making it easy to add custom handling logic.
 * 
 * @param regs Pointer to saved processor state
 */
void interrupt_handler(interrupt_registers_t *regs)
{
    /*
     * Handle CPU exceptions (vectors 0-31):
     * These are synchronous interrupts generated by the CPU when
     * it detects an error or exceptional condition during instruction execution.
     */
    if (regs->int_no < 32) {
        /* Display exception information */
        terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_RED));
        terminal_writestring("\n*** KERNEL PANIC ***\n");
        terminal_writestring("Exception: ");
        
        /* Display the exception name if we have one */
        if (regs->int_no < sizeof(exception_messages) / sizeof(exception_messages[0])) {
            terminal_writestring(exception_messages[regs->int_no]);
        } else {
            terminal_writestring("Unknown Exception");
        }
        
        terminal_writestring("\n");
        
        /* Display additional exception information */
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        terminal_writestring("Vector: ");
        /* Simple number to string conversion for debugging */
        char num_str[16];
        int i = 0;
        uint32_t num = regs->int_no;
        if (num == 0) {
            num_str[i++] = '0';
        } else {
            while (num > 0) {
                num_str[i++] = '0' + (num % 10);
                num /= 10;
            }
            /* Reverse the string */
            for (int j = 0; j < i / 2; j++) {
                char temp = num_str[j];
                num_str[j] = num_str[i - 1 - j];
                num_str[i - 1 - j] = temp;
            }
        }
        num_str[i] = '\0';
        terminal_writestring(num_str);
        
        terminal_writestring(", Error Code: ");
        /* Convert error code to string */
        i = 0;
        num = regs->err_code;
        if (num == 0) {
            num_str[i++] = '0';
        } else {
            while (num > 0) {
                num_str[i++] = '0' + (num % 10);
                num /= 10;
            }
            /* Reverse the string */
            for (int j = 0; j < i / 2; j++) {
                char temp = num_str[j];
                num_str[j] = num_str[i - 1 - j];
                num_str[i - 1 - j] = temp;
            }
        }
        num_str[i] = '\0';
        terminal_writestring(num_str);
        terminal_writestring("\n");
        
        terminal_writestring("EIP: 0x");
        /* Convert EIP to hex string */
        num = regs->eip;
        for (int shift = 28; shift >= 0; shift -= 4) {
            int digit = (num >> shift) & 0xF;
            terminal_putchar(digit < 10 ? '0' + digit : 'A' + digit - 10);
        }
        terminal_writestring("\n");
        
        /* Halt the system - we can't recover from most exceptions */
        terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_RED));
        terminal_writestring("System halted.\n");
        
        /* Infinite loop to halt the system */
        for (;;) {
            asm volatile ("hlt");
        }
    }
    
    /*
     * Handle hardware interrupts (vectors 32+):
     * These are asynchronous interrupts generated by hardware devices.
     */
    else if (regs->int_no >= 32 && regs->int_no < 48) {
        /* This is a hardware IRQ */
        uint32_t irq_num = regs->int_no - 32;
        
        /* Handle specific IRQs */
        if (irq_num == 1) {
            /* IRQ1: Keyboard interrupt */
            keyboard_interrupt_handler();
        } else {
            /* Generic IRQ handling for debugging */
            terminal_setcolor(vga_entry_color(VGA_COLOR_BROWN, VGA_COLOR_BLACK));
            terminal_writestring("Received IRQ: ");
            
            /* Convert IRQ number to string */
            char num_str[16];
            int i = 0;
            if (irq_num == 0) {
                num_str[i++] = '0';
            } else {
                while (irq_num > 0) {
                    num_str[i++] = '0' + (irq_num % 10);
                    irq_num /= 10;
                }
                /* Reverse the string */
                for (int j = 0; j < i / 2; j++) {
                    char temp = num_str[j];
                    num_str[j] = num_str[i - 1 - j];
                    num_str[i - 1 - j] = temp;
                }
            }
            num_str[i] = '\0';
            terminal_writestring(num_str);
            terminal_writestring("\n");
            
            terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        }
        
        /* Send End of Interrupt (EOI) to PIC */
        pic_send_eoi(irq_num);
    }
    
    /*
     * Handle software interrupts and reserved vectors (48-255):
     * These can be used for system calls or custom interrupt purposes.
     */
    else {
        terminal_setcolor(vga_entry_color(VGA_COLOR_CYAN, VGA_COLOR_BLACK));
        terminal_writestring("Received interrupt: ");
        
        /* Convert interrupt number to string */
        char num_str[16];
        int i = 0;
        uint32_t num = regs->int_no;
        if (num == 0) {
            num_str[i++] = '0';
        } else {
            while (num > 0) {
                num_str[i++] = '0' + (num % 10);
                num /= 10;
            }
            /* Reverse the string */
            for (int j = 0; j < i / 2; j++) {
                char temp = num_str[j];
                num_str[j] = num_str[i - 1 - j];
                num_str[i - 1 - j] = temp;
            }
        }
        num_str[i] = '\0';
        terminal_writestring(num_str);
        terminal_writestring("\n");
        
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    }
}
