/* boot_kernel.S */

.section ".text.boot"

.global _start
_start:
    /*
     * 暫停非主要核心
     * 只允許 CPU core 0 執行
     */
    mrs  x1, mpidr_el1       /* 讀取多核心處理器 ID 暫存器 */
    and  x1, x1, #0xFF         /* 取出核心 ID (bits 0:1) */
    cmp  x1, #0               /* 比較核心 ID 是否為 0 */
    b.ne halt                 /* 如果核心 ID 不是 0，則跳到 halt 標籤 */
    /* 保存 dtb 地址 */
    mov  x28, x0              /* 將 x0 (dtb 地址) 保存在 x28 中 */
    
    /* switch kernel from EL2 to EL1*/
    bl from_el2_to_el1
    /* set vector table*/
    bl set_exception_vector_table
    /* IRQ enable*/
    bl timer_interrupt_enable
    /*
     * 初始化 BSS 區段
     */
    ldr  x1, =__bss_start     /* 載入 BSS 區段的起始位址 */
    ldr  x2, =__bss_end       /* 載入 BSS 區段的結束位址 */

clear_bss:
    cmp  x1, x2               /* 比較起始位址和結束位址 */
    b.eq stack_setup          /* 如果相等，則跳到 stack_setup 標籤 */
    mov  x3, #0               /* 將 0 載入 x3 暫存器 */
    str  x3, [x1]             /* 將 x3 暫存器的值 (0) 儲存到 x1 暫存器所指向的記憶體位址 */
    add  x1, x1, #8           /* 將 x1 暫存器的值加上 8 (因為是 64 位元架構) */
    b    clear_bss            /* 跳到 clear_bss 標籤 */

    /*
     * 設定堆疊指標
     */
stack_setup:
    ldr  x1, =_stack_top      /* 載入堆疊頂端的位址 */
    mov  sp, x1               /* 將堆疊頂端的位址設定為堆疊指標 */

    /*
     * 跳到 kernel_main 函數
     */
    mov  x0, x28        /* 跳轉到主函數，傳遞 DTB 地址 */
    bl   kernel_main         /* 呼叫 bootloader_main 函數 */

halt:
    wfe                      /* Wait For Event, 進入低功耗模式 */
    b halt                   /* 無限迴圈 */

from_el2_to_el1:
    mov x0, (1 << 31) // EL1 uses aarch64
    msr hcr_el2, x0
    mov x0, 0x3c5 // EL1h (SPSel = 1) with interrupt disabled
    msr spsr_el2, x0
    msr elr_el2, lr
    eret // return to EL1

#define CORE0_TIMER_IRQ_CTRL 0x40000040
timer_interrupt_enable:
    mov x0, 2
    ldr x1, =CORE0_TIMER_IRQ_CTRL
    str w0, [x1] // unmask timer interrupt
    ret

.section ".data"
.global __bss_start
__bss_start:
    .quad __bss_start

.global __bss_end
__bss_end:
    .quad __bss_end

.global _stack_top
_stack_top:
    .quad _stack_top