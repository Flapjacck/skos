#ifndef SHELL_H
#define SHELL_H

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

/* Maximum command line length */
#define SHELL_MAX_COMMAND_LENGTH 256

/* Shell initialization and main functions */
void shell_init(void);
void shell_process_command(const char* command);
void shell_handle_input(int c);  /* Changed from char to int to handle special keys */

/* Built-in command functions */
void shell_cmd_help(void);
void shell_cmd_clear(void);
void shell_cmd_mem(void);
void shell_cmd_uptime(void);
void shell_cmd_timer(void);
void shell_cmd_sleep(void);
void shell_cmd_cpuid(void);
void shell_cmd_regs(void);
void shell_cmd_irq(void);
void shell_cmd_debug(void);
void shell_cmd_echo(void);
void shell_cmd_reboot(void);
void shell_cmd_scancode(void);

/* Utility functions */
void shell_print_prompt(void);
bool shell_strcmp(const char* str1, const char* str2);
size_t shell_strlen(const char* str);

#endif /* SHELL_H */
