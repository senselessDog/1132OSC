#ifndef THREAD_H
#define THREAD_H

#include <stdint.h>

#define THREAD_STACK_SIZE 4096
#define MAX_THREADS 64

typedef enum {
    THREAD_RUNNING,
    THREAD_READY,
    THREAD_BLOCKED,
    THREAD_DEAD
} thread_state_t;


typedef struct {
    int id;
    thread_state_t state;
    void (*entry_point)(void);
    void* stack[THREAD_STACK_SIZE];
    uint64_t sp;  // 只需要保存堆疊指針
    struct thread *next;
} thread_t;

// Thread management functions
void thread_init(void);
thread_t *thread_create(void (*entry_point)(void));
void thread_exit(void);
void schedule(void);
void thread_create_save(uint64_t thread_sp, uint64_t thread_fp, uint64_t thread_lr);
thread_t *get_current_thread(void);
void idle(void);
void kill_zombie_thread(void);
void thread_test(void);

#endif // THREAD_H 