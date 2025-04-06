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
    . = ALIGN(16);
    .bss (NOLOAD): {
        __bss_start = .;
        *(.bss)
        __bss_end = .;
    }

    . = ALIGN(16);
    .heap (NOLOAD) : {
        __heap_start = .;
        . += 0x10000; /* 1 MB heap size */
        __heap_end = .;
    }

    . = ALIGN(16);
    _stack_top = . + 0x10000;
}