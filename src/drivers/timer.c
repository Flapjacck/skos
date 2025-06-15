/*------------------------------------------------------------------------------
 * Programmable Interval Timer (PIT) Driver Implementation
 *------------------------------------------------------------------------------
 * This file implements the timer functionality using the Intel 8253/8254 PIT
 * chip. It provides system timing, uptime tracking, and sleep functions.
 *-----------------    return result;
}-------------------------------------------
 */

#include "timer.h"
#include "../kernel/idt.h"
#include "../kernel/pic.h"

/*------------------------------------------------------------------------------
 * Forward Declarations for Helper Functions
 *------------------------------------------------------------------------------
 */
static uint64_t div64(uint64_t dividend, uint32_t divisor);

/* Need I/O port access functions */
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void cli(void) {
    __asm__ volatile ("cli");
}

static inline void sti(void) {
    __asm__ volatile ("sti");
}

static inline void hlt(void) {
    __asm__ volatile ("hlt");
}

/*------------------------------------------------------------------------------
 * Timer State Variables
 *------------------------------------------------------------------------------
 */

/* Timer state and configuration */
static bool timer_initialized = false;
static uint32_t timer_frequency = 0;
static uint16_t timer_reload_value = 0;

/* Timing tracking variables */
static volatile uint64_t timer_ticks = 0;
static volatile uint64_t uptime_ms = 0;
static uint32_t ms_per_tick = 0;
static uint32_t ms_fraction = 0;        /* 32.32 fixed point fractional ms */

/* Sleep functionality */
static volatile uint32_t sleep_countdown = 0;

/*------------------------------------------------------------------------------
 * Internal Helper Functions
 *------------------------------------------------------------------------------
 */

/**
 * @brief Calculate PIT reload value for given frequency
 */
uint16_t timer_calculate_reload_value(uint32_t frequency) {
    if (frequency < TIMER_MIN_FREQUENCY) {
        frequency = TIMER_MIN_FREQUENCY;
    }
    if (frequency > TIMER_MAX_FREQUENCY) {
        frequency = TIMER_MAX_FREQUENCY;
    }
    
    uint32_t reload = PIT_BASE_FREQUENCY / frequency;
    
    /* Handle rounding */
    if ((PIT_BASE_FREQUENCY % frequency) >= (frequency / 2)) {
        reload++;
    }
    
    /* Clamp to 16-bit range, 0 represents 65536 */
    if (reload > 65535) {
        reload = 65536;
    }
    
    return (uint16_t)(reload == 65536 ? 0 : reload);
}

/**
 * @brief Calculate actual frequency from reload value
 */
uint32_t timer_calculate_frequency(uint16_t reload_value) {
    uint32_t divisor = (reload_value == 0) ? 65536 : reload_value;
    return PIT_BASE_FREQUENCY / divisor;
}

/**
 * @brief Set PIT reload value directly
 */
void timer_set_reload_value(uint16_t reload_value) {
    cli();  /* Disable interrupts during PIT programming */
    
    /* Send command byte to set up channel 0 */
    outb(PIT_COMMAND_REGISTER, PIT_COMMAND_RATE_GEN);
    
    /* Send reload value (low byte first, then high byte) */
    outb(PIT_CHANNEL_0_DATA, reload_value & 0xFF);
    outb(PIT_CHANNEL_0_DATA, (reload_value >> 8) & 0xFF);
    
    sti();  /* Re-enable interrupts */
}

/**
 * @brief Read current PIT counter value
 */
uint16_t timer_read_current_count(void) {
    cli();  /* Disable interrupts during read */
    
    /* Send latch command for channel 0 */
    outb(PIT_COMMAND_REGISTER, PIT_SELECT_CHANNEL_0 | PIT_ACCESS_LATCH);
    
    /* Read latched value (low byte first, then high byte) */
    uint8_t low = inb(PIT_CHANNEL_0_DATA);
    uint8_t high = inb(PIT_CHANNEL_0_DATA);
    
    sti();  /* Re-enable interrupts */
    
    return (uint16_t)(low | (high << 8));
}

/**
 * @brief Calculate timing parameters for given frequency
 */
static void calculate_timing_parameters(uint32_t frequency) {
    /* Calculate milliseconds per tick using simple 32-bit arithmetic */
    ms_per_tick = 1000 / frequency;
    
    /* Calculate fractional part using careful arithmetic to avoid 64-bit ops */
    uint32_t remainder = 1000 % frequency;
    if (remainder > 0) {
        /* Approximate the fractional part without 64-bit division */
        /* This gives us reasonable accuracy for most use cases */
        ms_fraction = (remainder * 4294967U) / frequency;  /* ~2^32 / 1000 */
    } else {
        ms_fraction = 0;
    }
}

/*------------------------------------------------------------------------------
 * Timer Interrupt Handler
 *------------------------------------------------------------------------------
 */

/**
 * @brief Timer interrupt handler called by IRQ 0
 */
void timer_interrupt_handler(void) {
    /* Increment tick counter */
    timer_ticks++;
    
    /* Update uptime using whole milliseconds and fractions */
    static uint32_t fraction_accumulator = 0;
    
    uptime_ms += ms_per_tick;
    fraction_accumulator += ms_fraction;
    
    /* Handle overflow of fractional part */
    if (fraction_accumulator < ms_fraction) {  /* Overflow occurred */
        uptime_ms++;
        fraction_accumulator = 0xFFFFFFFF - fraction_accumulator + 1;
    }
    
    /* Handle sleep countdown */
    if (sleep_countdown > 0) {
        if (sleep_countdown <= ms_per_tick) {
            sleep_countdown = 0;
        } else {
            sleep_countdown -= ms_per_tick;
        }
    }
}

/*------------------------------------------------------------------------------
 * Public Timer Functions
 *------------------------------------------------------------------------------
 */

/**
 * @brief Initialize the timer subsystem
 */
void timer_init(void) {
    timer_init_frequency(TIMER_DEFAULT_FREQUENCY);
}

/**
 * @brief Initialize timer with custom frequency
 */
bool timer_init_frequency(uint32_t frequency) {
    /* Validate frequency range */
    if (frequency < TIMER_MIN_FREQUENCY || frequency > TIMER_MAX_FREQUENCY) {
        return false;
    }
    
    /* Calculate reload value */
    uint16_t reload = timer_calculate_reload_value(frequency);
    uint32_t actual_freq = timer_calculate_frequency(reload);
    
    /* Store configuration */
    timer_frequency = actual_freq;
    timer_reload_value = reload;
    
    /* Calculate timing parameters */
    calculate_timing_parameters(actual_freq);
    
    /* Reset timing variables */
    timer_ticks = 0;
    uptime_ms = 0;
    sleep_countdown = 0;
    
    /* Set up PIT hardware */
    timer_set_reload_value(reload);
    
    /* Mark as initialized */
    timer_initialized = true;
    
    return true;
}

/**
 * @brief Get current timer information
 */
void timer_get_info(struct timer_info *info) {
    if (!info || !timer_initialized) {
        return;
    }
    
    cli();  /* Ensure atomic read of timing variables */
    info->frequency = timer_frequency;
    info->reload_value = timer_reload_value;
    info->ticks = timer_ticks;
    info->uptime_ms = uptime_ms;
    info->ms_per_tick = ms_per_tick;
    info->ms_fraction = ms_fraction;
    sti();
}

/**
 * @brief Get system uptime in milliseconds
 */
uint64_t timer_get_uptime_ms(void) {
    if (!timer_initialized) {
        return 0;
    }
    
    cli();
    uint64_t ms = uptime_ms;
    sti();
    
    return ms;
}

/**
 * @brief Get system uptime in seconds
 */
uint32_t timer_get_uptime_seconds(void) {
    if (!timer_initialized) {
        return 0;
    }
    
    cli();
    uint64_t ms = uptime_ms;
    sti();
    
    /* Use our helper function for 64-bit division */
    return (uint32_t)div64(ms, 1000);
}

/**
 * @brief Get total number of timer ticks
 */
uint64_t timer_get_ticks(void) {
    if (!timer_initialized) {
        return 0;
    }
    
    cli();
    uint64_t ticks = timer_ticks;
    sti();
    
    return ticks;
}

/**
 * @brief Sleep for specified number of milliseconds
 */
void timer_sleep_ms(uint32_t milliseconds) {
    if (!timer_initialized || milliseconds == 0) {
        return;
    }
    
    /* Set sleep countdown */
    cli();
    sleep_countdown = milliseconds;
    sti();
    
    /* Wait for countdown to reach zero */
    while (sleep_countdown > 0) {
        hlt();  /* Halt until next interrupt */
    }
}

/**
 * @brief Sleep for specified number of seconds
 */
void timer_sleep_seconds(uint32_t seconds) {
    timer_sleep_ms(seconds * 1000);
}

/**
 * @brief Check if timer is initialized
 */
bool timer_is_initialized(void) {
    return timer_initialized;
}

/**
 * @brief Set timer frequency
 */
bool timer_set_frequency(uint32_t frequency) {
    if (!timer_initialized) {
        return timer_init_frequency(frequency);
    }
    
    /* Validate frequency range */
    if (frequency < TIMER_MIN_FREQUENCY || frequency > TIMER_MAX_FREQUENCY) {
        return false;
    }
    
    /* Calculate new reload value */
    uint16_t reload = timer_calculate_reload_value(frequency);
    uint32_t actual_freq = timer_calculate_frequency(reload);
    
    /* Update configuration */
    timer_frequency = actual_freq;
    timer_reload_value = reload;
    
    /* Calculate new timing parameters */
    calculate_timing_parameters(actual_freq);
    
    /* Update PIT hardware */
    timer_set_reload_value(reload);
    
    return true;
}

/*------------------------------------------------------------------------------
 * 64-bit Arithmetic Helper Functions
 *------------------------------------------------------------------------------
 * These functions provide 64-bit division and modulo operations without
 * relying on libgcc, which isn't available in freestanding kernel environment.
 *------------------------------------------------------------------------------
 */

/**
 * @brief 64-bit unsigned division
 * Simple implementation for kernel use
 */
static uint64_t div64(uint64_t dividend, uint32_t divisor) {
    if (divisor == 0) return 0;  /* Avoid division by zero */
    
    /* If dividend fits in 32 bits, use regular division */
    if (dividend <= 0xFFFFFFFF) {
        return (uint32_t)dividend / divisor;
    }
    
    /* For larger dividends, use bit-by-bit division */
    uint64_t quotient = 0;
    uint64_t remainder = 0;
    
    for (int i = 63; i >= 0; i--) {
        remainder <<= 1;
        remainder |= (dividend >> i) & 1;
        
        if (remainder >= divisor) {
            remainder -= divisor;
            quotient |= (1ULL << i);
        }
    }
    
    return quotient;
}

