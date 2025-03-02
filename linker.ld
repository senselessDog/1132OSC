/* linker.ld */
ENTRY(_start)

SECTIONS
{
    . = 0x80000;

    .text.boot : {
        *(.text.boot)
    }

    .text : {
        *(.text)
    }

    .data : {
        *(.data)
    }

    .bss : {
        __bss_start = .;
        *(.bss)
        __bss_end = .;
    }

    . = ALIGN(8);
    _stack_top = . + 0x10000; /* Define the stack top address */
}