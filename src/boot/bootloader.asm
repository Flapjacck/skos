;------------------------------------------------------------------------------
; SKOS Bootloader - GRUB Multiboot compliant
;------------------------------------------------------------------------------
; This bootloader follows the Multiboot specification required by GRUB
; It sets up the initial environment for our kernel
;------------------------------------------------------------------------------

; Declare constants for the multiboot header
MBALIGN     equ  1 << 0            ; align loaded modules on page boundaries
MEMINFO     equ  1 << 1            ; provide memory map
FLAGS       equ  MBALIGN | MEMINFO ; this is the Multiboot 'flag' field
MAGIC       equ  0x1BADB002        ; 'magic number' lets bootloader find the header
CHECKSUM    equ -(MAGIC + FLAGS)   ; checksum of above, to prove we are multiboot

; Declare a multiboot header that marks the program as a kernel
section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

; Allocate a small stack
section .bss
align 16
stack_bottom:
    resb 16384 ; 16 KiB
stack_top:

; The kernel entry point
section .text
global _start:function (_start.end - _start)
_start:
    ; The bootloader has loaded us into 32-bit protected mode on a x86
    ; machine. Interrupts are disabled. Paging is disabled. The processor
    ; state is as defined in the multiboot standard. The kernel has full
    ; control of the CPU. The kernel can only make use of hardware features
    ; and any code it provides as part of itself.

    ; Set up the stack
    mov esp, stack_top

    ; This is where we call the C function from our kernel
    extern kernel_main ; Declare that we will be referencing the external kernel_main symbol
    call kernel_main  ; Call our main kernel function

    ; If kernel_main returns, loop forever
    cli               ; Disable interrupts
.hang:
    hlt              ; Halt the CPU
    jmp .hang        ; If that didn't work, jump back to .hang
.end: