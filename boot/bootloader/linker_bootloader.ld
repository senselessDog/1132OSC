/* bootloader_linker.ld */


SECTIONS
{
    ENTRY(_start)
    . = 0x60000; /* Bootloader load address */

    .text.boot : {
        KEEP(*(.text.boot))
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
     _end = .;
    . = ALIGN(16);
    _stack_top = . + 0x10000;
    
}

__bootloader_size = (_end - 0x60000)>>3;