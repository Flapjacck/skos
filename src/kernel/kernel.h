#ifndef KERNEL_H
#define KERNEL_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Include GDT definitions */
#include "gdt.h"

/* Include IDT definitions */
#include "idt.h"

/* Include PIC definitions */
#include "pic.h"

/* Include keyboard driver definitions */
#include "../drivers/keyboard.h"

/* Forward declarations for multiboot */
typedef struct multiboot_info multiboot_info_t;

/*------------------------------------------------------------------------------
 * VGA Text Mode Color Constants
 *------------------------------------------------------------------------------
 * The VGA text mode supports 16 colors for both foreground and background.
 * Each character cell in VGA text mode consists of:
 * - An ASCII character (8 bits)
 * - A color attribute (8 bits):
 *   - Lower 4 bits: Foreground color
 *   - Upper 4 bits: Background color
 *------------------------------------------------------------------------------
 */
enum vga_color {
    VGA_COLOR_BLACK = 0,         /* Used for background and empty space */
    VGA_COLOR_BLUE = 1,          /* Standard blue */
    VGA_COLOR_GREEN = 2,         /* Standard green */
    VGA_COLOR_CYAN = 3,          /* Combination of blue and green */
    VGA_COLOR_RED = 4,           /* Standard red */
    VGA_COLOR_MAGENTA = 5,       /* Combination of red and blue */
    VGA_COLOR_BROWN = 6,         /* Dark yellow/brown */
    VGA_COLOR_LIGHT_GREY = 7,    /* Default text color */
    VGA_COLOR_DARK_GREY = 8,     /* Bright black */
    VGA_COLOR_LIGHT_BLUE = 9,    /* Bright blue */
    VGA_COLOR_LIGHT_GREEN = 10,  /* Bright green */
    VGA_COLOR_LIGHT_CYAN = 11,   /* Bright cyan */
    VGA_COLOR_LIGHT_RED = 12,    /* Bright red */
    VGA_COLOR_LIGHT_MAGENTA = 13,/* Bright magenta */
    VGA_COLOR_LIGHT_BROWN = 14,  /* Bright yellow */
    VGA_COLOR_WHITE = 15         /* Bright white */
};

/*------------------------------------------------------------------------------
 * VGA Text Mode Constants
 *------------------------------------------------------------------------------
 * The VGA text mode operates in a memory-mapped fashion, where writing to
 * specific memory addresses directly affects the display. The standard VGA
 * text mode provides an 80x25 character grid.
 *------------------------------------------------------------------------------
 */
#define VGA_WIDTH 80   /* Standard VGA text mode width in characters */
#define VGA_HEIGHT 25  /* Standard VGA text mode height in characters */

/*------------------------------------------------------------------------------
 * Terminal Color Management Functions
 *------------------------------------------------------------------------------
 */

/**
 * @brief Combines foreground and background colors into a single color attribute
 * 
 * @param fg Foreground color from vga_color enum
 * @param bg Background color from vga_color enum
 * @return uint8_t Combined color attribute where:
 *         - Bits 0-3: Foreground color
 *         - Bits 4-7: Background color
 */
uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg);

/**
 * @brief Creates a VGA entry combining a character and its color attribute
 * 
 * @param uc ASCII character to display
 * @param color Combined color attribute from vga_entry_color
 * @return uint16_t 16-bit VGA entry where:
 *         - Bits 0-7:   ASCII character
 *         - Bits 8-15:  Color attribute
 */
uint16_t vga_entry(unsigned char uc, uint8_t color);

/*------------------------------------------------------------------------------
 * Terminal Management Functions
 *------------------------------------------------------------------------------
 */

/**
 * @brief Initializes the terminal interface
 * 
 * This function:
 * 1. Sets up the terminal state (cursor position, default colors)
 * 2. Maps the VGA buffer to memory address 0xB8000 (standard VGA text buffer)
 * 3. Clears the screen by filling it with blank spaces
 */
void terminal_initialize(void);

/**
 * @brief Changes the current terminal color scheme
 * 
 * @param color Combined color attribute created by vga_entry_color()
 */
void terminal_setcolor(uint8_t color);

/**
 * @brief Places a character with specified color at given coordinates
 * 
 * @param c Character to display
 * @param color Color attribute for the character
 * @param x X-coordinate in the terminal (0 to VGA_WIDTH-1)
 * @param y Y-coordinate in the terminal (0 to VGA_HEIGHT-1)
 */
void terminal_putentryat(char c, uint8_t color, size_t x, size_t y);

/**
 * @brief Handles newline operation in the terminal
 * 
 * This function:
 * 1. Resets the column position to 0
 * 2. Increments the row position
 * 3. Scrolls the screen up when reaching bottom of screen
 */
void terminal_newline(void);

/**
 * @brief Scrolls the terminal screen up by one line
 * 
 * This function:
 * 1. Moves all lines up by one position
 * 2. Clears the bottom line
 * 3. Is called when the terminal reaches the bottom of the screen
 */
void terminal_scroll(void);

/**
 * @brief Outputs a single character to the terminal
 * 
 * This function:
 * 1. Handles special characters (like newline)
 * 2. Places regular characters at current cursor position
 * 3. Advances cursor position
 * 4. Handles line wrapping when reaching end of line
 * 
 * @param c Character to output
 */
void terminal_putchar(char c);

/**
 * @brief Outputs a string to the terminal
 * 
 * @param data Null-terminated string to output
 */
void terminal_writestring(const char* data);

/**
 * @brief Show the cursor at the current terminal position
 */
void terminal_show_cursor(void);

/**
 * @brief Hide the cursor
 */
void terminal_hide_cursor(void);

/**
 * @brief Update cursor position to match terminal position
 */
void terminal_update_cursor(void);

/**
 * @brief Clear the current line from cursor position to end
 */
void terminal_clear_line_from_cursor(void);

/**
 * @brief Handle backspace operation
 */
void terminal_backspace(void);

/**
 * @brief Initialize keyboard input mode
 * 
 * This function enables keyboard input and displays a prompt with cursor
 */
void terminal_start_input(void);

#endif /* KERNEL_H */