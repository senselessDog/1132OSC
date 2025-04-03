.section .text
.global set_exception_vector_table
// save general registers to stack
.macro save_all
    sub sp, sp, 32 * 9
    stp x0, x1, [sp, 16 * 0] // storing the contents of x0 and x1 to the address pointed by the stack pointer sp with an offset of 16 bytes
    stp x2, x3, [sp, 16 * 1] // stp: Store Pair and it's used to store a pair of 64-bit (8-byte) registers to memory
    stp x4, x5, [sp, 16 * 2]
    stp x6, x7, [sp, 16 * 3]
    stp x8, x9, [sp, 16 * 4]
    stp x10, x11, [sp, 16 * 5]
    stp x12, x13, [sp, 16 * 6]
    stp x14, x15, [sp, 16 * 7]
    stp x16, x17, [sp, 16 * 8]
    stp x18, x19, [sp, 16 * 9]
    stp x20, x21, [sp, 16 * 10]
    stp x22, x23, [sp, 16 * 11]
    stp x24, x25, [sp, 16 * 12]
    stp x26, x27, [sp, 16 * 13]
    stp x28, x29, [sp, 16 * 14]
    str x30, [sp, 16 * 15]
    // 保存異常相關寄存器
    mrs x28, spsr_el1
    mrs x29, elr_el1
    stp x28, x29, [sp, 16 * 16]
.endm

// load general registers from stack
.macro load_all
    ldp x28, x29, [sp, 16 * 16]
    msr spsr_el1, x28
    msr elr_el1, x29
    ldp x0, x1, [sp, 16 * 0]
    ldp x2, x3, [sp, 16 * 1]
    ldp x4, x5, [sp, 16 * 2]
    ldp x6, x7, [sp, 16 * 3]
    ldp x8, x9, [sp, 16 * 4]
    ldp x10, x11, [sp, 16 * 5]
    ldp x12, x13, [sp, 16 * 6]
    ldp x14, x15, [sp, 16 * 7]
    ldp x16, x17, [sp, 16 * 8]
    ldp x18, x19, [sp, 16 * 9]
    ldp x20, x21, [sp, 16 * 10]
    ldp x22, x23, [sp, 16 * 11]
    ldp x24, x25, [sp, 16 * 12]
    ldp x26, x27, [sp, 16 * 13]
    ldp x28, x29, [sp, 16 * 14]
    ldr x30, [sp, 16 * 15]
    add sp, sp, 32 * 9
.endm

default_exception_handler:
    eret

/*
    el0_irq_exception_handler:
    save_all                    // 保存上下文，使用之前定義的save_all宏
    bl el0_irq_entry                // 調用C函數處理IRQ
    load_all                    // 恢復上下文
    eret                        // 返回中斷點
*/
sync_lower_el_64_handler:
    // 先確保使用 SP_EL1
    //msr DAIFSet, 0xf
    msr SPSel, #1
    save_all
    bl sync_lower_el_64_entry
    load_all
    //msr DAIFClr, 0xf
    eret

lower_el_irq_exception_handler:
    msr DAIFSet, 0xf
    save_all                    // 保存上下文，使用之前定義的save_all宏
    bl lower_el_irq_entry                // 調用C函數處理IRQ
    load_all                    // 恢復上下文
    msr DAIFClr, 0xf
    eret                        // 返回中斷點

irq_exception_handler:
    //msr DAIFSet, 0xf
    save_all                    // 保存上下文，使用之前定義的save_all宏
    bl irq_entry                // 調用C函數處理IRQ
    load_all                    // 恢復上下文
    //msr DAIFClr, 0xf
    //eret                        // 返回中斷點

.align 11                      // vector table should be aligned to 0x800
.global exception_vector_table
exception_vector_table:
    b default_exception_handler        // Current EL with SP_EL0 - Synchronous
    .align 7                   // entry size is 0x80
    b default_exception_handler        // Current EL with SP_EL0 - IRQ
    .align 7
    b default_exception_handler        // Current EL with SP_EL0 - FIQ
    .align 7
    b default_exception_handler        // Current EL with SP_EL0 - SError
    .align 7
    b default_exception_handler        // Current EL with SP_ELx - Synchronous
    .align 7
    b irq_exception_handler        // Current EL with SP_ELx - IRQ
    .align 7
    b default_exception_handler        // Current EL with SP_ELx - FIQ
    .align 7
    b default_exception_handler        // Current EL with SP_ELx - SError
    .align 7
    b sync_lower_el_64_handler         // Lower EL using AArch64 - Synchronous (need to do)
    .align 7
    b lower_el_irq_exception_handler        // Lower EL using AArch64 - IRQ
    .align 7
    b default_exception_handler        // Lower EL using AArch64 - FIQ
    .align 7
    b default_exception_handler        // Lower EL using AArch64 - SError
    .align 7
    b default_exception_handler        // Lower EL using AArch32 - Synchronous
    .align 7
    b default_exception_handler        // Lower EL using AArch32 - IRQ
    .align 7
    b default_exception_handler        // Lower EL using AArch32 - FIQ
    .align 7
    b default_exception_handler        // Lower EL using AArch32 - SError

set_exception_vector_table:
    adr x0, exception_vector_table
    msr vbar_el1, x0
    ret