#ifndef PIC_H
#define PIC_H

#include <stdint.h>
#include <stdbool.h>

/*------------------------------------------------------------------------------
 * 8259 Programmable Interrupt Controller (PIC) Definitions
 *------------------------------------------------------------------------------
 * The 8259 PIC is responsible for managing hardware interrupts on x86 systems.
 * Most systems have two PICs in a master-slave configuration:
 * - Master PIC: Handles IRQs 0-7
 * - Slave PIC: Handles IRQs 8-15, connected to IRQ2 of master
 * 
 * By default, the PIC maps IRQs to interrupt vectors 8-15 (master) and 
 * 70-77h (slave), but these conflict with CPU exceptions, so we remap them.
 *------------------------------------------------------------------------------
 */

/*------------------------------------------------------------------------------
 * PIC Port Addresses
 *------------------------------------------------------------------------------
 * Each PIC has two I/O ports: command and data
 *------------------------------------------------------------------------------
 */
#define PIC1_COMMAND    0x20    /* Master PIC command port */
#define PIC1_DATA       0x21    /* Master PIC data port */
#define PIC2_COMMAND    0xA0    /* Slave PIC command port */
#define PIC2_DATA       0xA1    /* Slave PIC data port */

/*------------------------------------------------------------------------------
 * PIC Command Constants
 *------------------------------------------------------------------------------
 */
#define PIC_EOI         0x20    /* End of Interrupt command */

/*------------------------------------------------------------------------------
 * ICW1 (Initialization Command Word 1) Bits
 *------------------------------------------------------------------------------
 * ICW1 is the first initialization command sent to the PIC
 *------------------------------------------------------------------------------
 */
#define ICW1_ICW4       0x01    /* ICW4 (not) needed */
#define ICW1_SINGLE     0x02    /* Single (cascade) mode */
#define ICW1_INTERVAL4  0x04    /* Call address interval 4 (8) */
#define ICW1_LEVEL      0x08    /* Level triggered (edge) mode */
#define ICW1_INIT       0x10    /* Initialization - required! */

/*------------------------------------------------------------------------------
 * ICW4 (Initialization Command Word 4) Bits
 *------------------------------------------------------------------------------
 * ICW4 configures the PIC's operating mode
 *------------------------------------------------------------------------------
 */
#define ICW4_8086       0x01    /* 8086/88 (MCS-80/85) mode */
#define ICW4_AUTO       0x02    /* Auto (normal) EOI */
#define ICW4_BUF_SLAVE  0x08    /* Buffered mode/slave */
#define ICW4_BUF_MASTER 0x0C    /* Buffered mode/master */
#define ICW4_SFNM       0x10    /* Special fully nested (not) */

/*------------------------------------------------------------------------------
 * IRQ Vector Offsets
 *------------------------------------------------------------------------------
 * We remap the PIC IRQs to avoid conflicts with CPU exceptions (0-31)
 * Master PIC: IRQs 0-7  → Vectors 32-39
 * Slave PIC:  IRQs 8-15 → Vectors 40-47
 *------------------------------------------------------------------------------
 */
#define PIC1_OFFSET     32      /* Master PIC vector offset */
#define PIC2_OFFSET     40      /* Slave PIC vector offset */

/*------------------------------------------------------------------------------
 * Common IRQ Numbers
 *------------------------------------------------------------------------------
 * Standard IRQ assignments for common hardware devices
 *------------------------------------------------------------------------------
 */
#define IRQ_TIMER       0       /* System timer (PIT) */
#define IRQ_KEYBOARD    1       /* Keyboard controller */
#define IRQ_CASCADE     2       /* Cascade for slave PIC (never raised) */
#define IRQ_COM2        3       /* COM2 serial port */
#define IRQ_COM1        4       /* COM1 serial port */
#define IRQ_LPT2        5       /* LPT2 parallel port */
#define IRQ_FLOPPY      6       /* Floppy disk controller */
#define IRQ_LPT1        7       /* LPT1 parallel port */
#define IRQ_CMOS        8       /* CMOS real-time clock */
#define IRQ_FREE1       9       /* Free for peripherals */
#define IRQ_FREE2       10      /* Free for peripherals */
#define IRQ_FREE3       11      /* Free for peripherals */
#define IRQ_MOUSE       12      /* PS/2 mouse */
#define IRQ_FPU         13      /* FPU / coprocessor / inter-processor */
#define IRQ_ATA1        14      /* Primary ATA hard disk */
#define IRQ_ATA2        15      /* Secondary ATA hard disk */

/*------------------------------------------------------------------------------
 * PIC Function Declarations
 *------------------------------------------------------------------------------
 */

/**
 * @brief Initializes the 8259 PIC
 * 
 * This function performs the complete PIC initialization sequence:
 * 1. Saves the current interrupt masks
 * 2. Starts the initialization sequence (ICW1)
 * 3. Sets the vector offsets (ICW2) to avoid conflicts with CPU exceptions
 * 4. Configures master-slave relationship (ICW3)
 * 5. Sets the PIC mode to 8086 (ICW4)
 * 6. Restores the interrupt masks
 * 
 * After initialization:
 * - Master PIC handles IRQs 0-7 as vectors 32-39
 * - Slave PIC handles IRQs 8-15 as vectors 40-47
 * - All IRQs are initially masked (disabled)
 */
void pic_init(void);

/**
 * @brief Sends End of Interrupt (EOI) signal to the PIC
 * 
 * This function must be called at the end of every hardware interrupt handler
 * to inform the PIC that the interrupt has been processed and it can send
 * the next interrupt. Without EOI, the PIC will not send any more interrupts
 * of the same or lower priority.
 * 
 * @param irq The IRQ number (0-15) that was just handled
 * 
 * Note: For IRQs 8-15 (slave PIC), EOI must be sent to both PICs since
 * the slave is connected through the master's IRQ2.
 */
void pic_send_eoi(uint8_t irq);

/**
 * @brief Masks (disables) a specific IRQ
 * 
 * When an IRQ is masked, the PIC will not send interrupts for that IRQ line
 * to the CPU. This is useful for temporarily disabling specific hardware
 * interrupts.
 * 
 * @param irq The IRQ number (0-15) to mask
 */
void pic_mask_irq(uint8_t irq);

/**
 * @brief Unmasks (enables) a specific IRQ
 * 
 * When an IRQ is unmasked, the PIC will send interrupts for that IRQ line
 * to the CPU when the corresponding hardware device raises an interrupt.
 * 
 * @param irq The IRQ number (0-15) to unmask
 */
void pic_unmask_irq(uint8_t irq);

/**
 * @brief Disables the PIC by masking all IRQs
 * 
 * This function masks all IRQs on both PICs, effectively disabling all
 * hardware interrupts. This is typically used when transitioning to APIC
 * or during system shutdown.
 * 
 * Note: This does not disable the PIC hardware itself, just masks all
 * interrupt lines.
 */
void pic_disable(void);

/**
 * @brief Gets the current interrupt mask for the master PIC
 * 
 * @return uint8_t Bitmask where each bit represents an IRQ (0-7)
 *                 1 = masked (disabled), 0 = unmasked (enabled)
 */
uint8_t pic_get_mask_master(void);

/**
 * @brief Gets the current interrupt mask for the slave PIC
 * 
 * @return uint8_t Bitmask where each bit represents an IRQ (8-15)
 *                 1 = masked (disabled), 0 = unmasked (enabled)
 */
uint8_t pic_get_mask_slave(void);

/**
 * @brief Reads the In-Service Register (ISR) from the master PIC
 * 
 * The ISR shows which IRQs are currently being serviced (handled).
 * A bit is set when an IRQ is received and cleared when EOI is sent.
 * 
 * @return uint8_t Bitmask where each bit represents an IRQ (0-7)
 *                 1 = IRQ is being serviced, 0 = IRQ is not being serviced
 */
uint8_t pic_read_isr_master(void);

/**
 * @brief Reads the In-Service Register (ISR) from the slave PIC
 * 
 * @return uint8_t Bitmask where each bit represents an IRQ (8-15)
 *                 1 = IRQ is being serviced, 0 = IRQ is not being serviced
 */
uint8_t pic_read_isr_slave(void);

/**
 * @brief Reads the Interrupt Request Register (IRR) from the master PIC
 * 
 * The IRR shows which IRQs have been raised but not yet acknowledged.
 * This is useful for debugging interrupt issues.
 * 
 * @return uint8_t Bitmask where each bit represents an IRQ (0-7)
 *                 1 = IRQ has been raised, 0 = IRQ has not been raised
 */
uint8_t pic_read_irr_master(void);

/**
 * @brief Reads the Interrupt Request Register (IRR) from the slave PIC
 * 
 * @return uint8_t Bitmask where each bit represents an IRQ (8-15)
 *                 1 = IRQ has been raised, 0 = IRQ has not been raised
 */
uint8_t pic_read_irr_slave(void);

/**
 * @brief Checks if an IRQ is spurious
 * 
 * Spurious IRQs can occur when an IRQ signal disappears between the PIC 
 * notifying the CPU and the CPU acknowledging the interrupt. This function
 * checks the ISR to determine if the IRQ is real or spurious.
 * 
 * @param irq The IRQ number (0-15) to check
 * @return true if the IRQ is spurious, false if it's real
 */
bool pic_is_spurious_irq(uint8_t irq);

#endif /* PIC_H */
