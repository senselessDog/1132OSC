/* linker.ld */
ENTRY(_start)

SECTIONS
{
    /*在許多 ARM 系統中，0x80000 是一個常見的啟動地址。這個地址通常是由系統的啟動代碼（bootloader）設置的。
    當系統啟動時，bootloader 會將內核映像加載到這個地址，然後跳轉到這個地址開始執行。*/
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