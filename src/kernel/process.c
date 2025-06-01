#include "thread.h"
#include "uart.h"
#include "buddy_alloc.h"
#include "syscall.h"
#include "mmu.h"
extern thread_t *run_queue;
void user_idle(trap_frame_t *frame) {
    while (1) {
        kill_zombie_thread();
        user_thread_schedule(frame);
    }
}
void user_thread_exit(trap_frame_t *frame,int thread_id) {
    thread_t * delete_thread;
    if (thread_id==-1) { // Delete current thread
        delete_thread = get_current();
    }else{ //kill others
        thread_t * current = run_queue;
        while(current->id!=thread_id){
            current=current->next;
        }
        delete_thread=current;
    } 
    if (delete_thread) {
        uart_send_string("[user_thread_exit] ---- Delete's Target Trap Frame Values ----\r\n");
        uart_send_string("  elr_el1 (return to user VA): 0x"); uart_send_hex(frame->elr_el1); uart_send_string("\r\n");
        uart_send_string("  sp_el0 (user stack VA):    0x"); uart_send_hex(frame->sp_el0); uart_send_string("\r\n");
        uart_send_string("  spsr_el1 (user PSTATE):    0x"); uart_send_hex(frame->spsr_el1); uart_send_string("\r\n");
        uart_send_string("  ttbr0_el1 (user PGD PA):   0x"); uart_send_hex(frame->ttbr0_el1); uart_send_string("\r\n");
        uart_send_string("  tpidr_el1 (thread ptr):    0x"); uart_send_hex(frame->tpidr_el1); uart_send_string("\r\n");
        // uart_send_string("  x0 (return value):         0x"); uart_send_hex(frame->trap_frame.x0); uart_send_string("\r\n");
        uart_send_string("[user_thread_exit] ------------------------------------------\r\n");
        uart_send_string("[user_thread_exit] Thread ID: ");
        uart_send_int(delete_thread->id);
        uart_send_string(" exiting.\r\n");
        delete_thread->state = THREAD_DEAD;
        remove_from_run_queue(delete_thread); // Remove from scheduling
        user_thread_schedule(frame); // Switch to another thread
        delete_thread->state = THREAD_DEAD;// Continue to load all
        uart_send_string("[user_thread_exit] Thread ID: ");
        uart_send_int(delete_thread->id);
        uart_send_string(" exited.\r\n");
        switch_user_address_space(frame->ttbr0_el1);
        uart_send_string("[user_thread_exit] ---- Next's Target Trap Frame Values ----\r\n");
        uart_send_string("  elr_el1 (return to user VA): 0x"); uart_send_hex(frame->elr_el1); uart_send_string("\r\n");
        uart_send_string("  sp_el0 (user stack VA):    0x"); uart_send_hex(frame->sp_el0); uart_send_string("\r\n");
        uart_send_string("  spsr_el1 (user PSTATE):    0x"); uart_send_hex(frame->spsr_el1); uart_send_string("\r\n");
        uart_send_string("  ttbr0_el1 (user PGD PA):   0x"); uart_send_hex(frame->ttbr0_el1); uart_send_string("\r\n");
        uart_send_string("  tpidr_el1 (thread ptr):    0x"); uart_send_hex(frame->tpidr_el1); uart_send_string("\r\n");
        // uart_send_string("  x0 (return value):         0x"); uart_send_hex(frame->trap_frame.x0); uart_send_string("\r\n");
        uart_send_string("[user_thread_exit] ------------------------------------------\r\n");
    }
}

void user_thread_schedule(trap_frame_t *frame) {
    if (!run_queue) {
        shell();
    }
    
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
        if (next->state == THREAD_READY) {
            break;
        }
        else if (next == current_thread && next->state == THREAD_RUNNING) {
            break;
        }
        else if (next == current_thread && next->state == THREAD_DEAD) {
            //uart_send_string("[user_thread_schedule] run_queue is dead, use idle thread\r\n");
            user_idle(frame);
            break;
        }
        next = next->next;
    }
    

    // If no ready thread found, use idle thread
    if (!next) {
        idle();
        return;
    }
    // //check frame address
    // uart_send_string("[user_thread_schedule] frame address: ");
    // uart_send_hex(frame);
    // uart_send_string("\r\n");
    // Switch context
    thread_t *prev = current_thread;
    prev->state = THREAD_READY;
    current_thread = next;
    current_thread->state = THREAD_RUNNING;
    
    while(prev->id==current_thread->id){
        return;
    };
    // uart_send_string("[user_thread_schedule] prev->id: ");
    // uart_send_int(prev->id);
    // uart_send_string("\r\n");
    // uart_send_string("[user_thread_schedule]frame->elr_el1: ");
    // uart_send_hex(frame->elr_el1);
    // uart_send_string("\r\n");
    // uart_send_string("[user_thread_schedule]frame->sp_el0: ");
    // uart_send_hex(frame->sp_el0);
    // uart_send_string("\r\n");
    // uart_send_string("[user_thread_schedule]frame->tpidr_el1: ");
    // uart_send_hex(frame->tpidr_el1);
    // uart_send_string("\r\n");
    // Save prev thread's trap frame
    save_trap_frame(frame,prev);
    // Restore current thread's trap frame
    restore_trap_frame(frame,current_thread);
    change_tpidr(current_thread);
    // uart_send_string("[user_thread_schedule] current_thread->id: ");
    // uart_send_int(current_thread->id);
    // uart_send_string("\r\n");
    // uart_send_string("[user_thread_schedule]frame->elr_el1: ");
    // uart_send_hex(frame->elr_el1);
    // uart_send_string("\r\n");
    // uart_send_string("[user_thread_schedule]frame->sp_el0: ");
    // uart_send_hex(frame->sp_el0);
    // uart_send_string("\r\n");
    // uart_send_string("[user_thread_schedule]frame->tpidr_el1: ");
    // uart_send_hex(frame->tpidr_el1);
    // uart_send_string("\r\n");
}