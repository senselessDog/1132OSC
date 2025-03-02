/* linker.ld */
ENTRY(_start)

SECTIONS
{
    . = 0x80000;

    .text.boot : {
        KEEP(*(text.boot))
    }

    .text : {
        *(.text)
    }

    .data : {
        *(.data)
    }
    . = ALIGN(8);  /* 確保BSS段對齊到8字節邊界 */
    .bss : {
        __bss_start = .;
        *(.bss)
        __bss_end = .;
    }

    . = ALIGN(8);
    _stack_top = . + 0x10000; /* Define the stack top address */
}