#include "pic.h"
#include <stdbool.h>

/*------------------------------------------------------------------------------
 * I/O Port Helper Functions
 *------------------------------------------------------------------------------
 * These inline functions provide a clean interface for reading from and
 * writing to I/O ports. They use inline assembly for direct hardware access.
 *------------------------------------------------------------------------------
 */

/**
 * @brief Writes a byte to an I/O port
 * 
 * @param port The I/O port address
 * @param data The byte to write
 */
static inline void outb(uint16_t port, uint8_t data) {
    __asm__ volatile ("outb %0, %1" : : "a"(data), "Nd"(port));
}

/**
 * @brief Reads a byte from an I/O port
 * 
 * @param port The I/O port address
 * @return uint8_t The byte read from the port
 */
static inline uint8_t inb(uint16_t port) {
    uint8_t result;
    __asm__ volatile ("inb %1, %0" : "=a"(result) : "Nd"(port));
    return result;
}

/**
 * @brief Provides a small delay for I/O operations
 * 
 * Some hardware requires a small delay between I/O operations.
 * Writing to port 0x80 is a common way to create this delay.
 */
static inline void io_wait(void) {
    outb(0x80, 0);
}

/*------------------------------------------------------------------------------
 * PIC Implementation
 *------------------------------------------------------------------------------
 */

void pic_init(void) {
    uint8_t master_mask, slave_mask;
    
    /* Save the current interrupt masks before initialization */
    master_mask = inb(PIC1_DATA);
    slave_mask = inb(PIC2_DATA);
    
    /*--------------------------------------------------------------------------
     * Start the initialization sequence (ICW1)
     *--------------------------------------------------------------------------
     * ICW1 tells the PIC that we want to initialize it and configures:
     * - ICW4 will be sent (ICW1_ICW4)
     * - Cascade mode with master-slave configuration (not ICW1_SINGLE)
     * - Edge triggered mode (not ICW1_LEVEL)
     * - Initialization required (ICW1_INIT)
     *--------------------------------------------------------------------------
     */
    outb(PIC1_COMMAND, ICW1_INIT | ICW1_ICW4);  /* Start init sequence for master */
    io_wait();
    outb(PIC2_COMMAND, ICW1_INIT | ICW1_ICW4);  /* Start init sequence for slave */
    io_wait();
    
    /*--------------------------------------------------------------------------
     * Set vector offsets (ICW2)
     *--------------------------------------------------------------------------
     * ICW2 sets the base interrupt vector for each PIC. We remap the PIC
     * interrupts to avoid conflicts with CPU exceptions (vectors 0-31).
     * 
     * Master PIC: IRQs 0-7  map to vectors 32-39 (0x20-0x27)
     * Slave PIC:  IRQs 8-15 map to vectors 40-47 (0x28-0x2F)
     *--------------------------------------------------------------------------
     */
    outb(PIC1_DATA, PIC1_OFFSET);              /* Master PIC vector offset */
    io_wait();
    outb(PIC2_DATA, PIC2_OFFSET);              /* Slave PIC vector offset */
    io_wait();
    
    /*--------------------------------------------------------------------------
     * Configure master-slave relationship (ICW3)
     *--------------------------------------------------------------------------
     * ICW3 tells the master PIC which IRQ line the slave PIC is connected to,
     * and tells the slave PIC its cascade identity.
     * 
     * Master: Bit mask indicating slave PIC on IRQ2 (bit 2 = 0x04)
     * Slave:  Cascade identity number (2, since it's connected to IRQ2)
     *--------------------------------------------------------------------------
     */
    outb(PIC1_DATA, 4);                        /* Slave PIC at IRQ2 (0000 0100) */
    io_wait();
    outb(PIC2_DATA, 2);                        /* Cascade identity: IRQ2 */
    io_wait();
    
    /*--------------------------------------------------------------------------
     * Set PIC mode (ICW4)
     *--------------------------------------------------------------------------
     * ICW4 configures the PIC's operating mode:
     * - 8086 mode (ICW4_8086) instead of 8080 mode
     * - Normal EOI mode (not automatic)
     * - Non-buffered mode
     * - Not special fully nested mode
     *--------------------------------------------------------------------------
     */
    outb(PIC1_DATA, ICW4_8086);                /* Set master to 8086 mode */
    io_wait();
    outb(PIC2_DATA, ICW4_8086);                /* Set slave to 8086 mode */
    io_wait();
    
    /*--------------------------------------------------------------------------
     * Restore the interrupt masks
     *--------------------------------------------------------------------------
     * Restore the original interrupt masks that were saved before initialization.
     * This preserves any masking that was done before calling pic_init().
     *--------------------------------------------------------------------------
     */
    outb(PIC1_DATA, master_mask);              /* Restore master mask */
    outb(PIC2_DATA, slave_mask);               /* Restore slave mask */
}

void pic_send_eoi(uint8_t irq) {
    /*--------------------------------------------------------------------------
     * Send End of Interrupt (EOI) to the appropriate PIC(s)
     *--------------------------------------------------------------------------
     * For IRQs 0-7 (master PIC): Send EOI only to master
     * For IRQs 8-15 (slave PIC): Send EOI to both slave and master
     * 
     * The slave PIC is connected to the master's IRQ2, so interrupts from
     * the slave appear to the master as IRQ2. Therefore, we must send EOI
     * to both PICs for slave interrupts.
     *--------------------------------------------------------------------------
     */
    if (irq >= 8) {
        /* IRQ 8-15: Send EOI to slave PIC first */
        outb(PIC2_COMMAND, PIC_EOI);
    }
    
    /* IRQ 0-15: Always send EOI to master PIC */
    outb(PIC1_COMMAND, PIC_EOI);
}

void pic_mask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t value;
    
    /*--------------------------------------------------------------------------
     * Mask (disable) the specified IRQ
     *--------------------------------------------------------------------------
     * IRQs 0-7:  Use master PIC data port (0x21)
     * IRQs 8-15: Use slave PIC data port (0xA1), adjust IRQ number
     * 
     * To mask an IRQ, we set the corresponding bit in the PIC's mask register.
     *--------------------------------------------------------------------------
     */
    if (irq < 8) {
        /* Master PIC: IRQs 0-7 */
        port = PIC1_DATA;
    } else {
        /* Slave PIC: IRQs 8-15, adjust to 0-7 range */
        port = PIC2_DATA;
        irq -= 8;
    }
    
    /* Read current mask, set the bit for this IRQ, write back */
    value = inb(port) | (1 << irq);
    outb(port, value);
}

void pic_unmask_irq(uint8_t irq) {
    uint16_t port;
    uint8_t value;
    
    /*--------------------------------------------------------------------------
     * Unmask (enable) the specified IRQ
     *--------------------------------------------------------------------------
     * IRQs 0-7:  Use master PIC data port (0x21)
     * IRQs 8-15: Use slave PIC data port (0xA1), adjust IRQ number
     * 
     * To unmask an IRQ, we clear the corresponding bit in the PIC's mask register.
     *--------------------------------------------------------------------------
     */
    if (irq < 8) {
        /* Master PIC: IRQs 0-7 */
        port = PIC1_DATA;
    } else {
        /* Slave PIC: IRQs 8-15, adjust to 0-7 range */
        port = PIC2_DATA;
        irq -= 8;
    }
    
    /* Read current mask, clear the bit for this IRQ, write back */
    value = inb(port) & ~(1 << irq);
    outb(port, value);
}

void pic_disable(void) {
    /*--------------------------------------------------------------------------
     * Disable all IRQs by masking them
     *--------------------------------------------------------------------------
     * Set all bits in both PIC mask registers to 1, which disables all
     * hardware interrupts. This is useful when switching to APIC or during
     * system shutdown.
     *--------------------------------------------------------------------------
     */
    outb(PIC1_DATA, 0xFF);  /* Mask all IRQs on master PIC */
    outb(PIC2_DATA, 0xFF);  /* Mask all IRQs on slave PIC */
}

uint8_t pic_get_mask_master(void) {
    /*--------------------------------------------------------------------------
     * Read the current interrupt mask register from master PIC
     *--------------------------------------------------------------------------
     * Returns a bitmask where:
     * - 1 = IRQ is masked (disabled)
     * - 0 = IRQ is unmasked (enabled)
     *--------------------------------------------------------------------------
     */
    return inb(PIC1_DATA);
}

uint8_t pic_get_mask_slave(void) {
    /*--------------------------------------------------------------------------
     * Read the current interrupt mask register from slave PIC
     *--------------------------------------------------------------------------
     * Returns a bitmask where:
     * - 1 = IRQ is masked (disabled)
     * - 0 = IRQ is unmasked (enabled)
     *--------------------------------------------------------------------------
     */
    return inb(PIC2_DATA);
}

uint8_t pic_read_isr_master(void) {
    /*--------------------------------------------------------------------------
     * Read the In-Service Register (ISR) from master PIC
     *--------------------------------------------------------------------------
     * The ISR shows which interrupts are currently being serviced.
     * A bit is set when an interrupt is acknowledged and cleared when EOI is sent.
     * 
     * To read ISR, we send command 0x0B to the command port, then read from
     * the command port.
     *--------------------------------------------------------------------------
     */
    outb(PIC1_COMMAND, 0x0B);  /* OCW3: Read ISR */
    return inb(PIC1_COMMAND);
}

uint8_t pic_read_isr_slave(void) {
    /*--------------------------------------------------------------------------
     * Read the In-Service Register (ISR) from slave PIC
     *--------------------------------------------------------------------------
     */
    outb(PIC2_COMMAND, 0x0B);  /* OCW3: Read ISR */
    return inb(PIC2_COMMAND);
}

uint8_t pic_read_irr_master(void) {
    /*--------------------------------------------------------------------------
     * Read the Interrupt Request Register (IRR) from master PIC
     *--------------------------------------------------------------------------
     * The IRR shows which interrupts have been raised but not yet acknowledged.
     * This is useful for debugging interrupt problems.
     * 
     * To read IRR, we send command 0x0A to the command port, then read from
     * the command port.
     *--------------------------------------------------------------------------
     */
    outb(PIC1_COMMAND, 0x0A);  /* OCW3: Read IRR */
    return inb(PIC1_COMMAND);
}

uint8_t pic_read_irr_slave(void) {
    /*--------------------------------------------------------------------------
     * Read the Interrupt Request Register (IRR) from slave PIC
     *--------------------------------------------------------------------------
     */
    outb(PIC2_COMMAND, 0x0A);  /* OCW3: Read IRR */
    return inb(PIC2_COMMAND);
}

bool pic_is_spurious_irq(uint8_t irq) {
    /*--------------------------------------------------------------------------
     * Check if an IRQ is spurious
     *--------------------------------------------------------------------------
     * Spurious IRQs occur when an IRQ signal disappears between the PIC 
     * notifying the CPU and the CPU reading the interrupt vector. For spurious
     * IRQs, the corresponding bit in the ISR will NOT be set.
     * 
     * IRQ 7 (master): Check master PIC ISR bit 7
     * IRQ 15 (slave): Check slave PIC ISR bit 7 (IRQ 15 is bit 7 on slave)
     *--------------------------------------------------------------------------
     */
    
    if (irq == 7) {
        /* Check master PIC ISR for IRQ 7 */
        uint8_t isr = pic_read_isr_master();
        return !(isr & (1 << 7));  /* If bit 7 is NOT set, it's spurious */
    } 
    else if (irq == 15) {
        /* Check slave PIC ISR for IRQ 15 (bit 7 on slave) */
        uint8_t isr = pic_read_isr_slave();
        return !(isr & (1 << 7));  /* If bit 7 is NOT set, it's spurious */
    }
    
    /* For other IRQs, they're not typically spurious */
    return false;
}
