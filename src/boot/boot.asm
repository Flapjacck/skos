;===========================================
; SKOS First Stage Bootloader
; - Displays a cool ASCII art logo
; - Shows a loading animation
; - Loads second stage bootloader
;===========================================

[BITS 16]                    ; Tell assembler to generate 16-bit code (Real Mode)
[ORG 0x7C00]                 ; Set origin to 0x7C00 (where BIOS loads bootloader)

STAGE2_SEGMENT equ 0x0100    ; Segment where we'll load stage2
STAGE2_OFFSET  equ 0x0000    ; Offset where we'll load stage2
STAGE2_SECTORS equ 1         ; Number of sectors to load

;-------------------------------------------
; Initialize system segments
;-------------------------------------------
    cli                     ; Disable interrupts during setup
    xor ax, ax              ; Zero out AX register
    mov ds, ax              ; Set Data Segment to 0
    mov es, ax              ; Set Extra Segment to 0
    mov ss, ax              ; Set Stack Segment to 0
    mov sp, 0x7C00          ; Set Stack Pointer to bootloader start
    sti                     ; Re-enable interrupts

;-------------------------------------------
; Save boot drive number
;-------------------------------------------
    mov [bootDrive], dl     ; BIOS provides boot drive in DL

;-------------------------------------------
; Clear the screen
;-------------------------------------------
    mov ah, 0x00            ; BIOS video function (Set video mode)
    mov al, 0x03            ; Mode 3 (80x25 text mode)
    int 0x10                ; Call BIOS video interrupt

;-------------------------------------------
; Display SKOS logo
;-------------------------------------------
    mov si, skos_logo       ; Load logo address into Source Index
    call print_string       ; Print it!

;-------------------------------------------
; Show loading animation with progress bar
;-------------------------------------------
    mov si, loading_msg     ; "Loading SKOS" text
    call print_string
    
    mov cx, 10              ; Number of progress bar segments
.loading_loop:
    push cx                 ; Save our counter
    mov ah, 0x0E            ; BIOS teletype function
    mov al, '='             ; Progress bar character
    int 0x10                ; Print it

    ; Add a small delay
    push cx
    mov cx, 0xFFFF
.delay:
    loop .delay
    pop cx
    
    pop cx                  ; Restore counter
    loop .loading_loop      ; Repeat until done
    
    mov si, newline         ; Move to next line
    call print_string

;-------------------------------------------
; Load Stage 2
;-------------------------------------------
    mov si, stage2_msg
    call print_string

    ; Reset disk system
    xor ax, ax
    int 0x13                ; Reset disk system

    mov ax, STAGE2_SEGMENT  ; Set up ES:BX for memory location
    mov es, ax              ; where we'll load stage 2
    xor bx, bx

    mov ah, 0x02            ; BIOS read sectors function
    mov al, STAGE2_SECTORS  ; Number of sectors to read
    mov ch, 0               ; Cylinder 0
    mov cl, 2               ; Start from sector 2 (sector 1 is boot sector)
    mov dh, 0               ; Head 0
    mov dl, [bootDrive]     ; Drive number
    int 0x13                ; BIOS disk interrupt

    jc disk_error           ; If carry flag set, there was an error

    ; Small delay before jumping to stage 2
    mov cx, 0xFFFF
.final_delay:
    loop .final_delay

    ; Jump to stage 2
    jmp STAGE2_SEGMENT:STAGE2_OFFSET

;-------------------------------------------
; Error handler
;-------------------------------------------
disk_error:
    mov si, disk_error_msg
    call print_string
    jmp halt

;-------------------------------------------
; System halted - infinite loop
;-------------------------------------------
halt:
    cli                     ; Disable interrupts
    hlt                     ; Halt CPU
    jmp halt                ; Just in case of interrupt

;===========================================
; Function: print_string
; Input: SI = pointer to null-terminated string
; Output: None
; Destroys: None (saves all registers)
;===========================================
print_string:
    pusha                   ; Save all registers
.loop:
    lodsb                   ; Load byte from SI into AL
    test al, al             ; Check if we hit null terminator
    jz .done                ; If zero, we're done
    mov ah, 0x0E            ; BIOS teletype function
    int 0x10                ; Print character
    jmp .loop               ; Next character
.done:
    popa                    ; Restore all registers
    ret

;===========================================
; Data Section
;===========================================
bootDrive:    db 0
skos_logo:    db '   _____ __ ______  _____', 0x0D, 0x0A          ; Cool ASCII art logo
             db '  / ___// //_/ __ \/ ___/', 0x0D, 0x0A
             db '  \__ \/ ,< / / / /\__ \ ', 0x0D, 0x0A
             db ' ___/ / /| / /_/ /___/ / ', 0x0D, 0x0A
             db '/____/_/ |_\____//____/  ', 0x0D, 0x0A, 0
loading_msg: db 'Loading SKOS ', 0                                ; Loading message
stage2_msg:  db 'Loading Stage 2...', 0x0D, 0x0A, 0              ; Stage 2 loading message
disk_error_msg: db 'Disk read error!', 0x0D, 0x0A, 0             ; Error message
newline:    db 0x0D, 0x0A, 0                                     ; Carriage return + line feed

;===========================================
; Boot Sector Magic
;===========================================
times 510-($-$$) db 0       ; Pad with zeros until 510 bytes
dw 0xAA55                   ; Boot signature (required by BIOS)