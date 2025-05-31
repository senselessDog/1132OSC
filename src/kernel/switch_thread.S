.global thread_create_save
thread_create_save:
    // sub x0, x0, #16 * 7  // 為保存的寄存器預留空間
    stp xzr, xzr, [x0, 16 * 0]
    stp xzr, xzr, [x0, 16 * 1]
    stp xzr, xzr, [x0, 16 * 2]
    stp xzr, xzr, [x0, 16 * 3]
    stp xzr, xzr, [x0, 16 * 4]
    stp x1, x2, [x0, 16 * 5]
    mov x9, x0
    str x9, [x0, 16 * 6]
    ret
.global thread_inherit_save
thread_inherit_save:
    //load from parent's tcb 
    ldp x18, x19, [x0, 16 * 9]
    ldp x20, x21, [x0, 16 * 10]
    ldp x22, x23, [x0, 16 * 11]
    ldp x24, x25, [x0, 16 * 12]
    ldp x26, x27, [x0, 16 * 13]
    ldp x28, x29, [x0, 16 * 14]
    //store to child's tcb 
    stp x19, x20, [x1, 16 * 0]
    stp x21, x22, [x1, 16 * 1]
    stp x23, x24, [x1, 16 * 2]
    stp x25, x26, [x1, 16 * 3]
    stp x27, x28, [x1, 16 * 4]
    ret
.global switch_to
switch_to:
    stp x19, x20, [x0, 16 * 0]
    stp x21, x22, [x0, 16 * 1]
    stp x23, x24, [x0, 16 * 2]
    stp x25, x26, [x0, 16 * 3]
    stp x27, x28, [x0, 16 * 4]
    stp fp, lr, [x0, 16 * 5]
    mov x9, sp
    str x9, [x0, 16 * 6]

    ldp x19, x20, [x1, 16 * 0]
    ldp x21, x22, [x1, 16 * 1]
    ldp x23, x24, [x1, 16 * 2]
    ldp x25, x26, [x1, 16 * 3]
    ldp x27, x28, [x1, 16 * 4]
    ldp fp, lr, [x1, 16 * 5]
    ldr x9, [x1, 16 * 6]
    mov sp,  x9
    msr tpidr_el1, x2
    ret
.global svc_switch_to
svc_switch_to:
    stp x19, x20, [x0, 16 * 0]
    stp x21, x22, [x0, 16 * 1]
    stp x23, x24, [x0, 16 * 2]
    stp x25, x26, [x0, 16 * 3]
    stp x27, x28, [x0, 16 * 4]
    stp fp, lr, [x0, 16 * 5]
    mov x9, sp
    str x9, [x0, 16 * 6]

    ldp x19, x20, [x1, 16 * 0]
    ldp x21, x22, [x1, 16 * 1]
    ldp x23, x24, [x1, 16 * 2]
    ldp x25, x26, [x1, 16 * 3]
    ldp x27, x28, [x1, 16 * 4]
    //ldp fp, lr, [x1, 16 * 5]
    //ldr x9, [x1, 16 * 6]
    //mov sp,  x9
    msr tpidr_el1, x2
    ret
.global process_switch_to
process_switch_to:
    stp x19, x20, [x0, 16 * 0]
    stp x21, x22, [x0, 16 * 1]
    stp x23, x24, [x0, 16 * 2]
    stp x25, x26, [x0, 16 * 3]
    stp x27, x28, [x0, 16 * 4]
    stp fp, lr, [x0, 16 * 5]
    mov x9, sp
    str x9, [x0, 16 * 6]

    ldp x19, x20, [x1, 16 * 0]
    ldp x21, x22, [x1, 16 * 1]
    ldp x23, x24, [x1, 16 * 2]
    ldp x25, x26, [x1, 16 * 3]
    ldp x27, x28, [x1, 16 * 4]
    //ldp fp, lr, [x1, 16 * 5]
    //ldr x9, [x1, 16 * 6]
    //mov sp,  x9
    msr tpidr_el1, x2
    eret
.global get_current
get_current:
    mrs x0, tpidr_el1
    ret 
.global change_tpidr
change_tpidr:
    msr tpidr_el1, x0
    ret
.global get_current_ttbr0_el1
get_current_ttbr0_el1:
    mrs x0, ttbr0_el1
    ret 
.global change_ttbr0_el1
change_ttbr0_el1:
    msr ttbr0_el1, x0
    ret


.global sigreturn_trampoline_entry
sigreturn_trampoline_entry:
    mov x8, #18
    svc 0
    ret