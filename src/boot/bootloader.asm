;------------------------------------------------------------------------------
; Simple Bootloader
;------------------------------------------------------------------------------
; When the computer starts, the BIOS:
; 1. Performs POST (Power-On Self Test)
; 2. Loads the first sector (512 bytes) from the boot device into memory at 0x7C00
; 3. Jumps to 0x7C00 to execute our bootloader
;------------------------------------------------------------------------------

BITS 16              ; Tell assembler we're using 16-bit real mode
                    ; Real mode: Basic operating mode of x86 processors
                    ; Limited to 1MB of memory and no hardware protection

org 0x7c00          ; Origin directive: Tell assembler our code will be loaded at 0x7C00
                    ; This helps calculate correct memory offsets

;------------------------------------------------------------------------------
; Print "Hello" message using BIOS video services
;------------------------------------------------------------------------------
mov ah, 0x0e        ; BIOS teletype output function
                    ; AL = character to print
                    ; AH = 0x0E (function number for teletype output)
mov si, msg         ; SI = Source Index register, point it to our message
                    ; SI is commonly used for string operations

print:
    lodsb           ; Load byte at SI into AL and increment SI
                    ; This loads our message one character at a time
    or al, al       ; Bitwise OR of AL with itself - sets flags
                    ; If AL = 0 (string terminator), ZF (zero flag) will be set
    jz done         ; Jump if zero flag is set (end of string)
    int 0x10        ; BIOS video interrupt
                    ; Uses AH=0x0E (set above) to print char in AL
    jmp print       ; Loop to next character
done:

;------------------------------------------------------------------------------
; Switch to Protected Mode and load kernel
;------------------------------------------------------------------------------
cli                 ; Disable interrupts

; Enable A20 line
in al, 0x92         ; Fast A20 Gate
or al, 2
out 0x92, al

; Load GDT
lgdt [gdt_descriptor]

; Enable Protected Mode
mov eax, cr0
or eax, 1
mov cr0, eax

; Far jump to 32-bit code
jmp 0x08:protected_mode

BITS 32             ; From this point on, we're in 32-bit mode

protected_mode:
    ; Set up segment registers
    mov ax, 0x10    ; Data segment selector
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax

    ; Copy kernel from disk to 1MB
    mov esi, 0x7E00 ; Source: right after bootloader
    mov edi, 0x100000 ; Destination: 1MB mark (matches linker script)
    mov ecx, 512    ; Copy 512 bytes (one sector)
    rep movsb       ; Repeat move string byte

    ; Jump to kernel
    jmp 0x100000    ; Jump to kernel entry at 1MB

;------------------------------------------------------------------------------
; Global Descriptor Table
;------------------------------------------------------------------------------
gdt_start:
    ; Null descriptor
    dd 0x0
    dd 0x0

    ; Code segment descriptor
    dw 0xFFFF       ; Limit (bits 0-15)
    dw 0x0          ; Base (bits 0-15)
    db 0x0          ; Base (bits 16-23)
    db 10011010b    ; Access byte
    db 11001111b    ; Flags + Limit (bits 16-19)
    db 0x0          ; Base (bits 24-31)

    ; Data segment descriptor
    dw 0xFFFF       ; Limit (bits 0-15)
    dw 0x0          ; Base (bits 0-15)
    db 0x0          ; Base (bits 16-23)
    db 10010010b    ; Access byte
    db 11001111b    ; Flags + Limit (bits 16-19)
    db 0x0          ; Base (bits 24-31)

gdt_descriptor:
    dw $ - gdt_start - 1  ; GDT size
    dd gdt_start          ; GDT address

;------------------------------------------------------------------------------
; Error handler
;------------------------------------------------------------------------------
disk_error:
    cli             ; Clear interrupt flag - disable interrupts
    hlt             ; Halt the CPU

;------------------------------------------------------------------------------
; Data section
;------------------------------------------------------------------------------
msg db 'Hello', 0   ; Define null-terminated string
                    ; db = define byte

;------------------------------------------------------------------------------
; Boot sector padding and signature
;------------------------------------------------------------------------------
times 510-($-$$) db 0 ; Pad with zeros until 510 bytes
                      ; $ = current position
                      ; $$ = start of section
                      ; ($-$$) = size of code so far
dw 0xaa55             ; Boot signature required by BIOS
                      ; 0xAA55 = magic number that identifies this as a boot sector