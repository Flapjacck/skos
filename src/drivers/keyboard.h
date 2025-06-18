#ifndef KEYBOARD_H
#define KEYBOARD_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/*------------------------------------------------------------------------------
 * PS/2 Keyboard Driver Header
 *------------------------------------------------------------------------------
 * This header defines the interface for the PS/2 keyboard driver based on
 * the 8042 PS/2 Controller. The keyboard connects through the PS/2 controller
 * and generates IRQ1 (interrupt vector 33) when keys are pressed or released.
 * 
 * References:
 * - https://wiki.osdev.org/I8042_PS/2_Controller
 * - https://wiki.osdev.org/PS/2_Keyboard
 *------------------------------------------------------------------------------
 */

/*------------------------------------------------------------------------------
 * 8042 PS/2 Controller Ports
 *------------------------------------------------------------------------------
 */
#define PS2_DATA_PORT       0x60    /* PS/2 Data Port (read/write) */
#define PS2_STATUS_PORT     0x64    /* PS/2 Status Register (read) */
#define PS2_COMMAND_PORT    0x64    /* PS/2 Command Register (write) */

/*------------------------------------------------------------------------------
 * 8042 Status Register Bits
 *------------------------------------------------------------------------------
 */
#define PS2_STATUS_OUTPUT_FULL      0x01    /* Output buffer full (data available) */
#define PS2_STATUS_INPUT_FULL       0x02    /* Input buffer full (don't write) */
#define PS2_STATUS_SYSTEM_FLAG      0x04    /* System flag (passed by POST) */
#define PS2_STATUS_COMMAND_DATA     0x08    /* 0=data for PS/2 device, 1=command for controller */
#define PS2_STATUS_KEYBOARD_LOCK    0x10    /* Keyboard lock */
#define PS2_STATUS_AUX_OUTPUT_FULL  0x20    /* Auxiliary device output buffer full */
#define PS2_STATUS_TIMEOUT_ERROR    0x40    /* Timeout error */
#define PS2_STATUS_PARITY_ERROR     0x80    /* Parity error */

/*------------------------------------------------------------------------------
 * 8042 Controller Commands
 *------------------------------------------------------------------------------
 */
#define PS2_CMD_READ_CONFIG         0x20    /* Read controller configuration byte */
#define PS2_CMD_WRITE_CONFIG        0x60    /* Write controller configuration byte */
#define PS2_CMD_DISABLE_SECOND      0xA7    /* Disable second PS/2 port */
#define PS2_CMD_ENABLE_SECOND       0xA8    /* Enable second PS/2 port */
#define PS2_CMD_TEST_SECOND         0xA9    /* Test second PS/2 port */
#define PS2_CMD_TEST_CONTROLLER     0xAA    /* Test PS/2 controller */
#define PS2_CMD_TEST_FIRST          0xAB    /* Test first PS/2 port */
#define PS2_CMD_DISABLE_FIRST       0xAD    /* Disable first PS/2 port */
#define PS2_CMD_ENABLE_FIRST        0xAE    /* Enable first PS/2 port */

/*------------------------------------------------------------------------------
 * Controller Configuration Byte Bits
 *------------------------------------------------------------------------------
 */
#define PS2_CONFIG_FIRST_IRQ        0x01    /* First PS/2 port interrupt enabled */
#define PS2_CONFIG_SECOND_IRQ       0x02    /* Second PS/2 port interrupt enabled */
#define PS2_CONFIG_SYSTEM_FLAG      0x04    /* System flag */
#define PS2_CONFIG_FIRST_CLOCK      0x10    /* First PS/2 port clock disabled */
#define PS2_CONFIG_SECOND_CLOCK     0x20    /* Second PS/2 port clock disabled */
#define PS2_CONFIG_FIRST_TRANSLATE  0x40    /* First PS/2 port translation enabled */

/*------------------------------------------------------------------------------
 * Keyboard Commands
 *------------------------------------------------------------------------------
 */
#define KB_CMD_SET_LEDS             0xED    /* Set keyboard LEDs */
#define KB_CMD_ECHO                 0xEE    /* Echo command */
#define KB_CMD_SET_SCANCODE_SET     0xF0    /* Set scancode set */
#define KB_CMD_IDENTIFY             0xF2    /* Identify keyboard */
#define KB_CMD_SET_TYPEMATIC        0xF3    /* Set typematic rate and delay */
#define KB_CMD_ENABLE_SCANNING      0xF4    /* Enable scanning */
#define KB_CMD_DISABLE_SCANNING     0xF5    /* Disable scanning */
#define KB_CMD_SET_DEFAULTS         0xF6    /* Set default parameters */
#define KB_CMD_RESEND               0xFE    /* Resend last byte */
#define KB_CMD_RESET                0xFF    /* Reset keyboard */

/*------------------------------------------------------------------------------
 * Keyboard Response Codes
 *------------------------------------------------------------------------------
 */
#define KB_RESPONSE_ACK             0xFA    /* Command acknowledged */
#define KB_RESPONSE_RESEND          0xFE    /* Resend request */
#define KB_RESPONSE_ERROR           0xFC    /* Error */

/*------------------------------------------------------------------------------
 * Special Scancode Values
 *------------------------------------------------------------------------------
 */
#define SCANCODE_EXTENDED           0xE0    /* Extended scancode prefix */
#define SCANCODE_RELEASE            0x80    /* Key release bit mask */

/*------------------------------------------------------------------------------
 * Input Buffer Configuration
 *------------------------------------------------------------------------------
 */
#define KEYBOARD_BUFFER_SIZE        256     /* Size of keyboard input buffer */

/*------------------------------------------------------------------------------
 * Keyboard State Structure
 *------------------------------------------------------------------------------
 */
typedef struct {
    bool shift_pressed;         /* Shift key state */
    bool ctrl_pressed;          /* Control key state */
    bool alt_pressed;           /* Alt key state */
    bool caps_lock;             /* Caps lock state */
    bool num_lock;              /* Num lock state */
    bool scroll_lock;           /* Scroll lock state */
    bool extended_scancode;     /* Processing extended scancode */
    bool debug_mode;            /* Scancode debug mode active */
} keyboard_state_t;

/*------------------------------------------------------------------------------
 * Input Buffer Structure
 *------------------------------------------------------------------------------
 */
typedef struct {
    char buffer[KEYBOARD_BUFFER_SIZE];  /* Character buffer */
    size_t read_pos;                    /* Read position */
    size_t write_pos;                   /* Write position */
    size_t count;                       /* Number of characters in buffer */
} input_buffer_t;

/*------------------------------------------------------------------------------
 * Function Declarations
 *------------------------------------------------------------------------------
 */

/**
 * @brief Initialize the PS/2 keyboard driver
 * 
 * This function:
 * 1. Initializes the 8042 PS/2 controller
 * 2. Enables keyboard interrupts
 * 3. Sets up the input buffer
 * 4. Configures the keyboard for operation
 */
void keyboard_init(void);

/**
 * @brief Handle keyboard interrupt (IRQ1)
 * 
 * This function is called from the interrupt handler when a keyboard
 * interrupt occurs. It reads the scancode from the keyboard and processes it.
 */
void keyboard_interrupt_handler(void);

/**
 * @brief Convert scancode to ASCII character
 * 
 * @param scancode The scancode received from the keyboard
 * @return char The corresponding ASCII character, or 0 for non-printable keys
 */
char scancode_to_ascii(uint8_t scancode);

/**
 * @brief Read a character from the keyboard buffer
 * 
 * @return char The next character from the buffer, or 0 if buffer is empty
 */
char keyboard_getchar(void);

/**
 * @brief Check if the keyboard buffer has data available
 * 
 * @return bool True if data is available, false otherwise
 */
bool keyboard_has_data(void);

/**
 * @brief Read a line of input from the keyboard
 * 
 * This function blocks until a complete line is entered (terminated by Enter).
 * It handles basic line editing like backspace.
 * 
 * @param buffer Buffer to store the input line
 * @param max_length Maximum length of the input line
 * @return size_t Number of characters read
 */
size_t keyboard_readline(char* buffer, size_t max_length);

/**
 * @brief Get the current keyboard state
 * 
 * @return keyboard_state_t* Pointer to the current keyboard state
 */
keyboard_state_t* keyboard_get_state(void);

/**
 * @brief Wait for keyboard controller to be ready for input
 */
void keyboard_wait_input(void);

/**
 * @brief Wait for keyboard controller to have output ready
 */
void keyboard_wait_output(void);

/**
 * @brief Send a command to the keyboard
 * 
 * @param command The command byte to send
 * @return bool True if command was acknowledged, false otherwise
 */
bool keyboard_send_command(uint8_t command);

/**
 * @brief Update keyboard LEDs based on current state
 */
void keyboard_update_leds(void);

/**
 * @brief Enable scancode debug mode
 */
void keyboard_enable_debug_mode(void);

/**
 * @brief Disable scancode debug mode
 */
void keyboard_disable_debug_mode(void);

/**
 * @brief Check if debug mode is active
 * 
 * @return bool True if debug mode is active, false otherwise
 */
bool keyboard_is_debug_mode_active(void);

#endif /* KEYBOARD_H */