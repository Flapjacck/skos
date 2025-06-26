#ifndef DEBUG_H
#define DEBUG_H

#include <stdint.h>
#include <stdbool.h>

/*------------------------------------------------------------------------------
 * Kernel Debugging and Profiling Support
 *------------------------------------------------------------------------------
 * This module provides debugging utilities including:
 * - Stack canary support for stack overflow detection
 * - Basic profiling counters for system events
 * - Debug assertions and logging
 *------------------------------------------------------------------------------
 */

/*------------------------------------------------------------------------------
 * Stack Canary Implementation
 *------------------------------------------------------------------------------
 * Based on GCC's Stack Smashing Protector (SSP) design.
 * Uses a magic value to detect stack buffer overflows.
 *------------------------------------------------------------------------------
 */

/* Stack canary magic values */
#if defined(__i386__) || UINT32_MAX == UINTPTR_MAX
#define STACK_CHK_GUARD 0xe2dee396
#else
#define STACK_CHK_GUARD 0x595e9fbd94fda766
#endif

/* Stack canary global variable (used by GCC's SSP) */
extern uintptr_t __stack_chk_guard;

/*------------------------------------------------------------------------------
 * Profiling Counters
 *------------------------------------------------------------------------------
 * Simple counters for tracking system events and performance metrics
 *------------------------------------------------------------------------------
 */

struct kernel_profiling {
    /* Interrupt and exception counters */
    uint64_t total_interrupts;          /* Total interrupts handled */
    uint64_t timer_interrupts;          /* Timer (IRQ0) interrupts */
    uint64_t keyboard_interrupts;       /* Keyboard (IRQ1) interrupts */
    uint64_t spurious_interrupts;       /* Spurious IRQ7/IRQ15 */
    uint64_t exceptions;                 /* CPU exceptions (faults) */
    uint64_t page_faults;               /* Page fault exceptions */
    uint64_t general_protection_faults; /* GPF exceptions */
    
    /* Memory allocation counters */
    uint64_t memory_allocations;        /* Memory allocations */
    uint64_t memory_frees;              /* Memory deallocations */
    uint32_t memory_allocated_bytes;    /* Currently allocated bytes */
    uint32_t peak_memory_usage;         /* Peak memory usage */
    
    /* System call counters (for future use) */
    uint64_t system_calls;              /* System call count */
    
    /* Performance metrics */
    uint64_t context_switches;          /* Context switches (future) */
    uint32_t max_interrupt_latency;     /* Max interrupt handling time */
};

/*------------------------------------------------------------------------------
 * Debug Function Declarations
 *------------------------------------------------------------------------------
 */

/**
 * @brief Initialize debugging and profiling subsystem
 * 
 * Sets up stack canary protection and initializes profiling counters.
 * Should be called early in kernel initialization.
 */
void debug_init(void);

/**
 * @brief Get current profiling statistics
 * 
 * @return Pointer to current profiling data structure
 */
const struct kernel_profiling* debug_get_profiling_stats(void);

/**
 * @brief Reset all profiling counters to zero
 */
void debug_reset_profiling_stats(void);

/**
 * @brief Increment interrupt counter for profiling
 * 
 * @param irq_num IRQ number (0-15, or 0xFF for exceptions)
 */
void debug_count_interrupt(uint8_t irq_num);

/**
 * @brief Increment exception counter for profiling
 * 
 * @param exception_num Exception vector number (0-31)
 */
void debug_count_exception(uint8_t exception_num);

/**
 * @brief Track memory allocation for profiling
 * 
 * @param bytes Number of bytes allocated
 */
void debug_count_memory_alloc(uint32_t bytes);

/**
 * @brief Track memory deallocation for profiling
 * 
 * @param bytes Number of bytes freed
 */
void debug_count_memory_free(uint32_t bytes);

/**
 * @brief Simple assertion macro for kernel debugging
 * 
 * @param condition Condition to check
 * @param message Message to display if assertion fails
 */
#define KERNEL_ASSERT(condition, message) \
    do { \
        if (!(condition)) { \
            debug_panic("ASSERTION FAILED: " message " at %s:%d", __FILE__, __LINE__); \
        } \
    } while(0)

/**
 * @brief Kernel panic function
 * 
 * Displays panic message and halts the system.
 * 
 * @param format Printf-style format string
 * @param ... Arguments for format string
 */
void debug_panic(const char* format, ...);

/**
 * @brief Stack canary failure handler
 * 
 * Called when stack smashing is detected.
 * This function should not return.
 */
void __stack_chk_fail(void) __attribute__((noreturn));

/**
 * @brief Local stack canary failure handler
 * 
 * Alternative entry point used by GCC in some cases.
 * This function should not return.
 */
void __stack_chk_fail_local(void) __attribute__((noreturn));

/**
 * @brief Manual stack canary check
 * 
 * Can be used to manually verify stack integrity.
 * 
 * @param canary_value Expected canary value
 * @return true if stack is intact, false if corrupted
 */
bool debug_check_stack_canary(uintptr_t canary_value);

/**
 * @brief Display profiling statistics to terminal
 * 
 * Prints current profiling counters in a readable format.
 */
void debug_print_profiling_stats(void);

/*------------------------------------------------------------------------------
 * Debug Macros
 *------------------------------------------------------------------------------
 */

/* Debug output macro (can be disabled in release builds) */
#ifdef DEBUG_ENABLED
#define DEBUG_PRINT(msg) debug_print(msg)
#else
#define DEBUG_PRINT(msg) do {} while(0)
#endif

/**
 * @brief Debug print function
 * 
 * @param message Message to print
 */
void debug_print(const char* message);

#endif /* DEBUG_H */
