/* bootloader_linker.ld */
ENTRY(_start)

SECTIONS
{
    . = 0x60000; /* Bootloader load address */

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
    _stack_top = . + 0x10000;
}