;------------------------------------------------------------------------------
; GDT Assembly Support Functions
;------------------------------------------------------------------------------
; This file contains assembly language functions required for GDT operations
; that cannot be implemented in C due to direct CPU instruction requirements.
;
; The main function here is gdt_flush, which loads the new GDT and updates
; all segment registers to use the new descriptors.
;------------------------------------------------------------------------------

[GLOBAL gdt_flush]    ; Make gdt_flush accessible from C code

;------------------------------------------------------------------------------
; gdt_flush - Load GDT and update segment registers
;------------------------------------------------------------------------------
; This function performs the critical task of loading a new GDT and updating
; all segment registers. This process requires assembly because:
; 1. LGDT instruction must be called directly
; 2. Far jump is needed to reload CS (code segment register)
; 3. Other segment registers must be manually updated
;
; Parameters (passed on stack):
;   [esp+4] = Pointer to GDT pointer structure (contains limit and base)
;
; The function performs these operations:
; 1. Load the new GDT using LGDT instruction
; 2. Perform a far jump to reload CS with kernel code selector (0x08)
; 3. Update data segment registers with kernel data selector (0x10)
; 4. Return to caller
;------------------------------------------------------------------------------
gdt_flush:
    mov eax, [esp+4]        ; Get the pointer to GDT pointer structure from stack
                            ; eax now points to our gdt_ptr structure
    
    lgdt [eax]              ; Load the new GDT
                            ; This instruction tells the CPU where our GDT is located
                            ; [eax] points to: [limit:16][base:32] structure
    
    ;--------------------------------------------------------------------------
    ; Reload Code Segment (CS) using far jump
    ;--------------------------------------------------------------------------
    ; The CS register can only be changed by performing a far jump or call.
    ; We jump to the next instruction (.flush) but with the new code selector.
    ; 0x08 is our kernel code segment selector (index 1 in GDT, ring 0)
    ;--------------------------------------------------------------------------
    jmp 0x08:.flush        ; Far jump to reload CS
                            ; 0x08 = kernel code selector (GDT entry 1)
                            ; .flush = label to jump to (next instruction)

.flush:
    ;--------------------------------------------------------------------------
    ; Update Data Segment Registers
    ;--------------------------------------------------------------------------
    ; Now we need to update all the data segment registers to point to our
    ; new kernel data segment. Unlike CS, these can be loaded directly.
    ; 0x10 is our kernel data segment selector (index 2 in GDT, ring 0)
    ;--------------------------------------------------------------------------
    mov ax, 0x10            ; Load kernel data selector into AX
                            ; 0x10 = kernel data selector (GDT entry 2)
    
    mov ds, ax              ; Data Segment - used for data access
    mov es, ax              ; Extra Segment - used for string operations
    mov fs, ax              ; F Segment - general purpose
    mov gs, ax              ; G Segment - general purpose
    mov ss, ax              ; Stack Segment - used for stack operations
    
    ;--------------------------------------------------------------------------
    ; Return to caller
    ;--------------------------------------------------------------------------
    ; All segment registers now point to our new GDT entries
    ; The CPU is now using our custom GDT instead of any bootloader GDT
    ;--------------------------------------------------------------------------
    ret                     ; Return to C code

;------------------------------------------------------------------------------
; End of GDT Assembly Support
;------------------------------------------------------------------------------
