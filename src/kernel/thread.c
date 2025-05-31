#include "thread.h"
#include "uart.h"
#include "buddy_alloc.h"
#include "syscall.h"
#include <stddef.h>
#include "mmu.h"
#include "strcmp.h"
static thread_t *current_thread = NULL;
thread_t *run_queue = NULL;
static int next_thread_id = 1;

void add_to_run_queue(thread_t *thd) {
    if (!run_queue) {
        run_queue = thd;
        thd->next = thd; // Point to itself for circularity
    } else {
        // Traverse to the end of the queue
        thread_t *current = run_queue;
        while (current->next != run_queue) {
            current = current->next;
        }
        // Add new thread at the end
        thd->next = run_queue;
        current->next = thd;
    }
    thd->state = THREAD_READY;
}
// Helper function to remove a thread from the run queue
void remove_from_run_queue(thread_t *thd) {
    if (!run_queue || !thd) return;

    thread_t *delete_thread = get_current();
    thread_t *prev = NULL;

    thread_t *current = delete_thread;
    // Find the thread and its predecessor
    while (current != run_queue){
        if (current->next == thd){
            prev = current;
            break;
        }
        current = current->next;
    }

    // If thread not found or it's the only one
    if (delete_thread != thd) return; // Not found

    if (thd->next == thd) { // It's the only thread
        run_queue = NULL;
    } else {
        if (prev) {
            prev->next = thd->next;
        }
        // If removing the head node referenced by run_queue
        if (run_queue == thd) {
            // Find the new 'last' element if run_queue points to last, or just use prev->next if run_queue points to head
             if (prev && prev->state != THREAD_DEAD) run_queue = prev; // If run_queue points to last
             else run_queue = thd->next; // If run_queue points to head and we remove it
        }
    }
    //thd->next = NULL; // Clear the next pointer of the removed thread
}
void thread_init(void) {
    current_thread = NULL;
    run_queue = NULL;
    next_thread_id = 1;

    // 創建一個特殊的 kernel 線程
    thread_t *kernel_thread = (thread_t *)dynamic_malloc(sizeof(thread_t));
    kernel_thread->id = 0;  // kernel 線程的 ID 為 0
    kernel_thread->state = THREAD_RUNNING;
    kernel_thread->entry_point = NULL;
    kernel_thread->next = NULL;

    // 保存當前 kernel 的堆疊指針
    uint64_t kernel_sp;
    asm volatile("mov %0, sp" : "=r"(kernel_sp));
    kernel_thread->thread_context.sp = kernel_sp;
    extern void * user_stack;
    kernel_thread->fp = user_stack;

    current_thread = kernel_thread;
    add_to_run_queue(kernel_thread);
    uart_send_string("Thread system initialized. Idle thread created.\r\n");
}
void thread_init_user(void* user_code_start_pa,void* thread_stack_alloc_kva) {
    if (!thread_stack_alloc_kva){
        uart_send_string("Error: [thread_init_user] thread_stack_alloc_kva have error\r\n");
    }
    current_thread = NULL;
    run_queue = NULL;
    next_thread_id = 1;

    // 創建一個特殊的 kernel 線程
    thread_t *first_thread = (thread_t *)dynamic_malloc(sizeof(thread_t));
    first_thread->id = 0;  // kernel 線程的 ID 為 0
    first_thread->state = THREAD_RUNNING;
    first_thread->entry_point = NULL;
    first_thread->next = NULL;
    first_thread->user_code_start_pa=user_code_start_pa;
    first_thread->thread_stack_alloc_kva=thread_stack_alloc_kva;
    first_thread->vma_list=NULL;
    // 保存當前 kernel 的堆疊指針
    // extern void * user_stack;
    // first_thread->thread_context.sp = user_stack;
    // first_thread->thread_context.fp = user_stack;
    //allocate first thread tpidr
    asm volatile("msr tpidr_el1, %0" : : "r"(first_thread));


    add_to_run_queue(first_thread);
    setupInit_vma(first_thread);
    //signal
    for (int i = 0; i < NSIG; i++) {
        first_thread->sighand[i] = SIG_DFL;
    }
    first_thread->is_handling_signal=0;
    first_thread->sigpending=0;
    // 初始化 VFS 相關成員
    if (rootfs && rootfs->root) {
        first_thread->cwd = rootfs->root; // 新任務的 CWD 預設為根檔案系統的根
        rootfs->root->ref_count++;      // 增加引用計數

        first_thread->root_dir = rootfs->root; // 任務的根目錄也預設為 VFS 的根
        rootfs->root->ref_count++;          // 增加引用計數
    } else {
        // 錯誤處理：rootfs 尚未初始化
        first_thread->cwd = NULL;
        first_thread->root_dir = NULL;
        uart_send_string("CRITICAL: [thread_create] rootfs not initialized for new thread VFS members!\r\n");
    }

    for (int i = 0; i < MAX_PROCESS_OPEN_FILES; ++i) {
        first_thread->fd_table[i] = NULL; // 清空檔案描述符表
    }
    change_tpidr(first_thread);
    uart_send_string("First user initialized and Created.\r\n");
}
thread_t *thread_create(void (*entry_point)(void), trap_frame_t *frame) {
    // Allocate memory for new thread
    thread_t *new_thread = (thread_t *)dynamic_malloc(sizeof(thread_t));
    if (!new_thread) {
        uart_send_string("Failed to allocate memory for new thread\r\n");
        return NULL;
    }

    // Initialize thread structure
    new_thread->id = next_thread_id++;
    new_thread->state = THREAD_READY;
    new_thread->entry_point = entry_point;
    new_thread->next = NULL;
    new_thread->vma_list=NULL;
    setupInit_vma(new_thread);
    // if (entry_point==NULL) { //fork
    //     // fp will be overwritten in switch_to
    //     new_thread->thread_context.fp = (uint64_t)stack_top;
    //     uint32_t kernel_sp; 
    //     asm volatile("mov %0, sp" : "=r"(kernel_sp));  
    //     // sp & lr will be overwritten in switch_to
    //     new_thread->thread_context.sp = (uint64_t)kernel_sp;
    //     new_thread->thread_context.lr = (uint64_t)frame->elr_el1;
    //     uart_send_string("new thread lr: ");
    //     uart_send_hex(new_thread->thread_context.lr);
    //     uart_send_string("\r\n");
    //     //之後sp、lr也會再switch_to中進行修改
    //     // thread_inherit_save(frame, new_thread->thread_context);
    // }else{
    //     new_thread->thread_context.fp = (uint64_t)stack_top;
    //     new_thread->thread_context.sp = (uint64_t)stack_top;
    //     new_thread->thread_context.lr = (uint64_t)entry_point;
    // }
    
    // Add to thread list
    add_to_run_queue(new_thread);
    // //new_thread->context.sp = thread_sp-7*16;
    // uart_send_string("new thread fp: ");
    // uart_send_hex(new_thread->thread_context.fp);
    // uart_send_string("\r\n");
    // // thread_create_save(new_thread->thread_context.sp, new_thread->thread_context.fp, new_thread->thread_context.lr);
    // uart_send_string("new thread sp: ");
    // uart_send_hex(new_thread->thread_context.sp);
    // uart_send_string("\r\n");
    //SIGnal
    for (int i = 0; i < NSIG; i++) {
        new_thread->sighand[i] = SIG_DFL;
    }
    // 初始化 VFS 相關成員
    if (rootfs && rootfs->root) {
        new_thread->cwd = rootfs->root; // 新任務的 CWD 預設為根檔案系統的根
        rootfs->root->ref_count++;      // 增加引用計數

        new_thread->root_dir = rootfs->root; // 任務的根目錄也預設為 VFS 的根
        rootfs->root->ref_count++;          // 增加引用計數
    } else {
        // 錯誤處理：rootfs 尚未初始化
        new_thread->cwd = NULL;
        new_thread->root_dir = NULL;
        uart_send_string("CRITICAL: [thread_create] rootfs not initialized for new thread VFS members!\r\n");
    }

    for (int i = 0; i < MAX_PROCESS_OPEN_FILES; ++i) {
        new_thread->fd_table[i] = NULL; // 清空檔案描述符表
    }
    return new_thread;
}

void thread_exit(void) {
    thread_t * delete_thread = get_current(); // Ensure current_thread is up-to-date
    if (delete_thread) {
        uart_send_string("Thread ID: ");
        uart_send_int(delete_thread->id);
        uart_send_string(" exiting.\r\n");
        delete_thread->state = THREAD_DEAD;
        remove_from_run_queue(delete_thread); // Remove from scheduling
        schedule(1); // Switch to another thread
        // Should not return here
        uart_send_string("Error: Exited thread returned!\r\n");
        while(1);
    }
}



void schedule(int is_exit) {
    if (!run_queue) {
        return;
    }

    // Find next ready thread
    thread_t *next = current_thread->next;
    while (next) {
        if (next->state == THREAD_READY) {
            break;
        }
        else if (next == current_thread && next->state == THREAD_RUNNING) {
            break;
        }
        else if (next == current_thread && next->state == THREAD_DEAD) {
            uart_send_string("[schedule] current thread is dead, use idle thread\r\n");
            idle();
            return;
        }
        next = next->next;
    }

    // If no ready thread found, use idle thread
    if (!next) {
        idle();
        return;
    }
    

    // Switch context
    thread_t *prev = current_thread;
    if (is_exit) {
        prev->state = THREAD_DEAD;
    } else {
        prev->state = THREAD_READY;
    }
    current_thread = next;
    current_thread->state = THREAD_RUNNING;
    while(prev->id==current_thread->id){
        return;
    };

    //update prev thread sp
    // uint64_t prev_thread_sp;
    // asm volatile("mov %0, sp" : "=r"(prev_thread_sp));
    // prev->sp = prev_thread_sp-7*16;
    //Don't habe to deal with current thread sp
    //current thread sp is already updated in switch.S
    uart_send_string("[schedule] switch to thread: ");
    uart_send_int(current_thread->id);
    uart_send_string("\r\n");
    uart_send_string("current sp: ");
    uart_send_hex(current_thread->thread_context.sp);
    uart_send_string("\r\n");
    // uart_send_string("prev sp: ");
    // uart_send_hex(prev->thread_context.sp);
    // uart_send_string("\r\n");
    asm volatile("mov %0, sp" : "=r"(prev->thread_context.sp));
    asm volatile("mov %0, fp" : "=r"(prev->thread_context.fp));
    asm volatile("mov %0, lr" : "=r"(prev->thread_context.lr));
    uart_send_string("[schedule] prev sp: ");
    uart_send_hex(prev->thread_context.sp);
    uart_send_string("\r\n");
    uart_send_string("[schedule] prev fp: ");
    uart_send_hex(prev->thread_context.fp);
    uart_send_string("\r\n");
    uart_send_string("[schedule] prev lr: ");
    uart_send_hex(prev->thread_context.lr);
    uart_send_string("\r\n");
    // Call assembly function to switch context
    extern void switch_to(thread_context_block_t *prev_context, thread_context_block_t *current_context, thread_t *current_thread);
    switch_to(&prev->thread_context, &current_thread->thread_context, current_thread);
}
void fork_schedule(trap_frame_t *frame, thread_t* child_thread) {
    //先不使用current_thread
    uart_send_string("[fork_schedule] frame_address: ");
    uart_send_hex((uint64_t)frame);
    uart_send_string("\r\n");
    if (!run_queue) {
        return;
    }
    current_thread = get_current();
    // Find next ready thread
    thread_t *next = current_thread->next;
    while (next) {
        if (next == child_thread) {
            break;
        }
        else if (next == current_thread) {
            uart_send_string("Error: [fork_schedule] Can't find child thread");
            break;
        }
        uart_send_string("[fork_schedule] next_thread: ");
        uart_send_int(next->id);
        uart_send_string("\r\n");
        uart_send_string("[fork_schedule] next_thread state: ");
        uart_send_int(next->state);
        uart_send_string("\r\n");
        next = next->next;
    }

    // If no ready thread found, use idle thread
    if (!next) {
        idle();
        return;
    }
    

    // Switch context
    thread_t *prev = current_thread;
    prev->state = THREAD_READY;
    current_thread = next;
    current_thread->state = THREAD_RUNNING;
    
    while(prev->id==current_thread->id){
        return;
    };
    uint64_t kernel_sp, kernel_lr;
    asm volatile("mov %0, sp" : "=r"(kernel_sp));
    current_thread->thread_context.sp = kernel_sp;
    asm volatile("mov %0, lr" : "=r"(kernel_lr));
    current_thread->thread_context.lr = kernel_lr;
    //update prev thread sp
    // uint64_t prev_thread_sp;
    // asm volatile("mov %0, sp" : "=r"(prev_thread_sp));
    // prev->sp = prev_thread_sp-7*16;
    //Don't habe to deal with current thread sp
    //current thread sp is already updated in switch.S
    uart_send_string("[fork_schedule] switch to thread: ");
    uart_send_int(current_thread->id);
    uart_send_string("\r\n");
    // uart_send_string("current sp: ");
    // uart_send_hex(current_thread->thread_context.sp);
    // uart_send_string("\r\n");
    // uart_send_string("prev sp: ");
    // uart_send_hex(prev->thread_context.sp);
    // uart_send_string("\r\n");
    // asm volatile("mov %0, sp" : "=r"(prev->thread_context.sp));
    // asm volatile("mov %0, fp" : "=r"(prev->thread_context.fp));
    // asm volatile("mov %0, lr" : "=r"(prev->thread_context.lr));
    // uart_send_string("[schedule] prev sp: ");
    // uart_send_hex(prev->thread_context.sp);
    // uart_send_string("\r\n");
    // uart_send_string("[schedule] prev fp: ");
    // uart_send_hex(prev->thread_context.fp);
    // uart_send_string("\r\n");
    // uart_send_string("[schedule] prev lr: ");
    // uart_send_hex(prev->thread_context.lr);
    // uart_send_string("\r\n");
    // uart_send_string("[schedule] current sp: ");
    // uart_send_hex(current_thread->thread_context.sp);
    // uart_send_string("\r\n");
    // uart_send_string("[schedule] current fp: ");
    // uart_send_hex(current_thread->thread_context.fp);
    // uart_send_string("\r\n");
    // uart_send_string("[schedule] current lr: ");
    // uart_send_hex(current_thread->thread_context.lr);
    // uart_send_string("\r\n");
    // uart_send_string("sp0: ");
    uint64_t sp0,elr_el1,spsr_el1;
    // uart_send_string("[fork_schedule]sp_el0: ");
    // asm volatile("mrs %0, sp_el0" : "=r"(sp0));
    // uart_send_hex(sp0);
    // uart_send_string("\r\n");
    // uart_send_string("[fork_schedule]elr_el1: ");
    // asm volatile("mrs %0, elr_el1" : "=r"(elr_el1));
    // uart_send_hex(elr_el1);
    // uart_send_string("\r\n");
    // uart_send_string("[fork_schedule]spsr_el1: ");
    // asm volatile("mrs %0, spsr_el1" : "=r"(spsr_el1));
    // uart_send_hex(spsr_el1);
    // uart_send_string("\r\n");
    // uart_send_string("prev user fp: ");
    // uart_send_hex(prev->fp);
    // uart_send_string("\r\n");
    // uart_send_string("next user fp: ");
    // uart_send_hex(next->fp);
    // uart_send_string("\r\n");
    // 1. 為新執行緒分配 PGD
    uint64_t child_pgd_kva = (uint64_t)dynamic_malloc(PAGE_SIZE);
    if (!child_pgd_kva) {
        uart_send_string("Error:[fork_schedule] Failed to allocate PGD (KVA)!\r\n");
        // 可能需要釋放 new_thread_kva
        return;
    }
    uint64_t child_pgd_pa = KVA_TO_PHYS(child_pgd_kva);
    memset((void*)child_pgd_kva, 0, PAGE_SIZE);
    uart_send_string("[fork_schedule] Child PGD KVA: 0x"); uart_send_hex(child_pgd_kva);
    uart_send_string(", Child PGD PA: 0x"); uart_send_hex(child_pgd_pa); uart_send_string("\r\n");
    //2. map user code
    // if (mappages(child_pgd_pa, USER_CODE_VA, (uint64_t)user_space_size, (uint64_t)prev->user_code_start_pa, USER_CODE_ATTR) != 0) {
    //     uart_send_string("Error:[fork_schedule] Failed to map user code!\r\n");
    //     // Potentially free PGD and other allocated tables here
    //     return;
    // }
    // uart_send_string("[fork_schedule] User code mapped: VA 0x0 to PA 0x"); uart_send_hex((uint64_t)prev->user_code_start_pa);
    // uart_send_string(" (size 0x"); uart_send_hex(user_space_size); uart_send_string(")\r\n");

    
    // 3. Allocate and map user stack (4 pages = 16KB)
    uint64_t child_stack_kva = (uint64_t)dynamic_malloc(USER_STACK_SIZE);
    if (!child_stack_kva) {
        uart_send_string("Error:[fork_schedule] Failed to allocate stack backing KVA!\r\n");
        return;
    }
    uint64_t child_stack_pa = KVA_TO_PHYS(child_stack_kva);
    uart_send_string("[fork_schedule] Child stack backing KVA: 0x"); uart_send_hex(child_stack_kva);
    uart_send_string("\r\n[fork_schedule]Child stack backing PA: 0x"); uart_send_hex(child_stack_pa); uart_send_string("\r\n");

    // if (mappages(child_pgd_pa, USER_STACK_BOTTOM_VA, USER_STACK_SIZE, child_stack_pa, USER_DATA_STACK_ATTR) != 0) {
    //     uart_send_string("Error: [fork_schedule] Failed to map child stack!\r\n");
    //     return;
    // }
    uart_send_string("[fork_schedule] Child stack mapped: VA 0x"); uart_send_hex(USER_STACK_BOTTOM_VA);
    uart_send_string(" - VA 0x"); uart_send_hex(USER_STACK_TOP_VA -1); uart_send_string("\r\n");
    
    //store new_thread information
    current_thread->user_code_start_pa=prev->user_code_start_pa;
    current_thread->thread_stack_alloc_kva=(void*)child_stack_kva;
    //maybe don't need
    void *stack_top = &current_thread->thread_stack_alloc_kva[USER_STACK_SIZE];
    current_thread->fp = stack_top;
    //stack memory copy
    memcpy((void *)(current_thread->thread_stack_alloc_kva), (void *)(prev->thread_stack_alloc_kva), USER_STACK_SIZE);
    // asm volatile("msr sp_el0,%0 " : :"r"(next->thread_context.fp-((uint64_t)prev->fp-frame->sp_el0)));
    //update frame->sp_el0 to child thread stack
    // frame->sp_el0=(uint64_t)USER_STACK_TOP_VA-((uint64_t)USER_STACK_TOP_VA-frame->sp_el0);
    // uart_send_string("prev user fp: ");
    // uart_send_hex(frame->x29);
    // uart_send_string("\r\n");
    // uart_send_string("prev user sp: ");
    // uart_send_hex(frame->sp_el0);
    // uart_send_string("\r\n");
    // asm volatile("msr sp_el0,%0 " : :"r"(current_thread->thread_context.fp-(frame->x29-frame->sp_el0)));
    //asm volatile("msr elr_el1,%0 " : :"r"(frame->elr_el1));
    //asm volatile("msr spsr_el1,%0 " : :"r"(0));

    // uart_send_string("sp0: ");
    // asm volatile("mrs %0, sp_el0" : "=r"(sp0));
    // uart_send_hex(sp0);
    // uart_send_string("\r\n");
    // uart_send_string("elr_el1: ");
    // asm volatile("mrs %0, elr_el1" : "=r"(elr_el1));
    // uart_send_hex(elr_el1);
    // uart_send_string("\r\n");
    // uart_send_string("spsr_el1: ");
    // asm volatile("mrs %0, spsr_el1" : "=r"(spsr_el1));
    // uart_send_hex(spsr_el1);
    // uart_send_string("\r\n");
    // Call assembly function to switch context
    // extern void svc_switch_to(thread_context_block_t *prev_context, thread_context_block_t *current_context, thread_t *current_thread);
    // svc_switch_to(&prev->thread_context, &current_thread->thread_context, current_thread);
    // uart_send_string("[schedule] prev sp: ");
    // uart_send_hex(prev->thread_context.sp);
    // uart_send_string("\r\n");
    // uart_send_string("[schedule] prev fp: ");
    // uart_send_hex(prev->thread_context.fp);
    // uart_send_string("\r\n");
    // uart_send_string("[schedule] prev lr: ");
    // uart_send_hex(prev->thread_context.lr);
    // uart_send_string("\r\n");
    // uart_send_string("[schedule] current sp: ");
    // uart_send_hex(current_thread->thread_context.sp);
    // uart_send_string("\r\n");
    // uart_send_string("[schedule] current fp: ");
    // uart_send_hex(current_thread->thread_context.fp);
    // uart_send_string("\r\n");
    // uart_send_string("[schedule] current lr: ");
    // uart_send_hex(current_thread->thread_context.lr);
    // uart_send_string("\r\n");
    //adjust to child frame
    frame->tpidr_el1=(uint64_t)current_thread;
    frame->ttbr0_el1=child_pgd_pa;
    //store tpidr_el1
    change_tpidr(current_thread);


}

void idle(void) {
    while (1) {
        kill_zombie_thread();
        schedule(0);
    }
}
void kill_zombie_thread(void) {
    // Check for dead threads and clean them up
    if (run_queue==NULL) return;
    thread_t *current = run_queue->next;
    thread_t *prev = run_queue;
    while (current!=run_queue) {
        if (current->state == THREAD_DEAD) {
            if (prev) {
                prev->next = current->next;
            } else {
                run_queue = current->next;
            }
            thread_t *to_free = current;
            current = current->next;
            dynamic_free(to_free);
        } else {
            prev = current;
            current = current->next;
        }
    }
}
void thread_test() {
    for(int i = 0; i < 10; ++i) {
        thread_t *current = get_current();
        uart_send_string("Thread id: ");
        uart_send_int(current->id);
        uart_send_string(" ");
        uart_send_int(i);
        uart_send_string("\r\n");
        uint64_t thread_sp;
        asm volatile("mov %0, sp" : "=r"(thread_sp));
        uart_send_string("thread sp: ");
        uart_send_int(thread_sp);
        uart_send_string("\r\n");
        
        // Simple delay
        for(int j = 0; j < 1000000; j++) {
            asm volatile("nop");
        }
        
        schedule(0);
    }
    thread_exit();
}

void test_eret(void) {
    uint64_t elr_el1,sp_el0;
    uart_send_string("elr_el1: \r\n");
    asm volatile("mrs %0, elr_el1":: "r"(elr_el1));
    uart_send_hex(elr_el1);
    uart_send_string("\r\n");
    uart_send_string("sp_el0: \r\n");
    asm volatile("mrs %0, sp_el0":: "r"(sp_el0));
    uart_send_hex(sp_el0);
    uart_send_string("\r\n");
    return;
}

void save_trap_frame(trap_frame_t *frame, thread_t *thread) {
    thread->trap_frame.x0=frame->x0;
    thread->trap_frame.x1=frame->x1;
    thread->trap_frame.x2=frame->x2;
    thread->trap_frame.x3=frame->x3;
    thread->trap_frame.x4=frame->x4;
    thread->trap_frame.x5=frame->x5;
    thread->trap_frame.x6=frame->x6;
    thread->trap_frame.x7=frame->x7;
    thread->trap_frame.x8=frame->x8;
    thread->trap_frame.x9=frame->x9;
    thread->trap_frame.x10=frame->x10;
    thread->trap_frame.x11=frame->x11;
    thread->trap_frame.x12=frame->x12;
    thread->trap_frame.x13=frame->x13;
    thread->trap_frame.x14=frame->x14;
    thread->trap_frame.x15=frame->x15;
    thread->trap_frame.x16=frame->x16;
    thread->trap_frame.x17=frame->x17;
    thread->trap_frame.x18=frame->x18;
    thread->trap_frame.x19=frame->x19;
    thread->trap_frame.x20=frame->x20;
    thread->trap_frame.x21=frame->x21;
    thread->trap_frame.x22=frame->x22;
    thread->trap_frame.x23=frame->x23;
    thread->trap_frame.x24=frame->x24;
    thread->trap_frame.x25=frame->x25;
    thread->trap_frame.x26=frame->x26;
    thread->trap_frame.x27=frame->x27;
    thread->trap_frame.x28=frame->x28;
    thread->trap_frame.x29=frame->x29;
    thread->trap_frame.x30=frame->x30;
    thread->trap_frame.sp_el0=frame->sp_el0;
    thread->trap_frame.elr_el1=frame->elr_el1;
    thread->trap_frame.spsr_el1=frame->spsr_el1;
    thread->trap_frame.tpidr_el1=frame->tpidr_el1;
    thread->trap_frame.ttbr0_el1=frame->ttbr0_el1;
}
void restore_trap_frame(trap_frame_t *frame, thread_t *thread) {
    frame->x0=thread->trap_frame.x0;
    frame->x1=thread->trap_frame.x1;
    frame->x2=thread->trap_frame.x2;
    frame->x3=thread->trap_frame.x3;
    frame->x4=thread->trap_frame.x4;
    frame->x5=thread->trap_frame.x5;
    frame->x6=thread->trap_frame.x6;
    frame->x7=thread->trap_frame.x7;
    frame->x8=thread->trap_frame.x8;
    frame->x9=thread->trap_frame.x9;
    frame->x10=thread->trap_frame.x10;
    frame->x11=thread->trap_frame.x11;
    frame->x12=thread->trap_frame.x12;
    frame->x13=thread->trap_frame.x13;
    frame->x14=thread->trap_frame.x14;
    frame->x15=thread->trap_frame.x15;
    frame->x16=thread->trap_frame.x16;
    frame->x17=thread->trap_frame.x17;
    frame->x18=thread->trap_frame.x18;
    frame->x19=thread->trap_frame.x19;
    frame->x20=thread->trap_frame.x20;
    frame->x21=thread->trap_frame.x21;
    frame->x22=thread->trap_frame.x22;
    frame->x23=thread->trap_frame.x23;
    frame->x24=thread->trap_frame.x24;
    frame->x25=thread->trap_frame.x25;
    frame->x26=thread->trap_frame.x26;
    frame->x27=thread->trap_frame.x27;
    frame->x28=thread->trap_frame.x28;
    frame->x29=thread->trap_frame.x29;
    frame->x30=thread->trap_frame.x30;
    frame->sp_el0=thread->trap_frame.sp_el0;
    frame->elr_el1=thread->trap_frame.elr_el1;
    frame->spsr_el1=thread->trap_frame.spsr_el1;
    frame->tpidr_el1=thread->trap_frame.tpidr_el1;
    frame->ttbr0_el1=thread->trap_frame.ttbr0_el1;
}

thread_t * find_thread_by_pid(int pid){
    thread_t *current_thread = get_current();
    // uart_send_string("[user_thread_schedule] current_thread: ");
    // uart_send_int(current_thread->id);
    // uart_send_string("\r\n");
    // Find next ready thread
    thread_t *next = current_thread->next;
    // //check next thread
    // uart_send_string("[user_thread_schedule] next_thread: ");
    // uart_send_int(next->id);
    // uart_send_string("\r\n");
    // uart_send_string("[user_thread_schedule] next_thread state: ");
    // uart_send_int(next->state);
    // uart_send_string("\r\n");
    while (next) {
        if (next->id == pid){
            break;
        }else if (next==current_thread){
            uart_send_string("Error: [find_thread_by_pid] can't find target thread");
            return 0;
        }
        next = next->next;
    }
    return next;
}
void setupInit_vma(thread_t* thread){
        //for new_vma overlap
    //user 
    // uart_send_string("[SetupInit] vma address");
    // uart_send_hex((uint64_t)thread->vma_list);
    // uart_send_string("\r\n");
    struct vm_area_struct *code_vma = (struct vm_area_struct *)dynamic_malloc(sizeof(struct vm_area_struct));
    // uart_send_string("[SetupInit] Allocated new VMA struct at: 0x"); uart_send_hex((uint64_t)code_vma); uart_send_string("\r\n");
    code_vma->vm_area_tag = VMA_AREA_CODE;
    code_vma->vm_start = USER_CODE_VA;
    code_vma->vm_end = USER_CODE_VA+user_space_size;
    code_vma->vm_size = user_space_size;
    code_vma->vm_prot = PROT_READ | PROT_EXEC;
    code_vma->vm_flags = MAP_ANONYMOUS; // 標記為匿名（因為不是透過 mmap file 來的）
    if (thread->vma_list) {
        uart_send_string("[setupInit_vma] first vma\r\n");
        code_vma->vm_next = thread->vma_list;
    }else{
        code_vma->vm_next=NULL; //避免亂碼
    }
    thread->vma_list = code_vma;
    //user stack
    struct vm_area_struct *stack_vma = (struct vm_area_struct *)dynamic_malloc(sizeof(struct vm_area_struct));
    stack_vma->vm_area_tag = VMA_AREA_STACK;
    stack_vma->vm_start = USER_STACK_BOTTOM_VA;
    stack_vma->vm_end = USER_STACK_TOP_VA;
    stack_vma->vm_size = USER_STACK_SIZE;
    stack_vma->vm_prot = PROT_READ | PROT_WRITE;
    stack_vma->vm_flags = MAP_ANONYMOUS;
    if (thread->vma_list) {
        stack_vma->vm_next = thread->vma_list;
    }
    thread->vma_list = stack_vma;
}