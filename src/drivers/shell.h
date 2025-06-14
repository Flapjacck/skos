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
void shell_handle_input(char c);

/* Built-in command functions */
void shell_cmd_help(void);
void shell_cmd_clear(void);
void shell_cmd_mem(void);

/* Utility functions */
void shell_print_prompt(void);
bool shell_strcmp(const char* str1, const char* str2);
size_t shell_strlen(const char* str);

#endif /* SHELL_H */
