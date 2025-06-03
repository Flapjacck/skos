/* Kernel entry point */
ENTRY(_start)

/* Define the program headers we want in our final kernel executable */
PHDRS
{
    text    PT_LOAD    FLAGS((1 << 0) | (1 << 2)); /* Execute + Read */
    rodata  PT_LOAD    FLAGS((1 << 2));            /* Read only */
    data    PT_LOAD    FLAGS((1 << 1) | (1 << 2)); /* Write + Read */
}

SECTIONS
{
    /* Kernel starts at 1M mark */
    . = 1M;

    .text ALIGN(4K) : {
        *(.multiboot)  /* Multiboot header first */
        *(.text)       /* All text sections from all files */
    } :text

    .rodata ALIGN(4K) : {
        *(.rodata)     /* Read-only data sections */
    } :rodata

    .data ALIGN(4K) : {
        *(.data)       /* Data segments */
    } :data

    .bss ALIGN(4K) : {
        *(COMMON)      /* Common symbols */
        *(.bss)        /* BSS segment */
    } :data

    /* Discard unused sections */
    /DISCARD/ : {
        *(.eh_frame)
        *(.comment)
    }
}