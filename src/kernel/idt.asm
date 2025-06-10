;------------------------------------------------------------------------------
; IDT Assembly Support Functions
;------------------------------------------------------------------------------
; This file contains assembly language functions required for IDT operations
; that cannot be implemented in C due to direct CPU instruction requirements.
;
; The main components here are:
; 1. idt_flush - Loads the new IDT using LIDT instruction
; 2. ISR stubs (isr0-isr31) - CPU exception handlers
; 3. IRQ stubs (irq0-irq15) - Hardware interrupt handlers
; 4. Common interrupt handler - Saves state and calls C function
;------------------------------------------------------------------------------

[GLOBAL idt_flush]    ; Make idt_flush accessible from C code

;------------------------------------------------------------------------------
; CPU Exception Handlers (ISRs 0-31)
;------------------------------------------------------------------------------
; These are the Interrupt Service Routine stubs for CPU exceptions.
; Each one pushes a unique interrupt number and calls the common handler.
; Some exceptions push an error code automatically, others don't, so we
; need to push a dummy error code (0) for consistency.
;------------------------------------------------------------------------------

[GLOBAL isr0]   ; Divide Error (#DE)
[GLOBAL isr1]   ; Debug Exception (#DB)
[GLOBAL isr2]   ; NMI Interrupt
[GLOBAL isr3]   ; Breakpoint (#BP)
[GLOBAL isr4]   ; Overflow (#OF)
[GLOBAL isr5]   ; BOUND Range Exceeded (#BR)
[GLOBAL isr6]   ; Invalid Opcode (#UD)
[GLOBAL isr7]   ; Device Not Available (#NM)
[GLOBAL isr8]   ; Double Fault (#DF)
[GLOBAL isr9]   ; Coprocessor Segment Overrun
[GLOBAL isr10]  ; Invalid TSS (#TS)
[GLOBAL isr11]  ; Segment Not Present (#NP)
[GLOBAL isr12]  ; Stack Segment Fault (#SS)
[GLOBAL isr13]  ; General Protection Fault (#GP)
[GLOBAL isr14]  ; Page Fault (#PF)
[GLOBAL isr15]  ; Reserved
[GLOBAL isr16]  ; x87 FPU Error (#MF)
[GLOBAL isr17]  ; Alignment Check (#AC)
[GLOBAL isr18]  ; Machine Check (#MC)
[GLOBAL isr19]  ; SIMD Floating-Point Exception (#XM)
[GLOBAL isr20]  ; Virtualization Exception (#VE)
[GLOBAL isr21]  ; Control Protection Exception (#CP)
[GLOBAL isr22]  ; Reserved
[GLOBAL isr23]  ; Reserved
[GLOBAL isr24]  ; Reserved
[GLOBAL isr25]  ; Reserved
[GLOBAL isr26]  ; Reserved
[GLOBAL isr27]  ; Reserved
[GLOBAL isr28]  ; Reserved
[GLOBAL isr29]  ; Reserved
[GLOBAL isr30]  ; Reserved
[GLOBAL isr31]  ; Reserved

;------------------------------------------------------------------------------
; Hardware IRQ Handlers (IRQs 0-15)
;------------------------------------------------------------------------------
; These are the Interrupt Service Routine stubs for hardware interrupts.
; They are mapped to interrupt vectors 32-47 in the IDT.
;------------------------------------------------------------------------------

[GLOBAL irq0]   ; Timer Interrupt
[GLOBAL irq1]   ; Keyboard Interrupt
[GLOBAL irq2]   ; Cascade (used internally by PICs)
[GLOBAL irq3]   ; COM2
[GLOBAL irq4]   ; COM1
[GLOBAL irq5]   ; LPT2
[GLOBAL irq6]   ; Floppy Disk
[GLOBAL irq7]   ; LPT1
[GLOBAL irq8]   ; CMOS Real-Time Clock
[GLOBAL irq9]   ; Free for peripherals
[GLOBAL irq10]  ; Free for peripherals
[GLOBAL irq11]  ; Free for peripherals
[GLOBAL irq12]  ; PS/2 Mouse
[GLOBAL irq13]  ; FPU / Coprocessor
[GLOBAL irq14]  ; Primary ATA Hard Disk
[GLOBAL irq15]  ; Secondary ATA Hard Disk

;------------------------------------------------------------------------------
; External C function declaration
;------------------------------------------------------------------------------
[EXTERN interrupt_handler]  ; Our C interrupt handler function

;------------------------------------------------------------------------------
; idt_flush - Load IDT and update interrupt handling
;------------------------------------------------------------------------------
; This function performs the critical task of loading a new IDT.
; It takes a pointer to the IDT pointer structure and uses LIDT to load it.
;
; Parameters (passed on stack):
;   [esp+4] = Pointer to IDT pointer structure (contains limit and base)
;
; The function simply loads the IDT - no segment register updates are needed
; for the IDT (unlike the GDT which requires segment register reloads).
;------------------------------------------------------------------------------
idt_flush:
    mov eax, [esp+4]        ; Get the pointer to IDT pointer structure from stack
                            ; eax now points to our idt_ptr structure
    
    lidt [eax]              ; Load the new IDT
                            ; This instruction tells the CPU where our IDT is located
                            ; [eax] points to: [limit:16][base:32] structure
    
    ret                     ; Return to C code - IDT is now active

;------------------------------------------------------------------------------
; CPU Exception Handlers - No Error Code
;------------------------------------------------------------------------------
; These exceptions do not push an error code, so we push a dummy 0 for
; consistency with exceptions that do push an error code.
;------------------------------------------------------------------------------

; ISR 0: Divide Error - No error code
isr0:
    cli                     ; Disable interrupts
    push byte 0             ; Push dummy error code
    push byte 0             ; Push interrupt number
    jmp isr_common_stub     ; Jump to common handler

; ISR 1: Debug Exception - No error code
isr1:
    cli
    push byte 0
    push byte 1
    jmp isr_common_stub

; ISR 2: NMI Interrupt - No error code
isr2:
    cli
    push byte 0
    push byte 2
    jmp isr_common_stub

; ISR 3: Breakpoint - No error code
isr3:
    cli
    push byte 0
    push byte 3
    jmp isr_common_stub

; ISR 4: Overflow - No error code
isr4:
    cli
    push byte 0
    push byte 4
    jmp isr_common_stub

; ISR 5: BOUND Range Exceeded - No error code
isr5:
    cli
    push byte 0
    push byte 5
    jmp isr_common_stub

; ISR 6: Invalid Opcode - No error code
isr6:
    cli
    push byte 0
    push byte 6
    jmp isr_common_stub

; ISR 7: Device Not Available - No error code
isr7:
    cli
    push byte 0
    push byte 7
    jmp isr_common_stub

;------------------------------------------------------------------------------
; CPU Exception Handlers - With Error Code
;------------------------------------------------------------------------------
; These exceptions automatically push an error code, so we don't push a dummy.
;------------------------------------------------------------------------------

; ISR 8: Double Fault - Pushes error code
isr8:
    cli
    push byte 8             ; Push interrupt number (error code already pushed)
    jmp isr_common_stub

; ISR 9: Coprocessor Segment Overrun - No error code
isr9:
    cli
    push byte 0
    push byte 9
    jmp isr_common_stub

; ISR 10: Invalid TSS - Pushes error code
isr10:
    cli
    push byte 10
    jmp isr_common_stub

; ISR 11: Segment Not Present - Pushes error code
isr11:
    cli
    push byte 11
    jmp isr_common_stub

; ISR 12: Stack Segment Fault - Pushes error code
isr12:
    cli
    push byte 12
    jmp isr_common_stub

; ISR 13: General Protection Fault - Pushes error code
isr13:
    cli
    push byte 13
    jmp isr_common_stub

; ISR 14: Page Fault - Pushes error code
isr14:
    cli
    push byte 14
    jmp isr_common_stub

; ISR 15: Reserved - No error code
isr15:
    cli
    push byte 0
    push byte 15
    jmp isr_common_stub

; ISR 16: x87 FPU Error - No error code
isr16:
    cli
    push byte 0
    push byte 16
    jmp isr_common_stub

; ISR 17: Alignment Check - Pushes error code
isr17:
    cli
    push byte 17
    jmp isr_common_stub

; ISR 18: Machine Check - No error code
isr18:
    cli
    push byte 0
    push byte 18
    jmp isr_common_stub

; ISR 19: SIMD Floating-Point Exception - No error code
isr19:
    cli
    push byte 0
    push byte 19
    jmp isr_common_stub

; ISR 20: Virtualization Exception - No error code
isr20:
    cli
    push byte 0
    push byte 20
    jmp isr_common_stub

; ISR 21: Control Protection Exception - Pushes error code
isr21:
    cli
    push byte 21
    jmp isr_common_stub

; ISRs 22-31: Reserved - No error code
isr22:
    cli
    push byte 0
    push byte 22
    jmp isr_common_stub

isr23:
    cli
    push byte 0
    push byte 23
    jmp isr_common_stub

isr24:
    cli
    push byte 0
    push byte 24
    jmp isr_common_stub

isr25:
    cli
    push byte 0
    push byte 25
    jmp isr_common_stub

isr26:
    cli
    push byte 0
    push byte 26
    jmp isr_common_stub

isr27:
    cli
    push byte 0
    push byte 27
    jmp isr_common_stub

isr28:
    cli
    push byte 0
    push byte 28
    jmp isr_common_stub

isr29:
    cli
    push byte 0
    push byte 29
    jmp isr_common_stub

isr30:
    cli
    push byte 0
    push byte 30
    jmp isr_common_stub

isr31:
    cli
    push byte 0
    push byte 31
    jmp isr_common_stub

;------------------------------------------------------------------------------
; Hardware IRQ Handlers (32-47)
;------------------------------------------------------------------------------
; These handle hardware interrupts. They are remapped to vectors 32-47
; to avoid conflicts with CPU exceptions (0-31).
; None of these push error codes.
;------------------------------------------------------------------------------

; IRQ 0: Timer Interrupt (Vector 32)
irq0:
    cli
    push byte 0             ; Push dummy error code
    push byte 32            ; Push interrupt number
    jmp irq_common_stub     ; Jump to IRQ common handler

; IRQ 1: Keyboard Interrupt (Vector 33)
irq1:
    cli
    push byte 0
    push byte 33
    jmp irq_common_stub

; IRQ 2: Cascade (Vector 34)
irq2:
    cli
    push byte 0
    push byte 34
    jmp irq_common_stub

; IRQ 3: COM2 (Vector 35)
irq3:
    cli
    push byte 0
    push byte 35
    jmp irq_common_stub

; IRQ 4: COM1 (Vector 36)
irq4:
    cli
    push byte 0
    push byte 36
    jmp irq_common_stub

; IRQ 5: LPT2 (Vector 37)
irq5:
    cli
    push byte 0
    push byte 37
    jmp irq_common_stub

; IRQ 6: Floppy Disk (Vector 38)
irq6:
    cli
    push byte 0
    push byte 38
    jmp irq_common_stub

; IRQ 7: LPT1 (Vector 39)
irq7:
    cli
    push byte 0
    push byte 39
    jmp irq_common_stub

; IRQ 8: CMOS Real-Time Clock (Vector 40)
irq8:
    cli
    push byte 0
    push byte 40
    jmp irq_common_stub

; IRQ 9: Free for peripherals (Vector 41)
irq9:
    cli
    push byte 0
    push byte 41
    jmp irq_common_stub

; IRQ 10: Free for peripherals (Vector 42)
irq10:
    cli
    push byte 0
    push byte 42
    jmp irq_common_stub

; IRQ 11: Free for peripherals (Vector 43)
irq11:
    cli
    push byte 0
    push byte 43
    jmp irq_common_stub

; IRQ 12: PS/2 Mouse (Vector 44)
irq12:
    cli
    push byte 0
    push byte 44
    jmp irq_common_stub

; IRQ 13: FPU / Coprocessor (Vector 45)
irq13:
    cli
    push byte 0
    push byte 45
    jmp irq_common_stub

; IRQ 14: Primary ATA Hard Disk (Vector 46)
irq14:
    cli
    push byte 0
    push byte 46
    jmp irq_common_stub

; IRQ 15: Secondary ATA Hard Disk (Vector 47)
irq15:
    cli
    push byte 0
    push byte 47
    jmp irq_common_stub

;------------------------------------------------------------------------------
; Common ISR Stub
;------------------------------------------------------------------------------
; This is called by all ISR handlers after they push their interrupt number
; and error code (or dummy error code). It saves all processor state,
; calls our C interrupt handler, then restores state and returns.
;
; Stack layout when this is called:
; [ESP+0]  = Interrupt number (pushed by ISR)
; [ESP+4]  = Error code (real or dummy, pushed by ISR or CPU)
; [ESP+8]  = EIP (pushed by CPU)
; [ESP+12] = CS (pushed by CPU)
; [ESP+16] = EFLAGS (pushed by CPU)
; [ESP+20] = ESP (pushed by CPU if privilege change occurred)
; [ESP+24] = SS (pushed by CPU if privilege change occurred)
;------------------------------------------------------------------------------
isr_common_stub:
    pusha                   ; Push all general-purpose registers
                            ; This pushes: EAX, ECX, EDX, EBX, ESP, EBP, ESI, EDI
    
    mov ax, ds              ; Save current data segment
    push eax                ; Push it onto stack
    
    mov ax, 0x10            ; Load kernel data segment (from our GDT)
    mov ds, ax              ; Set data segment
    mov es, ax              ; Set extra segment
    mov fs, ax              ; Set F segment
    mov gs, ax              ; Set G segment
    
    push esp                ; Push pointer to interrupt_registers_t structure
    call interrupt_handler  ; Call our C interrupt handler
    add esp, 4              ; Clean up parameter from stack
    
    pop eax                 ; Restore original data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    popa                    ; Restore all general-purpose registers
    add esp, 8              ; Clean up error code and interrupt number from stack
    sti                     ; Re-enable interrupts
    iret                    ; Return from interrupt

;------------------------------------------------------------------------------
; Common IRQ Stub
;------------------------------------------------------------------------------
; This is similar to the ISR stub but specifically for hardware interrupts.
; The main difference is that IRQs may need special handling like sending
; End of Interrupt (EOI) signals to the interrupt controller.
;------------------------------------------------------------------------------
irq_common_stub:
    pusha                   ; Push all general-purpose registers
    
    mov ax, ds              ; Save current data segment
    push eax
    
    mov ax, 0x10            ; Load kernel data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    push esp                ; Push pointer to interrupt_registers_t structure
    call interrupt_handler  ; Call our C interrupt handler
    add esp, 4              ; Clean up parameter from stack
    
    pop eax                 ; Restore original data segment
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    
    popa                    ; Restore all general-purpose registers
    add esp, 8              ; Clean up error code and interrupt number
    sti                     ; Re-enable interrupts
    iret                    ; Return from interrupt

;------------------------------------------------------------------------------
; End of IDT Assembly Support
;------------------------------------------------------------------------------
