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
void shell_cmd_help(const char* args);
void shell_cmd_clear(const char* args);
void shell_cmd_mem(const char* args);
void shell_cmd_uptime(const char* args);
void shell_cmd_timer(const char* args);
void shell_cmd_sleep(const char* args);
void shell_cmd_cpuid(const char* args);
void shell_cmd_regs(const char* args);
void shell_cmd_irq(const char* args);
void shell_cmd_debug(const char* args);
void shell_cmd_echo(const char* args);
void shell_cmd_reboot(const char* args);
void shell_cmd_scancode(const char* args);
void shell_cmd_ls(const char* args);
void shell_cmd_cat(const char* args);
void shell_cmd_fsinfo(const char* args);

/* Utility functions */
void shell_print_prompt(void);
bool shell_strcmp(const char* str1, const char* str2);  /* Case-insensitive comparison */
size_t shell_strlen(const char* str);

#endif /* SHELL_H */
