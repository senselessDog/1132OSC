#include "thread.h"
#include "uart.h"
#include "buddy_alloc.h"
#include <stddef.h>

static thread_t *current_thread = NULL;
static thread_t *thread_list = NULL;
static int next_thread_id = 1;

void thread_init(void) {
    current_thread = NULL;
    thread_list = NULL;
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
    kernel_thread->sp = kernel_sp;

    current_thread = kernel_thread;
    thread_list = kernel_thread;
}

thread_t *thread_create(void (*entry_point)(void)) {
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
    

    // Initialize stack
    void *stack_top = &new_thread->stack[THREAD_STACK_SIZE - 1];
    uint64_t thread_sp = (uint64_t)stack_top;
    uint64_t thread_fp = (uint64_t)stack_top;
    uint64_t thread_lr = (uint64_t)entry_point;
    
    
    // Add to thread list
    if (!thread_list) {
        thread_list = new_thread;
    } else {
        thread_t *current = thread_list;
        while (current->next) {
            current = current->next;
        }
        current->next = new_thread;
    }
    new_thread->sp = thread_sp-7*16;
    uart_send_string("new thread sp: ");
    uart_send_int(new_thread->sp);
    uart_send_string("\r\n");
    thread_create_save(new_thread->sp, thread_fp, thread_lr);
    uart_send_string("new thread sp: ");
    uart_send_int(new_thread->sp);
    uart_send_string("\r\n");
    return new_thread;
}

void thread_exit(void) {
    if (current_thread) {
        current_thread->state = THREAD_DEAD;
        schedule();
    }
}

void schedule(void) {
    if (!thread_list) {
        return;
    }

    // Find next ready thread
    thread_t *next = current_thread->next;
    while (next) {
        if (next->state == THREAD_READY) {
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

    //update prev thread sp
    uint64_t prev_thread_sp;
    asm volatile("mov %0, sp" : "=r"(prev_thread_sp));
    prev->sp = prev_thread_sp-7*16;
    //Don't habe to deal with current thread sp
    //current thread sp is already updated in switch.S
    uart_send_string("switch to thread: ");
    uart_send_int(current_thread->id);
    uart_send_string("\r\n");
    uart_send_string("current sp: ");
    uart_send_int(current_thread->sp);
    uart_send_string("\r\n");
    uart_send_string("prev sp: ");
    uart_send_int(prev->sp);
    uart_send_string("\r\n");
    

    // Call assembly function to switch context
    extern void switch_to(uint64_t *prev_sp, uint64_t *current_sp, thread_t *current_thread);
    switch_to(prev_thread_sp, current_thread->sp, current_thread);
}

thread_t *get_current_thread(void) {
    return current_thread;
}

void idle(void) {
    while (1) {
        kill_zombie_thread();
        schedule();
    }
}
void kill_zombie_thread(void) {
    // Check for dead threads and clean them up
    thread_t *current = thread_list;
    thread_t *prev = NULL;
    while (current) {
        if (current->state == THREAD_DEAD) {
            if (prev) {
                prev->next = current->next;
            } else {
                thread_list = current->next;
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
        thread_t *current = get_current_thread();
        uart_send_string("Thread id: ");
        uart_send_int(current->id);
        uart_send_string(" ");
        uart_send_int(i);
        uart_send_string("\r\n");
        
        // Simple delay
        for(int j = 0; j < 1000000; j++) {
            asm volatile("nop");
        }
        
        schedule();
    }
    thread_exit();
}
