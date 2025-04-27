SECTIONS
{
    ENTRY(_start)
    .text : {
        *(.text)  /* 代碼段 */
    }

    .rodata : {
        *(.rodata)  /* 只讀數據段 */
    }

    .data : {
        *(.data)  /* 數據段 */
    }

    .bss(NOLOAD): {
        __bss_start = .;
        *(.bss)
        __bss_end = .;
    }

    /* 設置堆棧指針 */
    . = ALIGN(16);
    __stack_top = .;
    . = . + 0x1000;  /* 4KB 的堆棧空間 */
    __stack_bottom = .;
}