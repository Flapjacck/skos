/*------------------------------------------------------------------------------
 * Simple Kernel
 *------------------------------------------------------------------------------
 * This is the entry point of our kernel after the bootloader transfers control.
 * The kernel is loaded at memory address 0x1000 by the bootloader.
 *------------------------------------------------------------------------------
 */

/* kmain: Kernel entry point - no parameters, doesn't return */
void kmain(void) {
    /* Video memory constants */
    char *video = (char*)0xb8000;    /* Base address of VGA text mode memory
                                     * In text mode, each character on screen
                                     * takes 2 bytes:
                                     * - First byte: ASCII character
                                     * - Second byte: Attribute (colors) */
    
    const char *msg = "Kernel in C!";  /* Message to display on screen */

    /* Write message to video memory */
    for (int i = 0; msg[i] != 0; i++) {
        video[i * 2] = msg[i];         /* Character byte: ASCII value */
        video[i * 2 + 1] = 0x07;       /* Attribute byte: 0x07 = light gray on black
                                       * Upper 4 bits (0): black background
                                       * Lower 4 bits (7): light gray foreground */
    }

    /* Infinite loop to prevent the CPU from executing beyond our code */
    while(1);    /* The CPU will continue executing this loop forever
                 * This is necessary because we don't have an operating
                 * system to return to */
}
