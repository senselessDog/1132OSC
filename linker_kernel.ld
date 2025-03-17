/* kernel_linker.ld */
ENTRY(_start)

SECTIONS
{
    . = 0x80000; /* Kernel load address */
    /* Not sure */
    .text.boot : {
        KEEP(*(text.boot))
    }

    .text : {
        *(.text)
    }

    .rodata : { *(.rodata) }

    .data : {
        *(.data)
    }
    . = ALIGN(8);
    .bss (NOLOAD): {
        __bss_start = .;
        *(.bss)
        __bss_end = .;
    }

    . = ALIGN(8);
    .heap (NOLOAD) : {
        __heap_start = .;
        . += 0x10000; /* 1 MB heap size */
        __heap_end = .;
    }

    . = ALIGN(8);
    _stack_top = . + 0x10000;
}