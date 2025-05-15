/* kernel_linker.ld */
ENTRY(_start)

SECTIONS
{
    /* * 修改核心的虛擬位址基底為高位址 
     * 核心程式碼和資料的虛擬位址將從 0xffff000000000000 + 0x80000 開始
     */
    . = 0xffff000000000000;  /*Kernel virtual space base */
    . += 0x80000;  /*Kernel load address */
    
    _kernel_virtual_start = .; /* Define a symbol for kernel's virtual start address */
    /* Not sure */
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

    . = ALIGN(16);
    .heap (NOLOAD) : {
        __heap_start = .;
        . += 0x1200000;
        __heap_end = .;
    }

    . = ALIGN(16);
    _stack_top = . + 0x10000;
    _kernel_end = .+0x10000;       /* define kernel end location*/
    _kernel_virtual_end = _kernel_end; /* Define a symbol for kernel's virtual end address */
}