/*------------------------------------------------------------------------------
 * Kernel Debugging and Profiling Implementation
 *------------------------------------------------------------------------------
 * This file implements debugging utilities including stack canaries and
 * basic profiling counters for the SKOS kernel.
 *------------------------------------------------------------------------------
 */

#include "debug.h"
#include "kernel.h"  /* For terminal functions */
#include <stdarg.h>

/*------------------------------------------------------------------------------
 * Stack Canary Implementation
 *------------------------------------------------------------------------------
 */

/* Global stack canary value - used by GCC's Stack Smashing Protector */
uintptr_t __stack_chk_guard = STACK_CHK_GUARD;

/*------------------------------------------------------------------------------
 * Profiling Data
 *------------------------------------------------------------------------------
 */

/* Global profiling statistics */
static struct kernel_profiling profiling_stats = {0};

/* Debug initialization flag */
static bool debug_initialized = false;

/*------------------------------------------------------------------------------
 * Helper Functions
 *------------------------------------------------------------------------------
 */

/* Simple integer to string conversion - 32-bit only to avoid libgcc dependencies */
static void debug_uint32_to_str(uint32_t value, char* buffer, size_t buffer_size) {
    if (buffer_size == 0) return;
    
    if (value == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }
    
    size_t i = 0;
    while (value > 0 && i < buffer_size - 1) {
        buffer[i++] = '0' + (value % 10);
        value /= 10;
    }
    buffer[i] = '\0';
    
    /* Reverse the string */
    for (size_t j = 0; j < i / 2; j++) {
        char temp = buffer[j];
        buffer[j] = buffer[i - 1 - j];
        buffer[i - 1 - j] = temp;
    }
}

/* 64-bit integer to string conversion - splits into high and low parts */
static void debug_uint64_to_str(uint64_t value, char* buffer, size_t buffer_size) {
    if (buffer_size == 0) return;
    
    /* For values that fit in 32-bit, use the simpler function */
    if (value <= 0xFFFFFFFF) {
        debug_uint32_to_str((uint32_t)value, buffer, buffer_size);
        return;
    }
    
    /* For larger values, show high and low parts separately */
    uint32_t high = (uint32_t)(value >> 32);
    uint32_t low = (uint32_t)(value & 0xFFFFFFFF);
    
    char high_str[16];
    char low_str[16];
    
    debug_uint32_to_str(high, high_str, sizeof(high_str));
    debug_uint32_to_str(low, low_str, sizeof(low_str));
    
    /* Combine them with a separator */
    size_t pos = 0;
    for (size_t i = 0; high_str[i] && pos < buffer_size - 1; i++) {
        buffer[pos++] = high_str[i];
    }
    if (pos < buffer_size - 1) buffer[pos++] = ':';
    for (size_t i = 0; low_str[i] && pos < buffer_size - 1; i++) {
        buffer[pos++] = low_str[i];
    }
    buffer[pos] = '\0';
}

/*------------------------------------------------------------------------------
 * Public Debug Functions
 *------------------------------------------------------------------------------
 */

/**
 * @brief Initialize debugging and profiling subsystem
 */
void debug_init(void) {
    /* Initialize profiling counters to zero */
    profiling_stats.total_interrupts = 0;
    profiling_stats.timer_interrupts = 0;
    profiling_stats.keyboard_interrupts = 0;
    profiling_stats.spurious_interrupts = 0;
    profiling_stats.exceptions = 0;
    profiling_stats.page_faults = 0;
    profiling_stats.general_protection_faults = 0;
    profiling_stats.memory_allocations = 0;
    profiling_stats.memory_frees = 0;
    profiling_stats.memory_allocated_bytes = 0;
    profiling_stats.peak_memory_usage = 0;
    profiling_stats.system_calls = 0;
    profiling_stats.context_switches = 0;
    profiling_stats.max_interrupt_latency = 0;
    
    /* Initialize stack canary with a random-ish value */
    /* In a real implementation, this should be properly randomized */
    __stack_chk_guard = STACK_CHK_GUARD;
    
    debug_initialized = true;
    
    /* Print initialization message */
    terminal_setcolor(vga_entry_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("Debug subsystem initialized with stack canaries\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
}

/**
 * @brief Get current profiling statistics
 */
const struct kernel_profiling* debug_get_profiling_stats(void) {
    return &profiling_stats;
}

/**
 * @brief Reset all profiling counters
 */
void debug_reset_profiling_stats(void) {
    if (!debug_initialized) return;
    
    profiling_stats.total_interrupts = 0;
    profiling_stats.timer_interrupts = 0;
    profiling_stats.keyboard_interrupts = 0;
    profiling_stats.spurious_interrupts = 0;
    profiling_stats.exceptions = 0;
    profiling_stats.page_faults = 0;
    profiling_stats.general_protection_faults = 0;
    profiling_stats.memory_allocations = 0;
    profiling_stats.memory_frees = 0;
    profiling_stats.memory_allocated_bytes = 0;
    profiling_stats.peak_memory_usage = 0;
    profiling_stats.system_calls = 0;
    profiling_stats.context_switches = 0;
    profiling_stats.max_interrupt_latency = 0;
}

/**
 * @brief Increment interrupt counter for profiling
 */
void debug_count_interrupt(uint8_t irq_num) {
    if (!debug_initialized) return;
    
    profiling_stats.total_interrupts++;
    
    switch (irq_num) {
        case 0:  /* Timer interrupt */
            profiling_stats.timer_interrupts++;
            break;
        case 1:  /* Keyboard interrupt */
            profiling_stats.keyboard_interrupts++;
            break;
        case 7:  /* Spurious IRQ7 */
        case 15: /* Spurious IRQ15 */
            profiling_stats.spurious_interrupts++;
            break;
        default:
            /* Other IRQs - just counted in total */
            break;
    }
}

/**
 * @brief Increment exception counter for profiling
 */
void debug_count_exception(uint8_t exception_num) {
    if (!debug_initialized) return;
    
    profiling_stats.exceptions++;
    
    switch (exception_num) {
        case 13: /* General Protection Fault */
            profiling_stats.general_protection_faults++;
            break;
        case 14: /* Page Fault */
            profiling_stats.page_faults++;
            break;
        default:
            /* Other exceptions - just counted in total */
            break;
    }
}

/**
 * @brief Track memory allocation for profiling
 */
void debug_count_memory_alloc(uint32_t bytes) {
    if (!debug_initialized) return;
    
    profiling_stats.memory_allocations++;
    profiling_stats.memory_allocated_bytes += bytes;
    
    /* Update peak memory usage */
    if (profiling_stats.memory_allocated_bytes > profiling_stats.peak_memory_usage) {
        profiling_stats.peak_memory_usage = profiling_stats.memory_allocated_bytes;
    }
}

/**
 * @brief Track memory deallocation for profiling
 */
void debug_count_memory_free(uint32_t bytes) {
    if (!debug_initialized) return;
    
    profiling_stats.memory_frees++;
    
    /* Prevent underflow */
    if (profiling_stats.memory_allocated_bytes >= bytes) {
        profiling_stats.memory_allocated_bytes -= bytes;
    } else {
        profiling_stats.memory_allocated_bytes = 0;
    }
}

/**
 * @brief Stack canary failure handler
 */
void __stack_chk_fail(void) {
    /* This function is called when GCC detects stack smashing */
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_RED));
    terminal_writestring("\n*** STACK SMASHING DETECTED ***\n");
    terminal_writestring("A buffer overflow has corrupted the stack!\n");
    terminal_writestring("This is a serious security vulnerability.\n");
    terminal_writestring("System halted to prevent further damage.\n");
    
    /* Count this as an exception for profiling */
    debug_count_exception(0xFF);  /* Special value for stack smashing */
    
    /* Halt the system immediately */
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_RED));
    terminal_writestring("SYSTEM HALTED\n");
    
    /* Disable interrupts and halt */
    __asm__ volatile ("cli");
    while (1) {
        __asm__ volatile ("hlt");
    }
}

/**
 * @brief Local stack canary failure handler (used by GCC in some cases)
 */
void __stack_chk_fail_local(void) {
    /* Just call the main stack check failure handler */
    __stack_chk_fail();
}

/**
 * @brief Manual stack canary check
 */
bool debug_check_stack_canary(uintptr_t canary_value) {
    return canary_value == __stack_chk_guard;
}

/**
 * @brief Kernel panic function
 */
void debug_panic(const char* format, ...) {
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_RED));
    terminal_writestring("\n*** KERNEL PANIC ***\n");
    
    /* For now, just print the format string directly */
    /* A full implementation would handle printf-style formatting */
    terminal_writestring(format);
    terminal_writestring("\n");
    
    terminal_writestring("System halted.\n");
    
    /* Disable interrupts and halt */
    __asm__ volatile ("cli");
    while (1) {
        __asm__ volatile ("hlt");
    }
}

/**
 * @brief Display profiling statistics to terminal
 */
void debug_print_profiling_stats(void) {
    if (!debug_initialized) {
        terminal_writestring("Debug subsystem not initialized\n");
        return;
    }
    
    char buffer[32];
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("\n=== KERNEL PROFILING STATISTICS ===\n");
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    
    /* Interrupt statistics */
    terminal_writestring("Interrupts:\n");
    
    terminal_writestring("  Total: ");
    debug_uint64_to_str(profiling_stats.total_interrupts, buffer, sizeof(buffer));
    terminal_writestring(buffer);
    terminal_writestring("\n");
    
    terminal_writestring("  Timer (IRQ0): ");
    debug_uint64_to_str(profiling_stats.timer_interrupts, buffer, sizeof(buffer));
    terminal_writestring(buffer);
    terminal_writestring("\n");
    
    terminal_writestring("  Keyboard (IRQ1): ");
    debug_uint64_to_str(profiling_stats.keyboard_interrupts, buffer, sizeof(buffer));
    terminal_writestring(buffer);
    terminal_writestring("\n");
    
    terminal_writestring("  Spurious: ");
    debug_uint64_to_str(profiling_stats.spurious_interrupts, buffer, sizeof(buffer));
    terminal_writestring(buffer);
    terminal_writestring("\n");
    
    /* Exception statistics */
    terminal_writestring("Exceptions:\n");
    
    terminal_writestring("  Total: ");
    debug_uint64_to_str(profiling_stats.exceptions, buffer, sizeof(buffer));
    terminal_writestring(buffer);
    terminal_writestring("\n");
    
    terminal_writestring("  Page Faults: ");
    debug_uint64_to_str(profiling_stats.page_faults, buffer, sizeof(buffer));
    terminal_writestring(buffer);
    terminal_writestring("\n");
    
    terminal_writestring("  GPF: ");
    debug_uint64_to_str(profiling_stats.general_protection_faults, buffer, sizeof(buffer));
    terminal_writestring(buffer);
    terminal_writestring("\n");
    
    /* Memory statistics */
    terminal_writestring("Memory:\n");
    
    terminal_writestring("  Allocations: ");
    debug_uint64_to_str(profiling_stats.memory_allocations, buffer, sizeof(buffer));
    terminal_writestring(buffer);
    terminal_writestring("\n");
    
    terminal_writestring("  Frees: ");
    debug_uint64_to_str(profiling_stats.memory_frees, buffer, sizeof(buffer));
    terminal_writestring(buffer);
    terminal_writestring("\n");
    
    terminal_writestring("  Current bytes: ");
    debug_uint32_to_str(profiling_stats.memory_allocated_bytes, buffer, sizeof(buffer));
    terminal_writestring(buffer);
    terminal_writestring("\n");
    
    terminal_writestring("  Peak bytes: ");
    debug_uint32_to_str(profiling_stats.peak_memory_usage, buffer, sizeof(buffer));
    terminal_writestring(buffer);
    terminal_writestring("\n");
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("===================================\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
}

/**
 * @brief Debug print function
 */
void debug_print(const char* message) {
    if (!debug_initialized) return;
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK));
    terminal_writestring("[DEBUG] ");
    terminal_writestring(message);
    terminal_writestring("\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
}
