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
; Load kernel from disk
;------------------------------------------------------------------------------
mov ax, 0x1000      ; Set segment where we'll load the kernel
mov es, ax          ; ES = Extra Segment register
mov bx, 0x0000      ; ES:BX = 0x1000:0000 = Physical address 0x10000

; Set up disk read operation
mov ah, 0x02        ; BIOS function: read disk sectors
mov al, 1           ; Number of sectors to read
mov ch, 0           ; Cylinder number (0-based)
mov cl, 2           ; Sector number (1-based, sector 2 = right after bootloader)
mov dh, 0           ; Head number
mov dl, 0           ; Drive number (0 = first floppy, 0x80 = first hard disk)
int 0x13            ; BIOS disk interrupt
                    ; If successful: CF (carry flag) = 0
                    ; If error: CF = 1
jc disk_error       ; Jump if carry flag is set (error occurred)

;------------------------------------------------------------------------------
; Jump to kernel
;------------------------------------------------------------------------------
jmp 0x1000:0000     ; Far jump to kernel
                    ; Changes CS (Code Segment) to 0x1000
                    ; Sets IP (Instruction Pointer) to 0x0000

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
