/*
 * SKOS Kernel
 * Initial Kernel Implementation
 */

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

/* Make kernel_main visible to the linker */
#if defined(__cplusplus)
extern "C" /* Use C linkage for kernel_main */
#endif
void kernel_main(void);

/* 
 * values to change the kernel's behavior
 */
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define WELCOME_MSG "Welcome to SKOS - Simple Kernel OS"
#define OS_VERSION "v0.1"

/* VGA Colors */
enum vga_color {
    VGA_COLOR_BLACK = 0,
    VGA_COLOR_BLUE = 1,
    VGA_COLOR_GREEN = 2,
    VGA_COLOR_CYAN = 3,
    VGA_COLOR_RED = 4,
    VGA_COLOR_MAGENTA = 5,
    VGA_COLOR_BROWN = 6,
    VGA_COLOR_LIGHT_GREY = 7,
    VGA_COLOR_DARK_GREY = 8,
    VGA_COLOR_LIGHT_BLUE = 9,
    VGA_COLOR_LIGHT_GREEN = 10,
    VGA_COLOR_LIGHT_CYAN = 11,
    VGA_COLOR_LIGHT_RED = 12,
    VGA_COLOR_LIGHT_MAGENTA = 13,
    VGA_COLOR_LIGHT_BROWN = 14,
    VGA_COLOR_WHITE = 15,
};

/* Customizable display attributes */
static const uint8_t DEFAULT_COLOR = VGA_COLOR_LIGHT_BLUE;
static const uint8_t DEFAULT_BGCOLOR = VGA_COLOR_BLACK;
static uint16_t* const VGA_MEMORY = (uint16_t*) 0xB8000;

/* Terminal state */
static size_t terminal_row;
static size_t terminal_column;
static uint8_t terminal_color;
static uint16_t* terminal_buffer;

/* Basic VGA text mode functions */
uint8_t vga_entry_color(enum vga_color fg, enum vga_color bg) {
    return fg | bg << 4;
}

uint16_t vga_entry(unsigned char uc, uint8_t color) {
    return (uint16_t) uc | (uint16_t) color << 8;
}

/* Initialize the terminal */
void terminal_initialize(void) {
    terminal_row = 0;
    terminal_column = 0;
    terminal_color = vga_entry_color(DEFAULT_COLOR, DEFAULT_BGCOLOR);
    terminal_buffer = VGA_MEMORY;
    
    /* Clear the screen */
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            terminal_buffer[index] = vga_entry(' ', terminal_color);
        }
    }
}

/* Basic output functions */
void terminal_putchar(char c) {
    if (c == '\n') {
        terminal_column = 0;
        terminal_row++;
        return;
    }

    const size_t index = terminal_row * VGA_WIDTH + terminal_column;
    terminal_buffer[index] = vga_entry(c, terminal_color);
    
    if (++terminal_column == VGA_WIDTH) {
        terminal_column = 0;
        terminal_row++;
    }
}

void terminal_write(const char* data, size_t size) {
    for (size_t i = 0; i < size; i++)
        terminal_putchar(data[i]);
}

void terminal_writestring(const char* data) {
    for (size_t i = 0; data[i] != '\0'; i++)
        terminal_putchar(data[i]);
}

/* Kernel entry point */
void kernel_main(void) {
    /* Initialize terminal interface */
    terminal_initialize();

    /* Display welcome message */
    terminal_writestring(WELCOME_MSG);
    terminal_putchar('\n');
    terminal_writestring("Version: ");
    terminal_writestring(OS_VERSION);
    terminal_putchar('\n');
    
    /* Halt the CPU */
    while(1) {
        asm("hlt");
    }
}