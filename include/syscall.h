#ifndef SYSCALL_H
#define SYSCALL_H

#include <stddef.h> 
typedef struct {
    int64_t x0, x1;   // 16 * 0
    uint64_t x2, x3;   // 16 * 1
    uint64_t x4, x5;   // 16 * 2
    uint64_t x6, x7;   // 16 * 3
    uint64_t x8, x9;   // 16 * 4
    uint64_t x10, x11; // 16 * 5
    uint64_t x12, x13; // 16 * 6
    uint64_t x14, x15; // 16 * 7
    uint64_t x16, x17; // 16 * 8
    uint64_t x18, x19; // 16 * 9
    uint64_t x20, x21; // 16 * 10
    uint64_t x22, x23; // 16 * 11
    uint64_t x24, x25; // 16 * 12
    uint64_t x26, x27; // 16 * 13
    uint64_t x28, x29; // 16 * 14
    uint64_t x30, sp_el0; // 16 * 15
    // union { // 16 * 15
    //     uint64_t x30;     // 实际使用的 8 字节
    //     uint64_t _pad[2]; // 强制占用 16 字节
    // };
    uint64_t spsr_el1, elr_el1; // 16 * 16
    union {
        uint64_t tpidr_el1;
        uint64_t _pad[2];
    };
} trap_frame_t;

typedef struct {
    uint64_t syscall_number;
    uint64_t args[8];
    trap_frame_t *frame_ptr;
} syscall_task_data_t;

// System call handler function
void execute_syscall_task(void *data); // 宣告回呼函數
void handle_syscall(uint64_t syscall_number);

// System call implementations
int sys_getpid();
size_t sys_uart_read(char buf[], size_t size);
size_t sys_uart_write(const char buf[], size_t size);
int sys_exec(const char* name, char* const argv[],trap_frame_t *frame);
int sys_fork();
void sys_exit(trap_frame_t *frame);
int sys_mbox_call(unsigned char ch, unsigned int* mbox);
int sys_kill(trap_frame_t *frame,int pid);

#endif // SYSCALL_H 