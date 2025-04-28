#ifndef THREAD_H
#define THREAD_H

#include <stdint.h>
#include "syscall.h"
#define THREAD_STACK_SIZE 4096
#define MAX_THREADS 64

typedef enum {
    THREAD_RUNNING,
    THREAD_READY,
    THREAD_BLOCKED,
    THREAD_DEAD
} thread_state_t;

typedef struct {
    uint64_t x19,x20;  // 16 * 0
    uint64_t x21,x22;  // 16 * 1
    uint64_t x23,x24;  // 16 * 2
    uint64_t x25,x26;  // 16 * 3
    uint64_t x27,x28;  // 16 * 4
    uint64_t fp,lr;  // 16 * 5
    uint64_t sp,base_addr;  // 16 * 6
} thread_context_block_t;

typedef struct {
    int id;
    thread_state_t state;
    void (*entry_point)(void);
    void* thread_stack_alloc_ptr;
    thread_context_block_t thread_context;
    struct thread *next;
    void* fp;
} thread_t;

// --- System Call Numbers ---
#define SYS_GETPID      0
#define SYS_UART_READ   1
#define SYS_UART_WRITE  2
#define SYS_EXEC        3
#define SYS_FORK        4
#define SYS_EXIT        5
#define SYS_MBOX_CALL   6
#define SYS_KILL        7
#define SYS_SIGNAL      8
#define SYS_KILL_SIG    9
// Add more if needed, e.g., for signals later

// Thread management functions
void thread_init(void);
thread_t *thread_create(void (*entry_point)(void),trap_frame_t *frame);
void thread_exit(void);
void schedule(void);
void thread_create_save(uint64_t thread_sp, uint64_t thread_fp, uint64_t thread_lr);
thread_t *get_current_thread(void);
void idle(void);
void kill_zombie_thread(void);
void thread_test(void);

// // --- External assembly functions ---
// extern void switch_to(kcontext_t *prev_ctx, kcontext_t *next_ctx);
// extern thread_t *get_current(void); // Gets current thread TCB pointer from TPIDR_EL1
// extern void set_current(thread_t *thd); // Sets TPIDR_EL1
// extern void init_vectors(void); // Initializes exception vectors
// extern void el1_to_el0(uint64_t elr, uint64_t spsr, uint64_t sp_el0); // Assembly helper for eret
#endif // THREAD_H 