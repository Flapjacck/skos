/*------------------------------------------------------------------------------
 * Simple Kernel
 *------------------------------------------------------------------------------
 * This is the entry point of our kernel after the bootloader transfers control.
 * The kernel is loaded at memory address 0x1000 by the bootloader.
 *------------------------------------------------------------------------------
 */

/* Include standard types */
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "kernel.h"
#include "gdt.h"
#include "idt.h"
#include "pic.h"
#include "memory.h"
#include "debug.h"
#include "fat32.h"
#include "../drivers/timer.h"
#include "../drivers/ata.h"

/* Global variables for terminal state */
size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
uint16_t* terminal_buffer;
size_t prompt_start_column;  /* Track where the prompt starts to prevent deletion */

/* Scrollback buffer variables */
static uint16_t scrollback_buffer[SCROLLBACK_LINES * VGA_WIDTH];  /* Scrollback buffer */
static size_t scrollback_head = 0;       /* Current write position in circular buffer */
static size_t scrollback_lines_used = 0; /* Number of lines actually stored */
static int scroll_offset = 0;            /* Current scroll position (0 = bottom/current) */
static uint16_t saved_terminal_buffer[VGA_HEIGHT * VGA_WIDTH]; /* Save current content when scrolling */

/* Implementation of kernel functions */

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
    terminal_color = vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK);
    terminal_buffer = (uint16_t*) 0xB8000;
    
    /* Clear the terminal */
    for (size_t y = 0; y < VGA_HEIGHT; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t index = y * VGA_WIDTH + x;
            terminal_buffer[index] = vga_entry(' ', terminal_color);
        }
    }
}

/* Set the terminal color */
void terminal_setcolor(uint8_t color) {
    terminal_color = color;
}

/* Put a character at a specific position */
void terminal_putentryat(char c, uint8_t color, size_t x, size_t y) {
    const size_t index = y * VGA_WIDTH + x;
    terminal_buffer[index] = vga_entry(c, color);
}

/* Scroll the terminal up by one line */
void terminal_scroll(void) {
    /* If we're scrolled up, automatically scroll back to bottom on new content */
    if (scroll_offset > 0) {
        terminal_reset_scroll();
    }
    
    /* Save the top line to scrollback buffer before scrolling */
    size_t scrollback_line_index = scrollback_head * VGA_WIDTH;
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        scrollback_buffer[scrollback_line_index + x] = terminal_buffer[x];
    }
    
    /* Update scrollback buffer head pointer (circular buffer) */
    scrollback_head = (scrollback_head + 1) % SCROLLBACK_LINES;
    if (scrollback_lines_used < SCROLLBACK_LINES) {
        scrollback_lines_used++;
    }
    
    /* Move all lines up by one */
    for (size_t y = 0; y < VGA_HEIGHT - 1; y++) {
        for (size_t x = 0; x < VGA_WIDTH; x++) {
            const size_t dst_index = y * VGA_WIDTH + x;
            const size_t src_index = (y + 1) * VGA_WIDTH + x;
            terminal_buffer[dst_index] = terminal_buffer[src_index];
        }
    }
    
    /* Clear the bottom line */
    for (size_t x = 0; x < VGA_WIDTH; x++) {
        const size_t index = (VGA_HEIGHT - 1) * VGA_WIDTH + x;
        terminal_buffer[index] = vga_entry(' ', terminal_color);
    }
}

/* Handle newline in terminal */
void terminal_newline(void) {
    /* If we're scrolled up, automatically scroll back to bottom on new content */
    if (scroll_offset > 0) {
        terminal_reset_scroll();
    }
    
    terminal_column = 0;
    if (++terminal_row == VGA_HEIGHT) {
        terminal_row = VGA_HEIGHT - 1;  /* Stay on the last line */
        terminal_scroll();              /* Scroll the screen up */
    }
}

/* Put a single character */
void terminal_putchar(char c) {
    /* If we're scrolled up, automatically scroll back to bottom on new content */
    if (scroll_offset > 0) {
        terminal_reset_scroll();
    }
    
    if (c == '\n') {
        terminal_newline();
        return;
    }

    terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
    if (++terminal_column == VGA_WIDTH) {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT) {
            terminal_row = VGA_HEIGHT - 1;  /* Stay on the last line */
            terminal_scroll();              /* Scroll the screen up */
        }
    }
}

/* Write a string to the terminal */
void terminal_writestring(const char* data) {
    for (size_t i = 0; data[i] != '\0'; i++)
        terminal_putchar(data[i]);
}

/* I/O port functions for cursor control */
static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* Show the cursor at the current terminal position */
void terminal_show_cursor(void) {
    /* Enable cursor */
    outb(0x3D4, 0x0A);  /* Cursor Start Register */
    outb(0x3D5, (inb(0x3D5) & 0xC0) | 0);  /* Cursor start line 0 */
    
    outb(0x3D4, 0x0B);  /* Cursor End Register */
    outb(0x3D5, (inb(0x3D5) & 0xE0) | 15); /* Cursor end line 15 */
}

/* Hide the cursor */
void terminal_hide_cursor(void) {
    outb(0x3D4, 0x0A);  /* Cursor Start Register */
    outb(0x3D5, 0x20);  /* Disable cursor */
}

/* Update cursor position to match terminal position */
void terminal_update_cursor(void) {
    uint16_t pos = terminal_row * VGA_WIDTH + terminal_column;
    
    /* Send low byte of cursor position */
    outb(0x3D4, 0x0F);  /* Cursor Location Low Register */
    outb(0x3D5, (uint8_t)(pos & 0xFF));
    
    /* Send high byte of cursor position */
    outb(0x3D4, 0x0E);  /* Cursor Location High Register */
    outb(0x3D5, (uint8_t)((pos >> 8) & 0xFF));
}

/* Clear the current line from cursor position to end */
void terminal_clear_line_from_cursor(void) {
    for (size_t x = terminal_column; x < VGA_WIDTH; x++) {
        terminal_putentryat(' ', terminal_color, x, terminal_row);
    }
}

/* Handle backspace operation */
void terminal_backspace(void) {
    /* Only allow backspace if we're not at the start of the prompt */
    if (terminal_column > prompt_start_column) {
        terminal_column--;
        terminal_putentryat(' ', terminal_color, terminal_column, terminal_row);
        terminal_update_cursor();
    }
}

/* Initialize keyboard input mode */
void terminal_start_input(void) {
    terminal_writestring("\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    terminal_writestring("Type 'help' for available commands or start typing a command.\n\n");
    terminal_writestring("skos~$ ");  /* Show prompt */
    prompt_start_column = terminal_column;  /* Remember where user input starts */
    terminal_show_cursor();
    terminal_update_cursor();
}

/* Move cursor left by one position within input bounds */
void terminal_move_cursor_left(void) {
    if (terminal_column > prompt_start_column) {
        terminal_column--;
        terminal_update_cursor();
    }
}

/* Move cursor right by one position within input bounds */
void terminal_move_cursor_right(void) {
    /* Only allow moving right if there's content to move through */
    /* This will be properly bounded by the shell based on command length */
    if (terminal_column < VGA_WIDTH - 1) {
        terminal_column++;
        terminal_update_cursor();
    }
}

/* Move cursor to beginning of input line */
void terminal_move_cursor_home(void) {
    terminal_column = prompt_start_column;
    terminal_update_cursor();
}

/* Move cursor to end of input line */
void terminal_move_cursor_end(void) {
    /* This will be called by shell with proper end position */
    terminal_update_cursor();
}

/* Kernel main function */
void kernel_main(uint32_t magic, multiboot_info_t* mboot_info) {
    /* Initialize terminal interface first for debug output */
    terminal_initialize();
    
    /* Initialize debugging subsystem early */
    debug_init();

    /* Check multiboot magic number */
    if (magic != 0x2BADB002) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_RED, VGA_COLOR_BLACK));
        terminal_writestring("ERROR: Invalid multiboot magic number!\n");
        while(1) asm volatile("hlt");
    }

    /* Display SKOS ASCII art banner */
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("\n");
    terminal_writestring("  ____  _  _   ____  ____ \n");
    terminal_writestring(" / ___|| |/ / / __ \\/ ___|\n");
    terminal_writestring(" \\___ \\| ' / | |  | \\___ \\\n");
    terminal_writestring("  ___) | . \\ | |__| |___) |\n");
    terminal_writestring(" |____/|_|\\_\\ \\____/|____/\n");
    terminal_writestring("\n");
    
    /* Boot sequence header */
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("=== SYSTEM INITIALIZATION ===\n\n");
    
    /* Initialize Global Descriptor Table (GDT) */
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    terminal_writestring("Initializing GDT... ");
    gdt_init();
    terminal_setcolor(vga_entry_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("OK\n");
    
    /* Initialize Interrupt Descriptor Table (IDT) */
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    terminal_writestring("Initializing IDT... ");
    idt_init();
    terminal_setcolor(vga_entry_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("OK\n");
    
    /* Initialize Programmable Interrupt Controller (PIC) */
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    terminal_writestring("Initializing PIC... ");
    pic_init();
    terminal_setcolor(vga_entry_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("OK\n");
    
    /* Initialize Timer */
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    terminal_writestring("Initializing Timer... ");
    timer_init();
    pic_unmask_irq(0);  /* Enable timer interrupts (IRQ 0) */
    terminal_setcolor(vga_entry_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("OK\n");
    
    /* Initialize Memory Management */
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    terminal_writestring("Initializing Memory... ");
    memory_init(mboot_info);
    terminal_setcolor(vga_entry_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("OK\n");
    
    /* Initialize Keyboard Driver */
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    terminal_writestring("Initializing Keyboard... ");
    keyboard_init();
    terminal_setcolor(vga_entry_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("OK\n");
    
    /* Initialize Shell Driver */
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    terminal_writestring("Initializing Shell... ");
    shell_init();
    terminal_setcolor(vga_entry_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("OK\n");
    
    /* Initialize ATA Driver */
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    terminal_writestring("Initializing ATA/IDE... ");
    bool ata_success = ata_init();
    if (ata_success) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK));
        terminal_writestring("OK\n");
    } else {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK));
        terminal_writestring("NO DRIVES\n");
    }
    
    /* Initialize FAT32 File System */
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    terminal_writestring("Initializing FAT32... ");
    bool fat32_success = fat32_init();
    if (fat32_success) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK));
        terminal_writestring("OK\n");
    } else {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK));
        terminal_writestring("NO FS\n");
    }
    
    /* Enable interrupts */
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    terminal_writestring("Enabling interrupts... ");
    asm volatile ("sti");  /* Enable interrupts */
    terminal_setcolor(vga_entry_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("OK\n");
    
    /* Boot complete message */
    terminal_writestring("\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK));
    terminal_writestring("=== SYSTEM READY ===\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_WHITE, VGA_COLOR_BLACK));
    terminal_writestring("Welcome to SKOS!\n");
    
    /* Start keyboard input mode */
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    terminal_start_input();
    
    /* Main system loop - handle keyboard input */
    while(1) {
        /* Process keyboard input if available */
        if (keyboard_has_data()) {
            int c = keyboard_getchar();
            if (c != 0) {
                /* Let the shell handle all input processing */
                shell_handle_input(c);
            }
        }
        
        /* Halt CPU until next interrupt */
        asm volatile ("hlt");
    }
}

/*------------------------------------------------------------------------------
 * Terminal Scrollback Functions
 *------------------------------------------------------------------------------
 */

/* Helper function to save current terminal content */
static void terminal_save_current_content(void) {
    for (size_t i = 0; i < VGA_HEIGHT * VGA_WIDTH; i++) {
        saved_terminal_buffer[i] = terminal_buffer[i];
    }
}

/* Helper function to restore saved terminal content */
static void terminal_restore_current_content(void) {
    for (size_t i = 0; i < VGA_HEIGHT * VGA_WIDTH; i++) {
        terminal_buffer[i] = saved_terminal_buffer[i];
    }
}

/* Helper function to redraw the terminal from scrollback buffer */
static void terminal_redraw_from_scrollback(void) {
    if (scroll_offset == 0) {
        /* Not scrolled, restore current content */
        terminal_restore_current_content();
        return;
    }
    
    /* Ensure scroll offset doesn't exceed available history */
    if ((size_t)scroll_offset > scrollback_lines_used) {
        scroll_offset = (int)scrollback_lines_used;
    }
    
    /* Fill the screen with the appropriate content */
    for (size_t display_line = 0; display_line < VGA_HEIGHT; display_line++) {
        /* Calculate which line to show (counting back from current) */
        int lines_back = scroll_offset - (int)display_line;
        
        if (lines_back > 0 && lines_back <= (int)scrollback_lines_used) {
            /* Show line from scrollback buffer */
            size_t scrollback_index = (scrollback_head + SCROLLBACK_LINES - lines_back) % SCROLLBACK_LINES;
            
            for (size_t x = 0; x < VGA_WIDTH; x++) {
                size_t src_index = scrollback_index * VGA_WIDTH + x;
                size_t dst_index = display_line * VGA_WIDTH + x;
                terminal_buffer[dst_index] = scrollback_buffer[src_index];
            }
        } else if (lines_back <= 0) {
            /* Show line from current/saved content */
            size_t saved_line = display_line - scroll_offset;
            if (saved_line < VGA_HEIGHT) {
                for (size_t x = 0; x < VGA_WIDTH; x++) {
                    size_t src_index = saved_line * VGA_WIDTH + x;
                    size_t dst_index = display_line * VGA_WIDTH + x;
                    terminal_buffer[dst_index] = saved_terminal_buffer[src_index];
                }
            } else {
                /* Clear line if beyond saved content */
                for (size_t x = 0; x < VGA_WIDTH; x++) {
                    size_t dst_index = display_line * VGA_WIDTH + x;
                    terminal_buffer[dst_index] = vga_entry(' ', terminal_color);
                }
            }
        } else {
            /* Clear line if beyond available history */
            for (size_t x = 0; x < VGA_WIDTH; x++) {
                size_t dst_index = display_line * VGA_WIDTH + x;
                terminal_buffer[dst_index] = vga_entry(' ', terminal_color);
            }
        }
    }
}

/* Scroll the terminal view up by one line */
void terminal_scroll_up(void) {
    if (scrollback_lines_used == 0) {
        return; /* No scrollback history available */
    }
    
    if (scroll_offset == 0) {
        /* First time scrolling up, save current content */
        terminal_save_current_content();
    }
    
    /* Limit scroll to available history */
    int max_scroll = (int)scrollback_lines_used;
    if (scroll_offset < max_scroll) {
        scroll_offset++;
        terminal_redraw_from_scrollback();
    }
}

/* Scroll the terminal view down by one line */
void terminal_scroll_down(void) {
    if (scroll_offset > 0) {
        scroll_offset--;
        terminal_redraw_from_scrollback();
    }
}

/* Check if the terminal is currently scrolled up (viewing history) */
bool terminal_is_scrolled(void) {
    return scroll_offset > 0;
}

/* Reset scroll position to show the current terminal content */
void terminal_reset_scroll(void) {
    if (scroll_offset > 0) {
        scroll_offset = 0;
        terminal_restore_current_content();
    }
}