/*------------------------------------------------------------------------------
 * Shell Driver
 *------------------------------------------------------------------------------
 * Simple command shell for SKOS providing basic user interaction.
 * Based on OSDev wiki principles for basic operating system shells.
 *------------------------------------------------------------------------------
 */

#include "shell.h"
#include "../kernel/kernel.h"
#include "../kernel/memory.h"
#include "../kernel/pic.h"
#include "../kernel/debug.h"
#include "../kernel/fat32.h"
#include "timer.h"
#include "keyboard.h"
#include "ata.h"

/* Forward declarations for helper functions */
static void print_hex32(uint32_t value);
static void print_hex16(uint16_t value);
static void print_hex8(uint8_t value);
static void shell_redraw_line(void);

/* I/O port functions (inline assembly) */
static inline void outb(uint16_t port, uint8_t value) {
    asm volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

/*------------------------------------------------------------------------------
 * Helper Functions for 64-bit Arithmetic
 *------------------------------------------------------------------------------
 * These avoid libgcc dependencies in freestanding environment
 *------------------------------------------------------------------------------
 */

/**
 * @brief 64-bit division helper
 */
static uint32_t div64_32(uint64_t dividend, uint32_t divisor) {
    if (divisor == 0) return 0;
    
    /* If dividend fits in 32 bits, use regular division */
    if (dividend <= 0xFFFFFFFF) {
        return (uint32_t)dividend / divisor;
    }
    
    /* For larger numbers, use successive subtraction (simple but works) */
    uint32_t result = 0;
    while (dividend >= divisor) {
        dividend -= divisor;
        result++;
        /* Prevent infinite loops on very large numbers */
        if (result > 0x7FFFFFFF) break;
    }
    return result;
}

/**
 * @brief Convert 64-bit number to string using custom arithmetic
 */
static void uint64_to_string(uint64_t value, char* buffer) {
    if (value == 0) {
        buffer[0] = '0';
        buffer[1] = '\0';
        return;
    }
    
    int i = 0;
    while (value > 0) {
        uint32_t remainder = (uint32_t)(value - (div64_32(value, 10) * 10));
        buffer[i++] = '0' + remainder;
        value = div64_32(value, 10);
    }
    
    /* Reverse the string */
    for (int j = 0; j < i/2; j++) {
        char temp = buffer[j];
        buffer[j] = buffer[i-1-j];
        buffer[i-1-j] = temp;
    }
    buffer[i] = '\0';
}

/* External terminal variables from kernel.c */
extern size_t terminal_row;
extern size_t terminal_column;
extern uint8_t terminal_color;
extern uint16_t* terminal_buffer;
extern size_t prompt_start_column;

/* Static variables for shell state */
static char command_buffer[SHELL_MAX_COMMAND_LENGTH];
static size_t command_length = 0;
static size_t cursor_position = 0;  /* Cursor position within command buffer */

/* Command table structure */
typedef struct {
    const char* name;
    void (*function)(const char* args);
    const char* description;
} shell_command_t;

/* Built-in commands table */
static const shell_command_t commands[] = {
    {"help", shell_cmd_help, "Show available commands"},
    {"clear", shell_cmd_clear, "Clear the screen"},
    {"mem", shell_cmd_mem, "Show memory information"},
    {"uptime", shell_cmd_uptime, "Show system uptime"},
    {"timer", shell_cmd_timer, "Show timer information"},
    {"sleep", shell_cmd_sleep, "Sleep for 3 seconds (demo)"},
    {"cpuid", shell_cmd_cpuid, "Show CPU information and features"},
    {"regs", shell_cmd_regs, "Show CPU register information"},
    {"irq", shell_cmd_irq, "Show interrupt controller status"},
    {"debug", shell_cmd_debug, "Show kernel profiling and debug statistics"},
    {"echo", shell_cmd_echo, "Echo text back"},
    {"reboot", shell_cmd_reboot, "Reboot the system"},
    {"scancode", shell_cmd_scancode, "Enter scancode debug mode (press q to quit)"},
    {"ls", shell_cmd_ls, "List files in current directory"},
    {"cat", shell_cmd_cat, "Display contents of a file"},
    {"write", shell_cmd_write, "Write text to a file (usage: write filename text)"},
    {"fsinfo", shell_cmd_fsinfo, "Show file system information"}
};

#define NUM_COMMANDS (sizeof(commands) / sizeof(commands[0]))

/*------------------------------------------------------------------------------
 * Utility Functions
 *------------------------------------------------------------------------------
 */

/* Case-insensitive string comparison function */
bool shell_strcmp(const char* str1, const char* str2) {
    if (str1 == NULL || str2 == NULL) {
        return false;
    }
    
    while (*str1 && *str2) {
        char c1 = *str1;
        char c2 = *str2;
        
        /* Convert to lowercase for comparison */
        if (c1 >= 'A' && c1 <= 'Z') {
            c1 = c1 + 32;  /* Convert uppercase to lowercase */
        }
        if (c2 >= 'A' && c2 <= 'Z') {
            c2 = c2 + 32;  /* Convert uppercase to lowercase */
        }
        
        if (c1 != c2) {
            return false;
        }
        str1++;
        str2++;
    }
    
    return (*str1 == *str2);
}

/* Simple string length function */
size_t shell_strlen(const char* str) {
    size_t len = 0;
    if (str) {
        while (str[len]) {
            len++;
        }
    }
    return len;
}

/* Parse command line into command and arguments */
static const char* shell_parse_command(const char* cmdline, char* cmd_name, size_t cmd_name_size) {
    if (!cmdline || !cmd_name || cmd_name_size == 0) {
        return NULL;
    }
    
    /* Skip leading whitespace */
    while (*cmdline == ' ' || *cmdline == '\t') {
        cmdline++;
    }
    
    /* Extract command name */
    size_t i = 0;
    while (*cmdline && *cmdline != ' ' && *cmdline != '\t' && i < cmd_name_size - 1) {
        cmd_name[i++] = *cmdline++;
    }
    cmd_name[i] = '\0';
    
    /* Skip whitespace after command */
    while (*cmdline == ' ' || *cmdline == '\t') {
        cmdline++;
    }
    
    /* Return pointer to arguments (or NULL if no args) */
    return (*cmdline) ? cmdline : NULL;
}

/* Print shell prompt */
void shell_print_prompt(void) {
    terminal_writestring("skos~$ ");
    /* Update the prompt start column for backspace handling */
    prompt_start_column = terminal_column;
    cursor_position = 0;  /* Reset cursor position for new command */
    terminal_update_cursor();
}

/* Redraw the current command line */
static void shell_redraw_line(void) {
    /* Save current cursor position */
    size_t saved_cursor_pos = cursor_position;
    
    /* Move to start of input area */
    terminal_move_cursor_home();
    
    /* Clear the rest of the line */
    terminal_clear_line_from_cursor();
    
    /* Redraw the command buffer */
    for (size_t i = 0; i < command_length; i++) {
        terminal_putchar(command_buffer[i]);
    }
    
    /* Move cursor to correct position */
    terminal_column = prompt_start_column + saved_cursor_pos;
    terminal_update_cursor();
}

/*------------------------------------------------------------------------------
 * Built-in Commands
 *------------------------------------------------------------------------------
 */

/* Help command - shows all available commands */
void shell_cmd_help(const char* args) {
    (void)args; /* Unused parameter */
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("\n=== SKOS SHELL COMMANDS ===\n\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    
    for (size_t i = 0; i < NUM_COMMANDS; i++) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK));
        terminal_writestring("  ");
        terminal_writestring(commands[i].name);
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        terminal_writestring(" - ");
        terminal_writestring(commands[i].description);
        terminal_writestring("\n");
    }
    terminal_writestring("\n");
}

/* Clear command - clears the screen */
void shell_cmd_clear(const char* args) {
    (void)args; /* Unused parameter */
    terminal_initialize();  /* Re-initialize terminal to clear screen */
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
}

/* Memory command - shows memory information */
void shell_cmd_mem(const char* args) {
    (void)args; /* Unused parameter */
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("\n=== MEMORY INFORMATION ===\n\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    
    /* Call the memory management function to print detailed stats */
    memory_print_stats();
    terminal_writestring("\n");
}

/* Uptime command - shows system uptime */
void shell_cmd_uptime(const char* args) {
    (void)args; /* Unused parameter */
    if (!timer_is_initialized()) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("Timer not initialized!\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        return;
    }
    
    uint64_t uptime_ms = timer_get_uptime_ms();
    uint64_t ticks = timer_get_ticks();
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("\n=== SYSTEM UPTIME ===\n\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    
    /* Calculate days, hours, minutes, seconds */
    uint32_t days = div64_32(uptime_ms, 86400000);  /* 86400 seconds * 1000 ms */
    uint64_t remaining_ms = uptime_ms - (uint64_t)days * 86400000;
    
    uint32_t hours = div64_32(remaining_ms, 3600000);  /* 3600 seconds * 1000 ms */
    remaining_ms -= (uint64_t)hours * 3600000;
    
    uint32_t minutes = div64_32(remaining_ms, 60000);  /* 60 seconds * 1000 ms */
    remaining_ms -= (uint64_t)minutes * 60000;
    
    uint32_t seconds = div64_32(remaining_ms, 1000);   /* Convert ms to seconds */
    
    /* Print uptime */
    terminal_writestring("  Uptime: ");
    
    /* Print days */
    if (days > 0) {
        char day_str[16];
        int i = 0;
        uint32_t temp = days;
        while (temp > 0) {
            day_str[i++] = '0' + (temp % 10);
            temp /= 10;
        }
        for (int j = 0; j < i/2; j++) {
            char temp_c = day_str[j];
            day_str[j] = day_str[i-1-j];
            day_str[i-1-j] = temp_c;
        }
        day_str[i] = '\0';
        terminal_writestring(day_str);
        terminal_writestring(" days, ");
    }
    
    /* Print hours */
    char hour_str[8];
    int i = 0;
    if (hours < 10) hour_str[i++] = '0';
    uint32_t temp = hours;
    if (temp == 0) {
        hour_str[i++] = '0';
    } else {
        int start = i;
        while (temp > 0) {
            hour_str[i++] = '0' + (temp % 10);
            temp /= 10;
        }
        for (int j = start; j < start + (i - start)/2; j++) {
            char temp_c = hour_str[j];
            hour_str[j] = hour_str[i-1-(j-start)];
            hour_str[i-1-(j-start)] = temp_c;
        }
    }
    hour_str[i] = '\0';
    terminal_writestring(hour_str);
    terminal_writestring(":");
    
    /* Print minutes */
    char min_str[8];
    i = 0;
    if (minutes < 10) min_str[i++] = '0';
    temp = minutes;
    if (temp == 0) {
        min_str[i++] = '0';
    } else {
        int start = i;
        while (temp > 0) {
            min_str[i++] = '0' + (temp % 10);
            temp /= 10;
        }
        for (int j = start; j < start + (i - start)/2; j++) {
            char temp_c = min_str[j];
            min_str[j] = min_str[i-1-(j-start)];
            min_str[i-1-(j-start)] = temp_c;
        }
    }
    min_str[i] = '\0';
    terminal_writestring(min_str);
    terminal_writestring(":");
    
    /* Print seconds */
    char sec_str[8];
    i = 0;
    if (seconds < 10) sec_str[i++] = '0';
    temp = seconds;
    if (temp == 0) {
        sec_str[i++] = '0';
    } else {
        int start = i;
        while (temp > 0) {
            sec_str[i++] = '0' + (temp % 10);
            temp /= 10;
        }
        for (int j = start; j < start + (i - start)/2; j++) {
            char temp_c = sec_str[j];
            sec_str[j] = sec_str[i-1-(j-start)];
            sec_str[i-1-(j-start)] = temp_c;
        }
    }
    sec_str[i] = '\0';
    terminal_writestring(sec_str);
    terminal_writestring("\n");
    
    /* Print milliseconds */
    terminal_writestring("  Milliseconds: ");
    char ms_str[16];
    uint64_to_string(uptime_ms, ms_str);
    terminal_writestring(ms_str);
    terminal_writestring(" ms\n");
    
    /* Print timer ticks */
    terminal_writestring("  Timer ticks: ");
    char tick_str[32];
    uint64_to_string(ticks, tick_str);
    terminal_writestring(tick_str);
    terminal_writestring("\n\n");
}

/* Timer command - shows timer information */
void shell_cmd_timer(const char* args) {
    (void)args; /* Unused parameter */
    if (!timer_is_initialized()) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("Timer not initialized!\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        return;
    }
    
    struct timer_info info;
    timer_get_info(&info);
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("\n=== TIMER INFORMATION ===\n\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    
    /* Print frequency */
    terminal_writestring("  Frequency: ");
    char freq_str[16];
    int i = 0;
    uint32_t temp = info.frequency;
    if (temp == 0) {
        freq_str[i++] = '0';
    } else {
        while (temp > 0) {
            freq_str[i++] = '0' + (temp % 10);
            temp /= 10;
        }
        for (int j = 0; j < i/2; j++) {
            char temp_c = freq_str[j];
            freq_str[j] = freq_str[i-1-j];
            freq_str[i-1-j] = temp_c;
        }
    }
    freq_str[i] = '\0';
    terminal_writestring(freq_str);
    terminal_writestring(" Hz\n");
    
    /* Print reload value */
    terminal_writestring("  PIT reload: ");
    char reload_str[16];
    i = 0;
    temp = info.reload_value;
    if (temp == 0) {
        reload_str[i++] = '6';
        reload_str[i++] = '5';
        reload_str[i++] = '5';
        reload_str[i++] = '3';
        reload_str[i++] = '6';
    } else {
        while (temp > 0) {
            reload_str[i++] = '0' + (temp % 10);
            temp /= 10;
        }
        for (int j = 0; j < i/2; j++) {
            char temp_c = reload_str[j];
            reload_str[j] = reload_str[i-1-j];
            reload_str[i-1-j] = temp_c;
        }
    }
    reload_str[i] = '\0';
    terminal_writestring(reload_str);
    terminal_writestring("\n");
    
    /* Print ms per tick */
    terminal_writestring("  MS per tick: ");
    char ms_tick_str[16];
    i = 0;
    temp = info.ms_per_tick;
    if (temp == 0) {
        ms_tick_str[i++] = '0';
    } else {
        while (temp > 0) {
            ms_tick_str[i++] = '0' + (temp % 10);
            temp /= 10;
        }
        for (int j = 0; j < i/2; j++) {
            char temp_c = ms_tick_str[j];
            ms_tick_str[j] = ms_tick_str[i-1-j];
            ms_tick_str[i-1-j] = temp_c;
        }
    }
    ms_tick_str[i] = '\0';
    terminal_writestring(ms_tick_str);
    terminal_writestring(" ms\n\n");
}

/* Sleep command - demonstrates timer sleep functionality */
void shell_cmd_sleep(const char* args) {
    (void)args; /* Unused parameter */
    if (!timer_is_initialized()) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("Timer not initialized!\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        return;
    }
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("Sleeping for 3 seconds...\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    
    /* Sleep for 3 seconds */
    timer_sleep_seconds(3);
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("Sleep complete!\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
}

/* CPUID command - shows CPU information and features */
void shell_cmd_cpuid(const char* args) {
    (void)args; /* Unused parameter */
    uint32_t eax, ebx, ecx, edx;
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("\n=== CPU INFORMATION ===\n\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    
    /* Get vendor string */
    asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(0));
    
    terminal_writestring("  Vendor ID: ");
    char vendor[13] = {0};
    *(uint32_t*)(vendor + 0) = ebx;
    *(uint32_t*)(vendor + 4) = edx;
    *(uint32_t*)(vendor + 8) = ecx;
    terminal_writestring(vendor);
    terminal_writestring("\n");
    
    /* Get CPU features */
    if (eax >= 1) {
        asm volatile("cpuid" : "=a"(eax), "=b"(ebx), "=c"(ecx), "=d"(edx) : "a"(1));
        
        terminal_writestring("  Model: ");
        uint32_t model = (eax >> 4) & 0xF;
        uint32_t family = (eax >> 8) & 0xF;
        char model_str[16];
        int i = 0;
        if (family == 0) family = 15; /* Extended family */
        uint32_t temp = family;
        while (temp > 0) {
            model_str[i++] = '0' + (temp % 10);
            temp /= 10;
        }
        /* Reverse string */
        for (int j = 0; j < i/2; j++) {
            char temp_c = model_str[j];
            model_str[j] = model_str[i-1-j];
            model_str[i-1-j] = temp_c;
        }
        model_str[i] = '\0';
        terminal_writestring(model_str);
        terminal_writestring(".");
        
        i = 0;
        temp = model;
        if (temp == 0) {
            model_str[i++] = '0';
        } else {
            while (temp > 0) {
                model_str[i++] = '0' + (temp % 10);
                temp /= 10;
            }
            /* Reverse string */
            for (int j = 0; j < i/2; j++) {
                char temp_c = model_str[j];
                model_str[j] = model_str[i-1-j];
                model_str[i-1-j] = temp_c;
            }
        }
        model_str[i] = '\0';
        terminal_writestring(model_str);
        terminal_writestring("\n");
        
        terminal_writestring("  Features: ");
        if (edx & (1 << 0)) terminal_writestring("FPU ");
        if (edx & (1 << 4)) terminal_writestring("TSC ");
        if (edx & (1 << 5)) terminal_writestring("MSR ");
        if (edx & (1 << 6)) terminal_writestring("PAE ");
        if (edx & (1 << 8)) terminal_writestring("CX8 ");
        if (edx & (1 << 9)) terminal_writestring("APIC ");
        if (edx & (1 << 15)) terminal_writestring("CMOV ");
        if (edx & (1 << 23)) terminal_writestring("MMX ");
        if (edx & (1 << 25)) terminal_writestring("SSE ");
        if (edx & (1 << 26)) terminal_writestring("SSE2 ");
        if (ecx & (1 << 0)) terminal_writestring("SSE3 ");
        terminal_writestring("\n");
    }
    
    terminal_writestring("\n");
}

/* Register command - shows CPU register information */
void shell_cmd_regs(const char* args) {
    (void)args; /* Unused parameter */
    uint32_t eax, ebx, ecx, edx, esp, ebp, esi, edi;
    uint32_t cr0, cr2, cr3;
    uint16_t cs, ds, es, fs, gs, ss;
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("\n=== CPU REGISTERS ===\n\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    
    /* Get general purpose registers */
    asm volatile("mov %%eax, %0" : "=m"(eax));
    asm volatile("mov %%ebx, %0" : "=m"(ebx));
    asm volatile("mov %%ecx, %0" : "=m"(ecx));
    asm volatile("mov %%edx, %0" : "=m"(edx));
    asm volatile("mov %%esp, %0" : "=m"(esp));
    asm volatile("mov %%ebp, %0" : "=m"(ebp));
    asm volatile("mov %%esi, %0" : "=m"(esi));
    asm volatile("mov %%edi, %0" : "=m"(edi));
    
    /* Get segment registers */
    asm volatile("mov %%cs, %0" : "=m"(cs));
    asm volatile("mov %%ds, %0" : "=m"(ds));
    asm volatile("mov %%es, %0" : "=m"(es));
    asm volatile("mov %%fs, %0" : "=m"(fs));
    asm volatile("mov %%gs, %0" : "=m"(gs));
    asm volatile("mov %%ss, %0" : "=m"(ss));
    
    /* Get control registers */
    asm volatile("mov %%cr0, %%eax; mov %%eax, %0" : "=m"(cr0) : : "eax");
    asm volatile("mov %%cr2, %%eax; mov %%eax, %0" : "=m"(cr2) : : "eax");
    asm volatile("mov %%cr3, %%eax; mov %%eax, %0" : "=m"(cr3) : : "eax");
    
    /* Print general purpose registers */
    terminal_writestring("  General Purpose:\n");
    terminal_writestring("    EAX: 0x"); print_hex32(eax); terminal_writestring("\n");
    terminal_writestring("    EBX: 0x"); print_hex32(ebx); terminal_writestring("\n");
    terminal_writestring("    ECX: 0x"); print_hex32(ecx); terminal_writestring("\n");
    terminal_writestring("    EDX: 0x"); print_hex32(edx); terminal_writestring("\n");
    terminal_writestring("    ESP: 0x"); print_hex32(esp); terminal_writestring("\n");
    terminal_writestring("    EBP: 0x"); print_hex32(ebp); terminal_writestring("\n");
    terminal_writestring("    ESI: 0x"); print_hex32(esi); terminal_writestring("\n");
    terminal_writestring("    EDI: 0x"); print_hex32(edi); terminal_writestring("\n");
    
    /* Print segment registers */
    terminal_writestring("  Segment Registers:\n");
    terminal_writestring("    CS: 0x"); print_hex16(cs); terminal_writestring("\n");
    terminal_writestring("    DS: 0x"); print_hex16(ds); terminal_writestring("\n");
    terminal_writestring("    ES: 0x"); print_hex16(es); terminal_writestring("\n");
    terminal_writestring("    FS: 0x"); print_hex16(fs); terminal_writestring("\n");
    terminal_writestring("    GS: 0x"); print_hex16(gs); terminal_writestring("\n");
    terminal_writestring("    SS: 0x"); print_hex16(ss); terminal_writestring("\n");
    
    /* Print control registers */
    terminal_writestring("  Control Registers:\n");
    terminal_writestring("    CR0: 0x"); print_hex32(cr0); terminal_writestring(" (PE=");
    terminal_writestring((cr0 & 1) ? "1" : "0");
    terminal_writestring(", PG=");
    terminal_writestring((cr0 & 0x80000000) ? "1" : "0");
    terminal_writestring(")\n");
    terminal_writestring("    CR2: 0x"); print_hex32(cr2); terminal_writestring(" (Page Fault Linear Address)\n");
    terminal_writestring("    CR3: 0x"); print_hex32(cr3); terminal_writestring(" (Page Directory Base)\n");
    
    terminal_writestring("\n");
}

/* IRQ command - shows interrupt controller status */
void shell_cmd_irq(const char* args) {
    (void)args; /* Unused parameter */
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("\n=== INTERRUPT CONTROLLER STATUS ===\n\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    
    /* Get PIC mask registers */
    uint8_t master_mask = pic_get_mask_master();
    uint8_t slave_mask = pic_get_mask_slave();
    
    /* Get PIC ISR registers */
    uint8_t master_isr = pic_read_isr_master();
    uint8_t slave_isr = pic_read_isr_slave();
    
    /* Get PIC IRR registers */
    uint8_t master_irr = pic_read_irr_master();
    uint8_t slave_irr = pic_read_irr_slave();
    
    terminal_writestring("  Master PIC (IRQ 0-7):\n");
    terminal_writestring("    Mask:  0x"); print_hex8(master_mask); terminal_writestring(" (1=disabled)\n");
    terminal_writestring("    ISR:   0x"); print_hex8(master_isr); terminal_writestring(" (1=in service)\n");
    terminal_writestring("    IRR:   0x"); print_hex8(master_irr); terminal_writestring(" (1=pending)\n");
    
    terminal_writestring("  Slave PIC (IRQ 8-15):\n");
    terminal_writestring("    Mask:  0x"); print_hex8(slave_mask); terminal_writestring(" (1=disabled)\n");
    terminal_writestring("    ISR:   0x"); print_hex8(slave_isr); terminal_writestring(" (1=in service)\n");
    terminal_writestring("    IRR:   0x"); print_hex8(slave_irr); terminal_writestring(" (1=pending)\n");
    
    terminal_writestring("  IRQ Status:\n");
    for (int i = 0; i < 8; i++) {
        terminal_writestring("    IRQ");
        char irq_str[8];
        irq_str[0] = '0' + i;
        irq_str[1] = ':';
        irq_str[2] = ' ';
        irq_str[3] = (master_mask & (1 << i)) ? 'D' : 'E'; /* Disabled/Enabled */
        irq_str[4] = ' ';
        irq_str[5] = (master_isr & (1 << i)) ? 'S' : '-';  /* In Service */
        irq_str[6] = ' ';
        irq_str[7] = (master_irr & (1 << i)) ? 'P' : '-';  /* Pending */
        irq_str[8] = '\0';
        terminal_writestring(irq_str);
        terminal_writestring("\n");
    }
    
    for (int i = 0; i < 8; i++) {
        terminal_writestring("    IRQ");
        char irq_str[16];
        irq_str[0] = '1';
        irq_str[1] = '0' + i;
        irq_str[2] = ':';
        irq_str[3] = ' ';
        irq_str[4] = (slave_mask & (1 << i)) ? 'D' : 'E';
        irq_str[5] = ' ';
        irq_str[6] = (slave_isr & (1 << i)) ? 'S' : '-';
        irq_str[7] = ' ';
        irq_str[8] = (slave_irr & (1 << i)) ? 'P' : '-';
        irq_str[9] = '\0';
        terminal_writestring(irq_str);
        terminal_writestring("\n");
    }
    
    terminal_writestring("\n");
}

/* Echo command - echoes text back */
void shell_cmd_echo(const char* args) {
    (void)args; /* Unused parameter */
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("Echo: This is the echo command!\n");
    terminal_writestring("Note: Argument parsing not yet implemented.\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
}

/* Debug command - shows kernel profiling and debug statistics */
void shell_cmd_debug(const char* args) {
    (void)args; /* Unused parameter */
    debug_print_profiling_stats();
}

/* Reboot command - reboots the system */
void shell_cmd_reboot(const char* args) {
    (void)args; /* Unused parameter */
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK));
    terminal_writestring("Rebooting system...\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    
    /* Give user a moment to see the message */
    if (timer_is_initialized()) {
        timer_sleep_seconds(1);
    }
    
    /* Reboot using keyboard controller */
    outb(0x64, 0xFE);
    
    /* If that doesn't work, try triple fault */
    asm volatile("cli");
    asm volatile("hlt");
}

/* Scancode command - enters scancode debug mode */
void shell_cmd_scancode(const char* args) {
    (void)args; /* Unused parameter */
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("\n=== SCANCODE DEBUG MODE ===\n\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    terminal_writestring("Entering scancode debug mode...\n");
    terminal_writestring("Press any keys to see their scancode details.\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK));
    terminal_writestring("Press 'q' to quit debug mode.\n\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    
    /* Enable debug mode */
    keyboard_enable_debug_mode();
    
    /* Wait in a loop until debug mode is disabled (by pressing 'q') */
    while (keyboard_is_debug_mode_active()) {
        /* Halt CPU until next interrupt */
        asm volatile ("hlt");
    }
    
    /* Debug mode exited, print prompt */
    shell_print_prompt();
}

/* List files command - shows files in current directory */
void shell_cmd_ls(const char* args) {
    (void)args; /* Unused parameter */
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("\n=== DIRECTORY LISTING ===\n\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    
    /* Check if FAT32 is initialized */
    fat32_fs_info_t* fs_info = fat32_get_fs_info();
    if (!fs_info) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("File system not initialized!\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        return;
    }
    
    /* Open root directory */
    fat32_dir_t* dir = fat32_opendir("/");
    if (!dir) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("Failed to open root directory!\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        return;
    }
    
    /* Read and display directory entries */
    fat32_dir_entry_t* entry;
    int file_count = 0;
    
    while ((entry = fat32_readdir(dir)) != NULL) {
        /* Set color based on file type */
        if (entry->attributes & FAT_ATTR_DIRECTORY) {
            terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK));
        } else {
            terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
        }
        
        fat32_print_file_info(entry);
        file_count++;
    }
    
    fat32_closedir(dir);
    
    /* Print summary */
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    terminal_writestring("\nTotal entries: ");
    
    /* Print file count */
    char count_str[16];
    int digits = 0;
    int temp = file_count;
    
    if (temp == 0) {
        count_str[0] = '0';
        digits = 1;
    } else {
        while (temp > 0) {
            count_str[digits++] = '0' + (temp % 10);
            temp /= 10;
        }
    }
    
    /* Reverse the string */
    for (int i = 0; i < digits / 2; i++) {
        char temp_c = count_str[i];
        count_str[i] = count_str[digits - 1 - i];
        count_str[digits - 1 - i] = temp_c;
    }
    count_str[digits] = '\0';
    
    terminal_writestring(count_str);
    terminal_writestring("\n\n");
}

/* Cat command - display file contents */
void shell_cmd_cat(const char* args) {
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("\n=== FILE CONTENTS ===\n\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    
    /* Check if FAT32 is initialized */
    fat32_fs_info_t* fs_info = fat32_get_fs_info();
    if (!fs_info) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("File system not initialized!\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        return;
    }
    
    /* Check if filename argument was provided */
    const char* filename = args;
    if (!filename || shell_strlen(filename) == 0) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("Usage: cat <filename>\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        terminal_writestring("Example: cat README.TXT\n\n");
        return;
    }
    
    terminal_writestring("Attempting to read file: ");
    terminal_writestring(filename);
    terminal_writestring("\n\n");
    
    /* Open the file */
    fat32_file_t* file = fat32_open(filename);
    if (!file) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("File not found: ");
        terminal_writestring(filename);
        terminal_writestring("\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        terminal_writestring("Use 'ls' to see available files.\n\n");
        return;
    }
    
    /* Read and display file contents */
    char buffer[513];  /* 512 bytes + null terminator */
    size_t bytes_read;
    size_t total_bytes = 0;
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    
    while ((bytes_read = fat32_read(file, buffer, 512)) > 0) {
        buffer[bytes_read] = '\0';  /* Null terminate for display */
        
        /* Display the content, handling special characters */
        for (size_t i = 0; i < bytes_read; i++) {
            char c = buffer[i];
            if (c == '\n') {
                terminal_putchar('\n');
            } else if (c == '\t') {
                terminal_writestring("    ");  /* Replace tabs with spaces */
            } else if (c >= 32 && c <= 126) {
                terminal_putchar(c);  /* Printable ASCII */
            } else {
                terminal_putchar('?');  /* Replace non-printable with ? */
            }
        }
        
        total_bytes += bytes_read;
        
        /* Break if we've read enough to avoid screen overflow */
        if (total_bytes > 2048) {
            terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK));
            terminal_writestring("\n\n[... truncated after 2KB ...]");
            break;
        }
    }
    
    fat32_close(file);
    
    /* Print file statistics */
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    terminal_writestring("\n\nBytes displayed: ");
    
    /* Print total bytes */
    char bytes_str[16];
    int digits = 0;
    int temp = total_bytes;
    
    if (temp == 0) {
        bytes_str[0] = '0';
        digits = 1;
    } else {
        while (temp > 0) {
            bytes_str[digits++] = '0' + (temp % 10);
            temp /= 10;
        }
    }
    
    /* Reverse the string */
    for (int i = 0; i < digits / 2; i++) {
        char temp_c = bytes_str[i];
        bytes_str[i] = bytes_str[digits - 1 - i];
        bytes_str[digits - 1 - i] = temp_c;
    }
    bytes_str[digits] = '\0';
    
    terminal_writestring(bytes_str);
    terminal_writestring("\n\n");
}

/* Write text to file command */
void shell_cmd_write(const char* args) {
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("\n=== WRITE TO FILE ===\n\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    
    /* Check if FAT32 is initialized */
    fat32_fs_info_t* fs_info = fat32_get_fs_info();
    if (!fs_info) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("File system not initialized!\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        return;
    }
    
    /* Parse arguments: write filename text */
    if (!args || shell_strlen(args) == 0) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("Usage: write <filename> <text>\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        terminal_writestring("Example: write test.txt Hello World!\n\n");
        return;
    }
    
    /* Find first space to separate filename from text */
    const char* filename = args;
    const char* text = NULL;
    
    size_t filename_len = 0;
    while (args[filename_len] && args[filename_len] != ' ') {
        filename_len++;
    }
    
    if (args[filename_len] == ' ') {
        /* Skip spaces to find text */
        text = &args[filename_len];
        while (*text == ' ') {
            text++;
        }
    }
    
    if (!text || shell_strlen(text) == 0) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("No text provided to write!\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        return;
    }
    
    /* Create a null-terminated filename */
    char filename_buffer[32];
    if (filename_len >= sizeof(filename_buffer)) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("Filename too long!\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        return;
    }
    
    for (size_t i = 0; i < filename_len; i++) {
        filename_buffer[i] = filename[i];
    }
    filename_buffer[filename_len] = '\0';
    
    terminal_writestring("Writing to file: ");
    terminal_writestring(filename_buffer);
    terminal_writestring("\n");
    terminal_writestring("Text: ");
    terminal_writestring(text);
    terminal_writestring("\n\n");
    
    /* Create or open the file for writing */
    fat32_file_t* file = fat32_create(filename_buffer);
    if (!file) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("Failed to create/open file: ");
        terminal_writestring(filename_buffer);
        terminal_writestring("\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        return;
    }
    
    /* Write the text to the file */
    size_t text_len = shell_strlen(text);
    size_t bytes_written = fat32_write(file, text, text_len);
    
    /* Add a newline at the end */
    if (bytes_written == text_len) {
        fat32_write(file, "\n", 1);
        bytes_written++;
    }
    
    fat32_close(file);
    
    /* Report success */
    if (bytes_written > 0) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
        terminal_writestring("Successfully wrote ");
        
        /* Print bytes written */
        char bytes_str[16];
        int digits = 0;
        size_t temp = bytes_written;
        
        if (temp == 0) {
            bytes_str[0] = '0';
            digits = 1;
        } else {
            while (temp > 0) {
                bytes_str[digits++] = '0' + (temp % 10);
                temp /= 10;
            }
        }
        
        /* Reverse the string */
        for (int i = 0; i < digits / 2; i++) {
            char temp_c = bytes_str[i];
            bytes_str[i] = bytes_str[digits - 1 - i];
            bytes_str[digits - 1 - i] = temp_c;
        }
        bytes_str[digits] = '\0';
        
        terminal_writestring(bytes_str);
        terminal_writestring(" bytes to ");
        terminal_writestring(filename_buffer);
        terminal_writestring("\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    } else {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("Failed to write to file!\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    }
    
    terminal_writestring("\n");
}

/* File system info command - shows FAT32 information */
void shell_cmd_fsinfo(const char* args) {
    (void)args; /* Unused parameter */
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("\n=== FILE SYSTEM INFORMATION ===\n\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    
    /* Check if FAT32 is initialized */
    fat32_fs_info_t* fs_info = fat32_get_fs_info();
    if (!fs_info) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("File system not initialized!\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        return;
    }
    
    /* Display file system information */
    terminal_writestring("File System Type: FAT32\n");
    
    terminal_writestring("Bytes per Sector: ");
    char value_str[16];
    int digits = 0;
    uint32_t temp = fs_info->boot_sector.bytes_per_sector;
    
    if (temp == 0) {
        value_str[0] = '0';
        digits = 1;
    } else {
        while (temp > 0) {
            value_str[digits++] = '0' + (temp % 10);
            temp /= 10;
        }
    }
    
    /* Reverse the string */
    for (int i = 0; i < digits / 2; i++) {
        char temp_c = value_str[i];
        value_str[i] = value_str[digits - 1 - i];
        value_str[digits - 1 - i] = temp_c;
    }
    value_str[digits] = '\0';
    
    terminal_writestring(value_str);
    terminal_writestring("\n");
    
    terminal_writestring("Sectors per Cluster: ");
    digits = 0;
    temp = fs_info->sectors_per_cluster;
    
    if (temp == 0) {
        value_str[0] = '0';
        digits = 1;
    } else {
        while (temp > 0) {
            value_str[digits++] = '0' + (temp % 10);
            temp /= 10;
        }
    }
    
    /* Reverse the string */
    for (int i = 0; i < digits / 2; i++) {
        char temp_c = value_str[i];
        value_str[i] = value_str[digits - 1 - i];
        value_str[digits - 1 - i] = temp_c;
    }
    value_str[digits] = '\0';
    
    terminal_writestring(value_str);
    terminal_writestring("\n");
    
    terminal_writestring("Bytes per Cluster: ");
    digits = 0;
    temp = fs_info->bytes_per_cluster;
    
    if (temp == 0) {
        value_str[0] = '0';
        digits = 1;
    } else {
        while (temp > 0) {
            value_str[digits++] = '0' + (temp % 10);
            temp /= 10;
        }
    }
    
    /* Reverse the string */
    for (int i = 0; i < digits / 2; i++) {
        char temp_c = value_str[i];
        value_str[i] = value_str[digits - 1 - i];
        value_str[digits - 1 - i] = temp_c;
    }
    value_str[digits] = '\0';
    
    terminal_writestring(value_str);
    terminal_writestring("\n");
    
    terminal_writestring("Total Clusters: ");
    digits = 0;
    temp = fs_info->total_clusters;
    
    if (temp == 0) {
        value_str[0] = '0';
        digits = 1;
    } else {
        while (temp > 0) {
            value_str[digits++] = '0' + (temp % 10);
            temp /= 10;
        }
    }
    
    /* Reverse the string */
    for (int i = 0; i < digits / 2; i++) {
        char temp_c = value_str[i];
        value_str[i] = value_str[digits - 1 - i];
        value_str[digits - 1 - i] = temp_c;
    }
    value_str[digits] = '\0';
    
    terminal_writestring(value_str);
    terminal_writestring("\n");
    
    terminal_writestring("Root Directory Cluster: ");
    digits = 0;
    temp = fs_info->root_dir_cluster;
    
    if (temp == 0) {
        value_str[0] = '0';
        digits = 1;
    } else {
        while (temp > 0) {
            value_str[digits++] = '0' + (temp % 10);
            temp /= 10;
        }
    }
    
    /* Reverse the string */
    for (int i = 0; i < digits / 2; i++) {
        char temp_c = value_str[i];
        value_str[i] = value_str[digits - 1 - i];
        value_str[digits - 1 - i] = temp_c;
    }
    value_str[digits] = '\0';
    
    terminal_writestring(value_str);
    terminal_writestring("\n");
    
    /* Display volume label if available */
    terminal_writestring("Volume Label: ");
    char label[12];
    for (int i = 0; i < 11; i++) {
        label[i] = fs_info->boot_sector.volume_label[i];
        if (label[i] == 0) break;
    }
    label[11] = '\0';
    
    /* Trim trailing spaces */
    for (int i = 10; i >= 0 && label[i] == ' '; i--) {
        label[i] = '\0';
    }
    
    if (label[0] != '\0') {
        terminal_writestring(label);
    } else {
        terminal_writestring("(none)");
    }
    terminal_writestring("\n");
    
    /* Show storage device information */
    terminal_writestring("\nStorage Device Information:\n");
    ata_device_t* device = ata_get_primary_master();
    if (!device) {
        device = ata_get_primary_slave();
    }
    
    if (device) {
        ata_print_device_info(device);
    } else {
        terminal_writestring("  No storage device detected\n");
    }
    
    terminal_writestring("\n");
}

/* Helper functions for hex printing */
static void print_hex32(uint32_t value) {
    for (int i = 28; i >= 0; i -= 4) {
        int digit = (value >> i) & 0xF;
        terminal_putchar(digit < 10 ? '0' + digit : 'A' + digit - 10);
    }
}

static void print_hex16(uint16_t value) {
    for (int i = 12; i >= 0; i -= 4) {
        int digit = (value >> i) & 0xF;
        terminal_putchar(digit < 10 ? '0' + digit : 'A' + digit - 10);
    }
}

static void print_hex8(uint8_t value) {
    for (int i = 4; i >= 0; i -= 4) {
        int digit = (value >> i) & 0xF;
        terminal_putchar(digit < 10 ? '0' + digit : 'A' + digit - 10);
    }
}

/*------------------------------------------------------------------------------
 * Shell Main Functions
 *------------------------------------------------------------------------------
 */

/* Initialize the shell */
void shell_init(void) {
    command_length = 0;
    cursor_position = 0;
    command_buffer[0] = '\0';
    
    /* Shell is now ready - no need for verbose messages during boot */
}

/* Process a complete command */
void shell_process_command(const char* command) {
    /* Skip empty commands */
    if (command == NULL || shell_strlen(command) == 0) {
        return;
    }
    
    /* Parse command line into command name and arguments */
    char cmd_name[32];
    const char* args = shell_parse_command(command, cmd_name, sizeof(cmd_name));
    
    /* Look for matching command */
    bool command_found = false;
    for (size_t i = 0; i < NUM_COMMANDS; i++) {
        if (shell_strcmp(cmd_name, commands[i].name)) {
            commands[i].function(args);
            command_found = true;
            break;
        }
    }
    
    /* If command not found, show error */
    if (!command_found) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("Unknown command: '");
        terminal_writestring(cmd_name);
        terminal_writestring("'\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        terminal_writestring("Type 'help' for available commands\n");
    }
}

/* Handle individual character input */
void shell_handle_input(int c) {
    if (c == '\n') {
        /* Process the command */
        command_buffer[command_length] = '\0';
        terminal_writestring("\n");
        
        shell_process_command(command_buffer);
        
        /* Reset for next command */
        command_length = 0;
        cursor_position = 0;
        shell_print_prompt();
        
    } else if (c == '\b') {
        /* Handle backspace - delete character to the left of cursor */
        if (cursor_position > 0) {
            /* Shift characters left to fill the gap */
            for (size_t i = cursor_position - 1; i < command_length - 1; i++) {
                command_buffer[i] = command_buffer[i + 1];
            }
            command_length--;
            cursor_position--;
            
            /* Redraw the line */
            shell_redraw_line();
        }
        
    } else if (c == KEY_ARROW_LEFT) {
        /* Move cursor left */
        if (cursor_position > 0) {
            cursor_position--;
            terminal_move_cursor_left();
        }
        
    } else if (c == KEY_ARROW_RIGHT) {
        /* Move cursor right */
        if (cursor_position < command_length) {
            cursor_position++;
            terminal_move_cursor_right();
        }
        
    } else if (c == KEY_ARROW_UP || c == KEY_ARROW_DOWN) {
        /* Up/Down arrows - handle terminal scrolling */
        if (c == KEY_ARROW_UP) {
            terminal_scroll_up();
        } else if (c == KEY_ARROW_DOWN) {
            terminal_scroll_down();
        }
        
    } else if (c >= 32 && c <= 126) {
        /* Handle printable characters - insert at cursor position */
        if (command_length < SHELL_MAX_COMMAND_LENGTH - 1) {
            /* Shift characters right to make space */
            for (size_t i = command_length; i > cursor_position; i--) {
                command_buffer[i] = command_buffer[i - 1];
            }
            
            /* Insert the new character */
            command_buffer[cursor_position] = c;
            command_length++;
            cursor_position++;
            
            /* Redraw the line */
            shell_redraw_line();
        }
    }
}
