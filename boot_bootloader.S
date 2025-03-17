/* boot_bootloader.S */

.section ".text.boot"

.global _start
_start:

    /*
     * 暫停非主要核心
     * 只允許 CPU core 0 執行
     */
    mrs  x0, mpidr_el1       /* 讀取多核心處理器 ID 暫存器 */
    and  x0, x0, #0xFF         /* 取出核心 ID (bits 0:1) */
    cmp  x0, #0               /* 比較核心 ID 是否為 0 */
    b.ne halt                 /* 如果核心 ID 不是 0，則跳到 halt 標籤 */

    
    /* 重定位 bootloader */
    ldr  x1, =0x80000         /* 當前加載地址 */
    ldr  x2, =0x60000         /* 目標地址 */
    ldr  w3, =__bootloader_size /* 加載 bootloader 大小 (以 8 字節為單位) */

relocate:
    /* 執行重定位過程 */
    ldr  x4, [x1], #8         /* 從源地址加載 8 字節，然後增加源地址 */
    str  x4, [x2], #8         /* 存儲 8 字節到目標地址，然後增加目標地址 */
    sub  w3, w3, #1           /* 減少計數 */
    cbnz w3, relocate         /* 如果計數不為零，繼續重定位 */
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
    bl   bootloader_main-0x20000          /* 呼叫 bootloader_main 函數 */

halt:
    wfe                      /* Wait For Event, 進入低功耗模式 */
    b halt                   /* 無限迴圈 */

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