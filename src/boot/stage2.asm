;===========================================
; SKOS Second Stage Bootloader
; - Sets up protected mode
; - Loads kernel from disk
; - Initializes essential hardware
; - Transfers control to kernel
;===========================================

[BITS 16]                    ; Start in 16-bit real mode
[ORG 0x1000]                ; Stage 2 is loaded at 0x1000

;-------------------------------------------
; Initialize segments for stage 2
;-------------------------------------------
    cli                     ; Disable interrupts
    mov ax, 0x0000         ; Setup segments
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov sp, 0x9000         ; Set up a new stack
    sti                    ; Enable interrupts

;-------------------------------------------
; Display stage 2 loaded message (only once)
;-------------------------------------------
    mov si, stage2_msg     
    call print_string

;-------------------------------------------
; Basic A20 Line enable
;-------------------------------------------
    in al, 0x92            ; Fast A20 gate method
    or al, 2
    out 0x92, al

;-------------------------------------------
; For now, halt the system
;-------------------------------------------
    mov si, halt_msg
    call print_string
    jmp halt              ; Jump to halt state

;-------------------------------------------
; Function: print_string (Real mode)
; Input: SI = pointer to null-terminated string
;-------------------------------------------
print_string:
    pusha
.loop:
    lodsb                  ; Load byte from SI into AL
    test al, al           ; Check for null terminator
    jz .done
    mov ah, 0x0E          ; BIOS teletype
    int 0x10
    jmp .loop
.done:
    popa
    ret

;-------------------------------------------
; System halt - infinite loop
;-------------------------------------------
halt:
    cli                    ; Disable interrupts
    hlt                    ; Halt CPU
    jmp halt              ; Just in case of interrupt

;===========================================
; Data Section
;===========================================
stage2_msg:  db 'SKOS Stage 2 Loaded', 0x0D, 0x0A, 0
halt_msg:    db 'System Halted', 0x0D, 0x0A, 0
error_msg:   db 'Error loading kernel', 0x0D, 0x0A, 0

;===========================================
; Padding
;===========================================
times 512-($-$$) db 0      ; Pad to 512 bytes