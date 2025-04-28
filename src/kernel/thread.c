#include "thread.h"
#include "uart.h"
#include "buddy_alloc.h"
#include "syscall.h"
#include <stddef.h>

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
void thread_init_user(void) {
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
    extern void * user_stack;
    kernel_thread->thread_context.sp = user_stack;
    kernel_thread->thread_context.fp = user_stack;
    asm volatile("msr tpidr_el1, %0" : : "r"(kernel_thread));
    
    kernel_thread->fp = user_stack;

    current_thread = kernel_thread;
    add_to_run_queue(kernel_thread);
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
    
    new_thread->thread_stack_alloc_ptr=dynamic_malloc(THREAD_STACK_SIZE);
    void *stack_top = &new_thread->thread_stack_alloc_ptr[THREAD_STACK_SIZE];
    
    new_thread->fp = stack_top;
    
    if (entry_point==NULL) { //fork
        // fp will be overwritten in switch_to
        new_thread->thread_context.fp = (uint64_t)stack_top;
        uint32_t kernel_sp; 
        asm volatile("mov %0, sp" : "=r"(kernel_sp));  
        // sp & lr will be overwritten in switch_to
        new_thread->thread_context.sp = (uint64_t)kernel_sp;
        new_thread->thread_context.lr = (uint64_t)frame->elr_el1;
        uart_send_string("new thread lr: ");
        uart_send_hex(new_thread->thread_context.lr);
        uart_send_string("\r\n");
        //之後sp、lr也會再switch_to中進行修改
        thread_inherit_save(frame, new_thread->thread_context);
    }else{
        new_thread->thread_context.fp = (uint64_t)stack_top;
        new_thread->thread_context.sp = (uint64_t)stack_top;
        new_thread->thread_context.lr = (uint64_t)entry_point;
    }
    
    // Add to thread list
    add_to_run_queue(new_thread);
    //new_thread->context.sp = thread_sp-7*16;
    uart_send_string("new thread fp: ");
    uart_send_hex(new_thread->thread_context.fp);
    uart_send_string("\r\n");
    // thread_create_save(new_thread->thread_context.sp, new_thread->thread_context.fp, new_thread->thread_context.lr);
    uart_send_string("new thread sp: ");
    uart_send_hex(new_thread->thread_context.sp);
    uart_send_string("\r\n");
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
        schedule(); // Switch to another thread
        // Should not return here
        uart_send_string("Error: Exited thread returned!\r\n");
        while(1);
    }
}



void schedule(void) {
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
    prev->state = THREAD_READY;
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
    uart_send_string("prev sp: ");
    uart_send_hex(prev->thread_context.sp);
    uart_send_string("\r\n");
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
void fork_schedule(uint64_t parent_sp) {
    trap_frame_t *frame = (trap_frame_t *)parent_sp;
    uart_send_string("[fork_schedule] parent sp: ");
    uart_send_hex(parent_sp);
    uart_send_string("\r\n");
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
    uart_send_string("[schedule] switch to thread: ");
    uart_send_int(current_thread->id);
    uart_send_string("\r\n");
    uart_send_string("current sp: ");
    uart_send_hex(current_thread->thread_context.sp);
    uart_send_string("\r\n");
    uart_send_string("prev sp: ");
    uart_send_hex(prev->thread_context.sp);
    uart_send_string("\r\n");
    asm volatile("mov %0, sp" : "=r"(prev->thread_context.sp));
    asm volatile("mov %0, fp" : "=r"(prev->thread_context.fp));
    asm volatile("mov %0, lr" : "=r"(prev->thread_context.lr));
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
    asm volatile("mrs %0, sp_el0" : "=r"(sp0));
    uart_send_hex(sp0);
    uart_send_string("\r\n");
    uart_send_string("elr_el1: ");
    asm volatile("mrs %0, elr_el1" : "=r"(elr_el1));
    uart_send_hex(elr_el1);
    uart_send_string("\r\n");
    uart_send_string("spsr_el1: ");
    asm volatile("mrs %0, spsr_el1" : "=r"(spsr_el1));
    uart_send_hex(spsr_el1);
    uart_send_string("\r\n");
    uart_send_string("prev user fp: ");
    uart_send_hex(prev->fp);
    uart_send_string("\r\n");
    uart_send_string("next user fp: ");
    uart_send_hex(next->fp);
    uart_send_string("\r\n");
    memcpy((void *)(next->fp-THREAD_STACK_SIZE), (void *)(prev->fp-THREAD_STACK_SIZE), THREAD_STACK_SIZE);
    asm volatile("msr sp_el0,%0 " : :"r"(next->thread_context.fp-((uint64_t)prev->fp-frame->sp_el0)));
    //update frame->sp_el0 to child thread stack
    frame->sp_el0=next->thread_context.fp-((uint64_t)prev->fp-frame->sp_el0);
    // uart_send_string("prev user fp: ");
    // uart_send_hex(frame->x29);
    // uart_send_string("\r\n");
    // uart_send_string("prev user sp: ");
    // uart_send_hex(frame->sp_el0);
    // uart_send_string("\r\n");
    // asm volatile("msr sp_el0,%0 " : :"r"(current_thread->thread_context.fp-(frame->x29-frame->sp_el0)));
    //asm volatile("msr elr_el1,%0 " : :"r"(frame->elr_el1));
    //asm volatile("msr spsr_el1,%0 " : :"r"(0));

    uart_send_string("sp0: ");
    asm volatile("mrs %0, sp_el0" : "=r"(sp0));
    uart_send_hex(sp0);
    uart_send_string("\r\n");
    uart_send_string("elr_el1: ");
    asm volatile("mrs %0, elr_el1" : "=r"(elr_el1));
    uart_send_hex(elr_el1);
    uart_send_string("\r\n");
    uart_send_string("spsr_el1: ");
    asm volatile("mrs %0, spsr_el1" : "=r"(spsr_el1));
    uart_send_hex(spsr_el1);
    uart_send_string("\r\n");
    // Call assembly function to switch context
    extern void svc_switch_to(thread_context_block_t *prev_context, thread_context_block_t *current_context, thread_t *current_thread);
    svc_switch_to(&prev->thread_context, &current_thread->thread_context, current_thread);
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

}

void idle(void) {
    while (1) {
        kill_zombie_thread();
        schedule();
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
        
        schedule();
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
}