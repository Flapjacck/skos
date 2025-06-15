#ifndef TIMER_H
#define TIMER_H

#include <stdint.h>
#include <stdbool.h>

/*------------------------------------------------------------------------------
 * Programmable Interval Timer (PIT) Driver
 *------------------------------------------------------------------------------
 * This driver implements timer functionality using the Intel 8253/8254 PIT chip.
 * The PIT uses Channel 0 connected to IRQ 0 for generating periodic timer
 * interrupts. The base frequency is ~1.193182 MHz.
 *
 * Features:
 * - Configurable timer frequency (18.2 Hz to ~1.19 MHz)
 * - System uptime tracking
 * - Sleep functionality
 * - Timer tick counting
 *------------------------------------------------------------------------------
 */

/*------------------------------------------------------------------------------
 * PIT Constants and Definitions
 *------------------------------------------------------------------------------
 */

/* PIT I/O Port addresses */
#define PIT_CHANNEL_0_DATA     0x40    /* Channel 0 data port (read/write) */
#define PIT_CHANNEL_1_DATA     0x41    /* Channel 1 data port (read/write) */
#define PIT_CHANNEL_2_DATA     0x42    /* Channel 2 data port (read/write) */
#define PIT_COMMAND_REGISTER   0x43    /* Mode/Command register (write only) */

/* PIT base frequency in Hz (~1.193182 MHz) */
#define PIT_BASE_FREQUENCY     1193182

/* Default timer frequency (100 Hz = 10ms per tick) */
#define TIMER_DEFAULT_FREQUENCY 100

/* Minimum and maximum supported frequencies */
#define TIMER_MIN_FREQUENCY    18      /* ~54.9ms per tick */
#define TIMER_MAX_FREQUENCY    1193181 /* ~0.84μs per tick */

/*------------------------------------------------------------------------------
 * PIT Command Register Bit Definitions
 *------------------------------------------------------------------------------
 */

/* Channel selection (bits 7-6) */
#define PIT_SELECT_CHANNEL_0   0x00    /* Select channel 0 */
#define PIT_SELECT_CHANNEL_1   0x40    /* Select channel 1 */
#define PIT_SELECT_CHANNEL_2   0x80    /* Select channel 2 */
#define PIT_READ_BACK          0xC0    /* Read-back command */

/* Access mode (bits 5-4) */
#define PIT_ACCESS_LATCH       0x00    /* Counter latch command */
#define PIT_ACCESS_LOBYTE      0x10    /* Access low byte only */
#define PIT_ACCESS_HIBYTE      0x20    /* Access high byte only */
#define PIT_ACCESS_LOHI        0x30    /* Access low byte, then high byte */

/* Operating mode (bits 3-1) */
#define PIT_MODE_0             0x00    /* Interrupt on terminal count */
#define PIT_MODE_1             0x02    /* Hardware re-triggerable one-shot */
#define PIT_MODE_2             0x04    /* Rate generator */
#define PIT_MODE_3             0x06    /* Square wave generator */
#define PIT_MODE_4             0x08    /* Software triggered strobe */
#define PIT_MODE_5             0x0A    /* Hardware triggered strobe */

/* BCD/Binary mode (bit 0) */
#define PIT_BINARY_MODE        0x00    /* 16-bit binary mode */
#define PIT_BCD_MODE          0x01    /* 4-digit BCD mode */

/* Common command combinations */
#define PIT_COMMAND_RATE_GEN   (PIT_SELECT_CHANNEL_0 | PIT_ACCESS_LOHI | PIT_MODE_2 | PIT_BINARY_MODE)
#define PIT_COMMAND_SQUARE_WAVE (PIT_SELECT_CHANNEL_0 | PIT_ACCESS_LOHI | PIT_MODE_3 | PIT_BINARY_MODE)

/*------------------------------------------------------------------------------
 * Timer Data Structures
 *------------------------------------------------------------------------------
 */

/**
 * @brief Timer statistics and state information
 */
struct timer_info {
    uint32_t frequency;           /* Current timer frequency in Hz */
    uint32_t reload_value;        /* Current PIT reload value */
    uint64_t ticks;              /* Total number of timer ticks since init */
    uint64_t uptime_ms;          /* System uptime in milliseconds */
    uint32_t ms_per_tick;        /* Whole milliseconds per tick */
    uint32_t ms_fraction;        /* Fractional milliseconds per tick (32.32 fixed point) */
};

/*------------------------------------------------------------------------------
 * Timer Function Declarations
 *------------------------------------------------------------------------------
 */

/**
 * @brief Initialize the timer subsystem
 * 
 * Sets up PIT Channel 0 to generate timer interrupts at the default frequency.
 * This function must be called after IDT and PIC initialization.
 * 
 * The timer will:
 * - Configure PIT Channel 0 in rate generator mode (Mode 2)
 * - Set up IRQ 0 handler for timer interrupts
 * - Initialize timing variables for uptime tracking
 * - Enable timer interrupts via PIC
 */
void timer_init(void);

/**
 * @brief Initialize timer with custom frequency
 * 
 * @param frequency Desired timer frequency in Hz (18-1193181)
 * @return true if frequency was set successfully, false if out of range
 */
bool timer_init_frequency(uint32_t frequency);

/**
 * @brief Get current timer information
 * 
 * @param info Pointer to timer_info structure to fill
 */
void timer_get_info(struct timer_info *info);

/**
 * @brief Get system uptime in milliseconds
 * 
 * @return System uptime in milliseconds since timer initialization
 */
uint64_t timer_get_uptime_ms(void);

/**
 * @brief Get system uptime in seconds
 * 
 * @return System uptime in seconds since timer initialization
 */
uint32_t timer_get_uptime_seconds(void);

/**
 * @brief Get total number of timer ticks
 * 
 * @return Total timer ticks since initialization
 */
uint64_t timer_get_ticks(void);

/**
 * @brief Sleep for specified number of milliseconds
 * 
 * This function blocks execution for approximately the specified duration.
 * The actual sleep time may be slightly longer due to timer resolution.
 * 
 * @param milliseconds Number of milliseconds to sleep
 */
void timer_sleep_ms(uint32_t milliseconds);

/**
 * @brief Sleep for specified number of seconds
 * 
 * @param seconds Number of seconds to sleep
 */
void timer_sleep_seconds(uint32_t seconds);

/**
 * @brief Check if timer is initialized
 * 
 * @return true if timer is initialized, false otherwise
 */
bool timer_is_initialized(void);

/**
 * @brief Set timer frequency
 * 
 * Changes the timer frequency at runtime. This will affect all timing-related
 * functions and may cause temporary inaccuracies in uptime tracking.
 * 
 * @param frequency New frequency in Hz (18-1193181)
 * @return true if frequency was set successfully, false if out of range
 */
bool timer_set_frequency(uint32_t frequency);

/**
 * @brief Timer interrupt handler
 * 
 * This function is called by the IRQ 0 handler on each timer tick.
 * It updates timing variables and handles sleep countdown.
 * 
 * Note: This function should only be called from interrupt context.
 */
void timer_interrupt_handler(void);

/*------------------------------------------------------------------------------
 * Internal Helper Functions (for advanced use)
 *------------------------------------------------------------------------------
 */

/**
 * @brief Calculate PIT reload value for given frequency
 * 
 * @param frequency Desired frequency in Hz
 * @return PIT reload value (0 represents 65536)
 */
uint16_t timer_calculate_reload_value(uint32_t frequency);

/**
 * @brief Calculate actual frequency from reload value
 * 
 * @param reload_value PIT reload value
 * @return Actual frequency in Hz
 */
uint32_t timer_calculate_frequency(uint16_t reload_value);

/**
 * @brief Set PIT reload value directly
 * 
 * Low-level function to set the PIT reload value directly.
 * Use timer_set_frequency() instead unless you need precise control.
 * 
 * @param reload_value 16-bit reload value (0 = 65536)
 */
void timer_set_reload_value(uint16_t reload_value);

/**
 * @brief Read current PIT counter value
 * 
 * @return Current counter value
 */
uint16_t timer_read_current_count(void);

#endif /* TIMER_H */