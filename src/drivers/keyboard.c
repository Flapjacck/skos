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
    /* Wait until input buffer is empty */
    while (inb(PS2_STATUS_PORT) & PS2_STATUS_INPUT_FULL);
}

void keyboard_wait_output(void) {
    /* Wait until output buffer is full */
    while (!(inb(PS2_STATUS_PORT) & PS2_STATUS_OUTPUT_FULL));
}

bool keyboard_send_command(uint8_t command) {
    keyboard_wait_input();
    outb(PS2_DATA_PORT, command);
    
    /* Wait for response */
    keyboard_wait_output();
    uint8_t response = inb(PS2_DATA_PORT);
    
    return response == KB_RESPONSE_ACK;
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
    
    /* Initialize input buffer */
    input_buffer.read_pos = 0;
    input_buffer.write_pos = 0;
    input_buffer.count = 0;
    
    /* Test PS/2 controller */
    keyboard_wait_input();
    outb(PS2_COMMAND_PORT, PS2_CMD_TEST_CONTROLLER);
    keyboard_wait_output();
    uint8_t result = inb(PS2_DATA_PORT);
    if (result != 0x55) {
        /* Controller test failed */
        return;
    }
    
    /* Enable first PS/2 port */
    keyboard_wait_input();
    outb(PS2_COMMAND_PORT, PS2_CMD_ENABLE_FIRST);
    
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
    
    /* Reset keyboard */
    if (!keyboard_send_command(KB_CMD_RESET)) {
        return;
    }
    
    /* Wait for BAT (Basic Assurance Test) completion */
    keyboard_wait_output();
    uint8_t bat_result = inb(PS2_DATA_PORT);
    if (bat_result != 0xAA) {
        /* BAT failed */
        return;
    }
    
    /* Set scancode set 1 */
    if (keyboard_send_command(KB_CMD_SET_SCANCODE_SET)) {
        keyboard_wait_input();
        outb(PS2_DATA_PORT, 1);  /* Use scancode set 1 */
        keyboard_wait_output();
        uint8_t ack = inb(PS2_DATA_PORT);
        if (ack != 0xFA) {
            /* Command failed, but continue anyway */
        }
    }
    
    /* Enable scanning */
    keyboard_send_command(KB_CMD_ENABLE_SCANNING);
    
    /* Unmask keyboard IRQ */
    pic_unmask_irq(IRQ_KEYBOARD);
}

void keyboard_interrupt_handler(void) {
    /* Read scancode from keyboard */
    uint8_t scancode = inb(PS2_DATA_PORT);
    
    /* Handle extended scancodes (0xE0 prefix) */
    if (scancode == SCANCODE_EXTENDED) {
        keyboard_state.extended_scancode = true;
        return;
    }
    
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