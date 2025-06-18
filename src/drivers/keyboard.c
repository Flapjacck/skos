/*------------------------------------------------------------------------------
 * PS/2 Keyboard Driver Implementation
 *------------------------------------------------------------------------------
 * This file implements a basic PS/2 keyboard driver for the SKOS kernel.
 * It handles keyboard initialization, interrupt processing, and scancode
 * to ASCII conversion.
 * 
 * References:
 * - https://wiki.osdev.org/I8042_PS/2_Controller
 * - https://wiki.osdev.org/PS/2_Keyboard
 *------------------------------------------------------------------------------
 */

#include "keyboard.h"
#include "../kernel/kernel.h"
#include "../kernel/pic.h"
#include <stddef.h>

/*------------------------------------------------------------------------------
 * Global Variables
 *------------------------------------------------------------------------------
 */

/* Keyboard state */
static keyboard_state_t keyboard_state = {0};

/* Input buffer */
static input_buffer_t input_buffer = {0};

/*------------------------------------------------------------------------------
 * Forward Declarations for Debug Functions
 *------------------------------------------------------------------------------
 */
static const char* get_scancode_name(uint8_t scancode);
static void print_debug_hex8(uint8_t value);
static void display_scancode_debug(uint8_t raw_scancode);

/*------------------------------------------------------------------------------
 * Scancode to ASCII Translation Table (US QWERTY Layout)
 *------------------------------------------------------------------------------
 * This table maps PS/2 scancodes to ASCII characters for a US QWERTY keyboard.
 * Only covers basic printable characters for now.
 *------------------------------------------------------------------------------
 */

/* PS/2 Scancode Set 1 to ASCII mapping table (US QWERTY) */
static const char scancode_to_ascii_table[128] = {
/*00*/ 0,    0,   '1', '2', '3', '4', '5', '6', '7', '8', '9', '0', '-', '=', '\b',
/*0F*/ '\t', 'q', 'w', 'e', 'r', 't', 'y', 'u', 'i', 'o', 'p', '[', ']', '\n',
/*1D*/ 0,    'a', 's', 'd', 'f', 'g', 'h', 'j', 'k', 'l', ';', '\'', '`',
/*2A*/ 0,    '\\', 'z', 'x', 'c', 'v', 'b', 'n', 'm', ',', '.', '/', 0,
/*37*/ '*',  0,   ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   /* 0x37-0x46 */
/*47*/ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,         /* 0x47-0x56 */
/*57*/ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,         /* 0x57-0x66 */
/*67*/ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0          /* 0x67-0x76 */
};

/* PS/2 Scancode Set 1 to ASCII mapping table (US QWERTY - Shift) */
static const char scancode_to_ascii_shift_table[128] = {
/*00*/ 0,    0,   '!', '@', '#', '$', '%', '^', '&', '*', '(', ')', '_', '+', '\b',
/*0F*/ '\t', 'Q', 'W', 'E', 'R', 'T', 'Y', 'U', 'I', 'O', 'P', '{', '}', '\n',
/*1D*/ 0,    'A', 'S', 'D', 'F', 'G', 'H', 'J', 'K', 'L', ':', '"', '~',
/*2A*/ 0,    '|', 'Z', 'X', 'C', 'V', 'B', 'N', 'M', '<', '>', '?', 0,
/*37*/ '*',  0,   ' ', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,   /* 0x37-0x46 */
/*47*/ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,         /* 0x47-0x56 */
/*57*/ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,         /* 0x57-0x66 */
/*67*/ 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0          /* 0x67-0x76 */
};

/*------------------------------------------------------------------------------
 * I/O Port Helper Functions
 *------------------------------------------------------------------------------
 */

static inline void outb(uint16_t port, uint8_t val) {
    asm volatile ("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    asm volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/*------------------------------------------------------------------------------
 * PS/2 Controller Helper Functions
 *------------------------------------------------------------------------------
 */

void keyboard_wait_input(void) {
    /* Wait until input buffer is empty with timeout */
    int timeout = 100000;
    while ((inb(PS2_STATUS_PORT) & PS2_STATUS_INPUT_FULL) && --timeout > 0);
}

void keyboard_wait_output(void) {
    /* Wait until output buffer is full with timeout */
    int timeout = 100000;
    while (!(inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL) && --timeout > 0);
}

bool keyboard_send_command(uint8_t command) {
    keyboard_wait_input();
    outb(PS2_DATA_PORT, command);
    
    /* Wait for response */
    keyboard_wait_output();
    uint8_t response = inb(PS2_DATA_PORT);
    
    return response == KB_RESPONSE_ACK;
}

/**
 * @brief Drain any pending data from keyboard output buffer
 * This prevents hangs when data is waiting but interrupts are disabled
 */
static void keyboard_drain_output_buffer(void) {
    int timeout = 1000;
    while ((inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL) && --timeout > 0) {
        inb(PS2_DATA_PORT);  /* Read and discard data */
    }
}

/*------------------------------------------------------------------------------
 * Input Buffer Functions
 *------------------------------------------------------------------------------
 */

static void input_buffer_put(char c) {
    if (input_buffer.count < KEYBOARD_BUFFER_SIZE - 1) {
        input_buffer.buffer[input_buffer.write_pos] = c;
        input_buffer.write_pos = (input_buffer.write_pos + 1) % KEYBOARD_BUFFER_SIZE;
        input_buffer.count++;
    }
}

static char input_buffer_get(void) {
    if (input_buffer.count > 0) {
        char c = input_buffer.buffer[input_buffer.read_pos];
        input_buffer.read_pos = (input_buffer.read_pos + 1) % KEYBOARD_BUFFER_SIZE;
        input_buffer.count--;
        return c;
    }
    return 0;
}

/*------------------------------------------------------------------------------
 * Public Interface Functions
 *------------------------------------------------------------------------------
 */

void keyboard_init(void) {
    /* Initialize keyboard state */
    keyboard_state.shift_pressed = false;
    keyboard_state.ctrl_pressed = false;
    keyboard_state.alt_pressed = false;
    keyboard_state.caps_lock = false;
    keyboard_state.num_lock = false;
    keyboard_state.scroll_lock = false;
    keyboard_state.extended_scancode = false;
    keyboard_state.debug_mode = false;
    keyboard_state.debug_mode = false;
    
    /* Initialize input buffer */
    input_buffer.read_pos = 0;
    input_buffer.write_pos = 0;
    input_buffer.count = 0;
    
    /* Drain any existing data first */
    keyboard_drain_output_buffer();
    
    /* Test PS/2 controller */
    keyboard_wait_input();
    outb(PS2_COMMAND_PORT, PS2_CMD_TEST_CONTROLLER);
    keyboard_wait_output();
    uint8_t result = inb(PS2_DATA_PORT);
    if (result != 0x55) {
        /* Controller test failed, but continue anyway */
    }
    
    /* Drain any spurious data */
    keyboard_drain_output_buffer();
    
    /* Enable first PS/2 port */
    keyboard_wait_input();
    outb(PS2_COMMAND_PORT, PS2_CMD_ENABLE_FIRST);
    
    /* Drain any data generated by enabling the port */
    keyboard_drain_output_buffer();
    
    /* Read current configuration */
    keyboard_wait_input();
    outb(PS2_COMMAND_PORT, PS2_CMD_READ_CONFIG);
    keyboard_wait_output();
    uint8_t config = inb(PS2_DATA_PORT);
    
    /* Enable first port interrupt and disable translation */
    config |= PS2_CONFIG_FIRST_IRQ;
    config &= ~PS2_CONFIG_FIRST_TRANSLATE;
    
    /* Write configuration back */
    keyboard_wait_input();
    outb(PS2_COMMAND_PORT, PS2_CMD_WRITE_CONFIG);
    keyboard_wait_input();
    outb(PS2_DATA_PORT, config);
    
    /* Drain any configuration-related data */
    keyboard_drain_output_buffer();
    
    /* Reset keyboard - this is often the source of hangs */
    keyboard_wait_input();
    outb(PS2_DATA_PORT, KB_CMD_RESET);
    
    /* Wait for acknowledgment with timeout */
    keyboard_wait_output();
    uint8_t ack = inb(PS2_DATA_PORT);
    if (ack != KB_RESPONSE_ACK) {
        /* Reset failed, continue anyway */
    }
    
    /* Wait for BAT (Basic Assurance Test) completion with timeout */
    keyboard_wait_output();
    uint8_t bat_result = inb(PS2_DATA_PORT);
    if (bat_result != 0xAA) {
        /* BAT failed, continue anyway */
    }
    
    /* Drain any additional reset-related data */
    keyboard_drain_output_buffer();
    
    /* Set scancode set 1 */
    keyboard_wait_input();
    outb(PS2_DATA_PORT, KB_CMD_SET_SCANCODE_SET);
    keyboard_wait_output();
    ack = inb(PS2_DATA_PORT);
    if (ack == KB_RESPONSE_ACK) {
        keyboard_wait_input();
        outb(PS2_DATA_PORT, 1);  /* Use scancode set 1 */
        keyboard_wait_output();
        ack = inb(PS2_DATA_PORT);
        /* Don't care if this fails */
    }
    
    /* Drain any scancode set data */
    keyboard_drain_output_buffer();
    
    /* Enable scanning */
    keyboard_wait_input();
    outb(PS2_DATA_PORT, KB_CMD_ENABLE_SCANNING);
    keyboard_wait_output();
    ack = inb(PS2_DATA_PORT);
    /* Don't care if this fails */
    
    /* Final buffer drain before enabling interrupts */
    keyboard_drain_output_buffer();
    
    /* Unmask keyboard IRQ */
    pic_unmask_irq(IRQ_KEYBOARD);
}

void keyboard_interrupt_handler(void) {
    /* Read scancode from keyboard */
    uint8_t scancode = inb(PS2_DATA_PORT);
    
    /* Handle extended scancodes (0xE0 prefix) */
    if (scancode == SCANCODE_EXTENDED) {
        keyboard_state.extended_scancode = true;
        if (keyboard_state.debug_mode) {
            terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK));
            terminal_writestring("Extended scancode prefix: 0xE0\n");
            terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
        }
        return;
    }
    
    /* If in debug mode, display scancode info */
    if (keyboard_state.debug_mode) {
        display_scancode_debug(scancode);
        
        /* Check for 'q' key press to exit debug mode (scancode 0x10) */
        if (scancode == 0x10 && !(scancode & 0x80)) {  /* Q key press, not release */
            keyboard_state.debug_mode = false;
            terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_BROWN, VGA_COLOR_BLACK));
            terminal_writestring("\nExiting scancode debug mode...\n\n");
            terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
            /* Clear the extended scancode flag and return without processing further */
            keyboard_state.extended_scancode = false;
            return;
        }
        
        /* Reset extended scancode flag and return without normal processing */
        keyboard_state.extended_scancode = false;
        return;
    }
    
    /* Normal keyboard processing when not in debug mode */
    
    /* Check if this is a Set 1 key release (bit 7 set) */
    bool key_released = (scancode & 0x80) != 0;
    if (key_released) {
        scancode &= 0x7F;  /* Remove release bit */
        /* Handle modifier key releases */
        switch (scancode) {
            case 0x2A: /* Left Shift */
            case 0x36: /* Right Shift */
                keyboard_state.shift_pressed = false;
                break;
            case 0x1D: /* Ctrl */
                keyboard_state.ctrl_pressed = false;
                break;
            case 0x38: /* Alt */
                keyboard_state.alt_pressed = false;
                break;
        }
        keyboard_state.extended_scancode = false;
        return;  /* Don't process key releases for regular keys */
    }
    
    
    /* Handle modifier key presses */
    switch (scancode) {
        case 0x2A: /* Left Shift */
        case 0x36: /* Right Shift */
            keyboard_state.shift_pressed = true;
            keyboard_state.extended_scancode = false;
            return;
            
        case 0x1D: /* Ctrl */
            keyboard_state.ctrl_pressed = true;
            keyboard_state.extended_scancode = false;
            return;
            
        case 0x38: /* Alt */
            keyboard_state.alt_pressed = true;
            keyboard_state.extended_scancode = false;
            return;
            
        case 0x3A: /* Caps Lock */
            keyboard_state.caps_lock = !keyboard_state.caps_lock;
            keyboard_update_leds();
            keyboard_state.extended_scancode = false;
            return;
    }
    
    /* Reset extended scancode flag */
    keyboard_state.extended_scancode = false;
    
    /* Convert scancode to ASCII */
    char ascii = scancode_to_ascii(scancode);
    
    if (ascii != 0) {
        /* Just add character to buffer - display will be handled by main loop */
        input_buffer_put(ascii);
    }
}

char scancode_to_ascii(uint8_t scancode) {
    if (scancode >= 128) {
        return 0;
    }
    
    char ascii;
    
    /* Use shift table if shift is pressed or caps lock affects this key */
    bool use_shift_table = keyboard_state.shift_pressed;
    if (keyboard_state.caps_lock && scancode >= 0x10 && scancode <= 0x32) {
        /* Caps lock affects letter keys */
        char c = scancode_to_ascii_table[scancode];
        if (c >= 'a' && c <= 'z') {
            use_shift_table = !use_shift_table; /* Toggle shift effect for letters */
        }
    }
    
    if (use_shift_table) {
        ascii = scancode_to_ascii_shift_table[scancode];
    } else {
        ascii = scancode_to_ascii_table[scancode];
    }
    
    return ascii;
}

char keyboard_getchar(void) {
    return input_buffer_get();
}

bool keyboard_has_data(void) {
    return input_buffer.count > 0;
}

size_t keyboard_readline(char* buffer, size_t max_length) {
    size_t pos = 0;
    
    while (pos < max_length - 1) {
        /* Wait for input */
        while (!keyboard_has_data()) {
            asm volatile ("hlt");
        }
        
        char c = keyboard_getchar();
        if (c == '\n') {
            break;
        } else if (c == '\b') {
            /* Handle backspace */
            if (pos > 0) {
                pos--;
                terminal_backspace();  /* Handle display */
            }
        } else if (c >= 32 && c <= 126) {
            /* Printable character */
            buffer[pos++] = c;
            terminal_putchar(c);  /* Handle display */
            terminal_update_cursor();
        } else if (c == '\t') {
            /* Tab character */
            buffer[pos++] = c;
            terminal_putchar(c);  /* Handle display */
            terminal_update_cursor();
        }
    }
    
    buffer[pos] = '\0';
    return pos;
}

keyboard_state_t* keyboard_get_state(void) {
    return &keyboard_state;
}

void keyboard_update_leds(void) {
    uint8_t led_state = 0;
    
    if (keyboard_state.scroll_lock) led_state |= 0x01;
    if (keyboard_state.num_lock) led_state |= 0x02;
    if (keyboard_state.caps_lock) led_state |= 0x04;
    
    keyboard_send_command(KB_CMD_SET_LEDS);
    keyboard_send_command(led_state);
}

void keyboard_enable_debug_mode(void) {
    keyboard_state.debug_mode = true;
}

void keyboard_disable_debug_mode(void) {
    keyboard_state.debug_mode = false;
}

bool keyboard_is_debug_mode_active(void) {
    return keyboard_state.debug_mode;
}

/**
 * @brief Get the name of a scancode for debug display
 * 
 * @param scancode The scancode to look up
 * @return const char* Human-readable name of the key
 */
static const char* get_scancode_name(uint8_t scancode) {
    switch (scancode) {
        case 0x01: return "ESC";
        case 0x02: return "1";
        case 0x03: return "2";
        case 0x04: return "3";
        case 0x05: return "4";
        case 0x06: return "5";
        case 0x07: return "6";
        case 0x08: return "7";
        case 0x09: return "8";
        case 0x0A: return "9";
        case 0x0B: return "0";
        case 0x0C: return "-";
        case 0x0D: return "=";
        case 0x0E: return "BACKSPACE";
        case 0x0F: return "TAB";
        case 0x10: return "Q";
        case 0x11: return "W";
        case 0x12: return "E";
        case 0x13: return "R";
        case 0x14: return "T";
        case 0x15: return "Y";
        case 0x16: return "U";
        case 0x17: return "I";
        case 0x18: return "O";
        case 0x19: return "P";
        case 0x1A: return "[";
        case 0x1B: return "]";
        case 0x1C: return "ENTER";
        case 0x1D: return "LEFT_CTRL";
        case 0x1E: return "A";
        case 0x1F: return "S";
        case 0x20: return "D";
        case 0x21: return "F";
        case 0x22: return "G";
        case 0x23: return "H";
        case 0x24: return "J";
        case 0x25: return "K";
        case 0x26: return "L";
        case 0x27: return ";";
        case 0x28: return "'";
        case 0x29: return "`";
        case 0x2A: return "LEFT_SHIFT";
        case 0x2B: return "\\";
        case 0x2C: return "Z";
        case 0x2D: return "X";
        case 0x2E: return "C";
        case 0x2F: return "V";
        case 0x30: return "B";
        case 0x31: return "N";
        case 0x32: return "M";
        case 0x33: return ",";
        case 0x34: return ".";
        case 0x35: return "/";
        case 0x36: return "RIGHT_SHIFT";
        case 0x37: return "KEYPAD_*";
        case 0x38: return "LEFT_ALT";
        case 0x39: return "SPACE";
        case 0x3A: return "CAPS_LOCK";
        case 0x3B: return "F1";
        case 0x3C: return "F2";
        case 0x3D: return "F3";
        case 0x3E: return "F4";
        case 0x3F: return "F5";
        case 0x40: return "F6";
        case 0x41: return "F7";
        case 0x42: return "F8";
        case 0x43: return "F9";
        case 0x44: return "F10";
        case 0x45: return "NUM_LOCK";
        case 0x46: return "SCROLL_LOCK";
        case 0x47: return "KEYPAD_7";
        case 0x48: return "KEYPAD_8";
        case 0x49: return "KEYPAD_9";
        case 0x4A: return "KEYPAD_-";
        case 0x4B: return "KEYPAD_4";
        case 0x4C: return "KEYPAD_5";
        case 0x4D: return "KEYPAD_6";
        case 0x4E: return "KEYPAD_+";
        case 0x4F: return "KEYPAD_1";
        case 0x50: return "KEYPAD_2";
        case 0x51: return "KEYPAD_3";
        case 0x52: return "KEYPAD_0";
        case 0x53: return "KEYPAD_.";
        case 0x57: return "F11";
        case 0x58: return "F12";
        default: return "UNKNOWN";
    }
}

/**
 * @brief Print hex value for debug display
 */
static void print_debug_hex8(uint8_t value) {
    char hex_chars[] = "0123456789ABCDEF";
    terminal_putchar(hex_chars[(value >> 4) & 0xF]);
    terminal_putchar(hex_chars[value & 0xF]);
}

/**
 * @brief Display scancode debug information
 */
static void display_scancode_debug(uint8_t raw_scancode) {
    bool is_release = (raw_scancode & 0x80) != 0;
    uint8_t scancode = raw_scancode & 0x7F;
    const char* key_name = get_scancode_name(scancode);
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_CYAN, VGA_COLOR_BLACK));
    terminal_writestring("Scancode: 0x");
    print_debug_hex8(raw_scancode);
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    terminal_writestring(" | Base: 0x");
    print_debug_hex8(scancode);
    
    terminal_writestring(" | Key: ");
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREEN, VGA_COLOR_BLACK));
    terminal_writestring(key_name);
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    terminal_writestring(" | ");
    if (is_release) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_RED, VGA_COLOR_BLACK));
        terminal_writestring("RELEASE");
    } else {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_BLUE, VGA_COLOR_BLACK));
        terminal_writestring("PRESS");
    }
    
    if (keyboard_state.extended_scancode) {
        terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_MAGENTA, VGA_COLOR_BLACK));
        terminal_writestring(" [EXTENDED]");
    }
    
    terminal_setcolor(vga_entry_color(VGA_COLOR_LIGHT_GREY, VGA_COLOR_BLACK));
    terminal_writestring("\n");
}