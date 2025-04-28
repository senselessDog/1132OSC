#include "thread.h"
#include "uart.h"
#include "buddy_alloc.h"
#include "syscall.h"
extern thread_t *run_queue;
void user_idle(void) {
    while (1) {
        kill_zombie_thread();
        user_thread_schedule();
    }
}
void user_thread_exit(trap_frame_t *frame,int thread_id) {
    thread_t * delete_thread;
    if (thread_id==NULL) { // Delete current thread
        delete_thread = get_current();
    }else{ //kill others
        thread_t * current = run_queue;
        while(current->id!=thread_id){
            current=current->next;
        }
        delete_thread=current;
    } 
    if (delete_thread) {
        uart_send_string("Thread ID: ");
        uart_send_int(delete_thread->id);
        uart_send_string(" exiting.\r\n");
        delete_thread->state = THREAD_DEAD;
        remove_from_run_queue(delete_thread); // Remove from scheduling
        user_thread_schedule(frame); // Switch to another thread
        // Continue to load all
    }
}

void user_thread_schedule(trap_frame_t *frame) {
    if (!run_queue) {
        shell();
    }
    thread_t *current_thread = get_current_thread();
    // Find next ready thread
    thread_t *next = current_thread->next;
    while (next) {
        if (next->state == THREAD_READY) {
            break;
        }
        else if (next == current_thread && next->state == THREAD_RUNNING) {
            break;
        }
        else if (next == run_queue && next->state == THREAD_DEAD) {
            user_idle();
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

    //update prev thread sp
    // uint64_t prev_thread_sp;
    // asm volatile("mov %0, sp" : "=r"(prev_thread_sp));
    // prev->sp = prev_thread_sp-7*16;
    //Don't habe to deal with current thread sp
    //current thread sp is already updated in switch.S
    uart_send_string("[user_thread_schedule] switch to thread: ");
    uart_send_int(current_thread->id);
    uart_send_string("\r\n");
    
    save_trap_frame(frame,prev);
    restore_trap_frame(frame,current_thread);
    uart_send_string("[user_thread_schedule]frame->elr_el1: ");
    uart_send_hex(frame->elr_el1);
    uart_send_string("\r\n");
    uart_send_string("[user_thread_schedule]frame->sp_el0: ");
    uart_send_hex(frame->sp_el0);
    uart_send_string("\r\n");
    uart_send_string("[user_thread_schedule]frame->tpidr_el1: ");
    uart_send_hex(frame->tpidr_el1);
    uart_send_string("\r\n");
}