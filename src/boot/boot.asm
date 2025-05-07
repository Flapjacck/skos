;===========================================
; SKOS Bootloader
; - Displays a cool ASCII art logo
; - Shows a loading animation
; - Sets up basic system parameters
;===========================================

[BITS 16]                    ; Tell assembler to generate 16-bit code (Real Mode)
[ORG 0x7C00]                 ; Set origin to 0x7C00 (where BIOS loads bootloader)

;-------------------------------------------
; Initialize system segments
;-------------------------------------------
    xor ax, ax              ; Zero out AX register
    mov ds, ax              ; Set Data Segment to 0
    mov es, ax              ; Set Extra Segment to 0
    mov ss, ax              ; Set Stack Segment to 0
    mov sp, 0x7C00         ; Set Stack Pointer to bootloader start

;-------------------------------------------
; Clear the screen
;-------------------------------------------
    mov ah, 0x00           ; BIOS video function (Set video mode)
    mov al, 0x03           ; Mode 3 (80x25 text mode)
    int 0x10               ; Call BIOS video interrupt

;-------------------------------------------
; Display SKOS logo
;-------------------------------------------
    mov si, skos_logo      ; Load logo address into Source Index
    call print_string      ; Print it!

;-------------------------------------------
; Show loading animation with progress bar
;-------------------------------------------
    mov si, loading_msg    ; "Loading SKOS" text
    call print_string
    
    mov cx, 10             ; Number of progress bar segments
.loading_loop:
    push cx                ; Save our counter
    mov ah, 0x0E          ; BIOS teletype function
    mov al, '='           ; Progress bar character
    int 0x10              ; Print it
    pop cx                ; Restore counter
    loop .loading_loop    ; Repeat until done
    
    mov si, newline       ; Move to next line
    call print_string

;-------------------------------------------
; Display welcome message
;-------------------------------------------
    mov si, hello_msg
    call print_string

;-------------------------------------------
; System halted - infinite loop
;-------------------------------------------
halt:
    jmp halt              ; Loop forever

;===========================================
; Function: print_string
; Input: SI = pointer to null-terminated string
; Output: None
; Destroys: None (saves all registers)
;===========================================
print_string:
    pusha                 ; Save all registers
.loop:
    lodsb                 ; Load byte from SI into AL
    test al, al          ; Check if we hit null terminator
    jz .done             ; If zero, we're done
    mov ah, 0x0E         ; BIOS teletype function
    int 0x10             ; Print character
    jmp .loop            ; Next character
.done:
    popa                 ; Restore all registers
    ret

;===========================================
; Data Section
;===========================================
skos_logo:    db '_____ __ ______  _____', 0x0D, 0x0A          ; Cool ASCII art logo
             db '  / ___// //_/ __ \/ ___/', 0x0D, 0x0A
             db '  \__ \/ ,< / / / /\__ \ ', 0x0D, 0x0A
             db ' ___/ / /| / /_/ /___/ / ', 0x0D, 0x0A
             db '/____/_/ |_\____//____/  ', 0x0D, 0x0A, 0
loading_msg: db 'Loading SKOS ', 0                             ; Loading message
newline:    db 0x0D, 0x0A, 0                                  ; Carriage return + line feed
hello_msg:  db 'Welcome to SKOS!', 0x0D, 0x0A, 0              ; Welcome message

;===========================================
; Boot Sector Magic
;===========================================
times 510-($-$$) db 0    ; Pad with zeros until 510 bytes
dw 0xAA55                ; Boot signature (required by BIOS)