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
#include "timer.h"

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

/* Command table structure */
typedef struct {
    const char* name;
    void (*function)(void);
    const char* description;
} shell_command_t;

/* Built-in commands table */
static const shell_command_t commands[] = {
    {"help", shell_cmd_help, "Show available commands"},
    {"clear", shell_cmd_clear, "Clear the screen"},
    {"mem", shell_cmd_mem, "Show memory information"},
    {"uptime", shell_cmd_uptime, "Show system uptime"},
    {"timer", shell_cmd_timer, "Show timer information"},
    {"sleep", shell_cmd_sleep, "Sleep for 3 seconds (demo)"}
};

#define NUM_COMMANDS (sizeof(commands) / sizeof(commands[0]))

/*------------------------------------------------------------------------------
 * Utility Functions
 *------------------------------------------------------------------------------
 */

/* Simple string comparison function */
bool shell_strcmp(const char* str1, const char* str2) {
    if (str1 == NULL || str2 == NULL) {
        return false;
    }
    
    while (*str1 && *str2) {
        if (*str1 != *str2) {
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

/* Print shell prompt */
void shell_print_prompt(void) {
    terminal_writestring("skos~$ ");
    /* Update the prompt start column for backspace handling */
    prompt_start_column = terminal_column;
    terminal_update_cursor();
}

/*------------------------------------------------------------------------------
 * Built-in Commands
 *------------------------------------------------------------------------------
 */

/* Help command - shows all available commands */
void shell_cmd_help(void) {
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
void shell_cmd_clear(void) {
    terminal_initialize();  /* Re-initialize terminal to clear screen */
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
}

/* Memory command - shows memory information */
void shell_cmd_mem(void) {
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("\n=== MEMORY INFORMATION ===\n\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    
    /* Call the memory management function to print detailed stats */
    memory_print_stats();
    terminal_writestring("\n");
}

/* Uptime command - shows system uptime */
void shell_cmd_uptime(void) {
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
void shell_cmd_timer(void) {
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
void shell_cmd_sleep(void) {
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

/*------------------------------------------------------------------------------
 * Shell Main Functions
 *------------------------------------------------------------------------------
 */

/* Initialize the shell */
void shell_init(void) {
    command_length = 0;
    command_buffer[0] = '\0';
    
    /* Shell is now ready - no need for verbose messages during boot */
}

/* Process a complete command */
void shell_process_command(const char* command) {
    /* Skip empty commands */
    if (command == NULL || shell_strlen(command) == 0) {
        return;
    }
    
    /* Look for matching command */
    bool command_found = false;
    for (size_t i = 0; i < NUM_COMMANDS; i++) {
        if (shell_strcmp(command, commands[i].name)) {
            commands[i].function();
            command_found = true;
            break;
        }
    }
    
    /* If command not found, show error */
    if (!command_found) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("Unknown command: '");
        terminal_writestring(command);
        terminal_writestring("'\n");
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        terminal_writestring("Type 'help' for available commands\n");
    }
}

/* Handle individual character input */
void shell_handle_input(char c) {
    if (c == '\n') {
        /* Process the command */
        command_buffer[command_length] = '\0';
        terminal_writestring("\n");
        
        shell_process_command(command_buffer);
        
        /* Reset for next command */
        command_length = 0;
        shell_print_prompt();
        
    } else if (c == '\b') {
        /* Handle backspace */
        if (command_length > 0) {
            command_length--;
            terminal_backspace();
        }
        
    } else if (c >= 32 && c <= 126) {
        /* Handle printable characters */
        if (command_length < SHELL_MAX_COMMAND_LENGTH - 1) {
            command_buffer[command_length++] = c;
            terminal_putchar(c);
            terminal_update_cursor();
        }
    }
}
