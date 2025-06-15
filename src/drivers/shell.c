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
    {"mem", shell_cmd_mem, "Show memory information"}
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
