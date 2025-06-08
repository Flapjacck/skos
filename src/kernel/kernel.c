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

/* Global variables for terminal state */
size_t terminal_row;
size_t terminal_column;
uint8_t terminal_color;
uint16_t* terminal_buffer;

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

/* Handle newline in terminal */
void terminal_newline(void) {
    terminal_column = 0;
    if (++terminal_row == VGA_HEIGHT)
        terminal_row = 0;
}

/* Put a single character */
void terminal_putchar(char c) {
    if (c == '\n') {
        terminal_newline();
        return;
    }

    terminal_putentryat(c, terminal_color, terminal_column, terminal_row);
    if (++terminal_column == VGA_WIDTH) {
        terminal_column = 0;
        if (++terminal_row == VGA_HEIGHT)
            terminal_row = 0;
    }
}

/* Write a string to the terminal */
void terminal_writestring(const char* data) {
    for (size_t i = 0; data[i] != '\0'; i++)
        terminal_putchar(data[i]);
}

/* Kernel main function */
void kernel_main(void) {
    /* Initialize terminal interface first for debug output */
    terminal_initialize();

    /* Display startup message */
    terminal_writestring("SKOS Kernel Starting...\n");
      /* Initialize Global Descriptor Table (GDT) */
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
    
    /* Reset color and display welcome message */
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    terminal_writestring("Welcome to SKOS!\n");
    terminal_writestring("A bare bones operating system\n");
    terminal_setcolor(vga_entry_color(VGA_COLOR_GREEN, VGA_COLOR_BLACK));
    terminal_writestring("System initialized successfully.\n");
    
    /* Infinite loop to prevent the CPU from executing beyond our code */
    while(1);    /* The CPU will continue executing this loop forever
                 * This is necessary because we don't have an operating
                 * system to return to */
}