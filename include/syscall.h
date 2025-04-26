#ifndef SYSCALL_H
#define SYSCALL_H

#include <stdint.h>
#include <stddef.h>

typedef struct {
    uint64_t regs[31];  // x0-x30
    uint64_t sp;        // sp_EL0
    uint64_t pc;        // elr_el1
    uint64_t pstate;    // spsr_el1
}pt_regs_t;
// System call handler function
void handle_syscall(uint64_t syscall_number);

// System call implementations
int sys_getpid();
size_t sys_uart_read(char buf[], size_t size);
size_t sys_uart_write(const char buf[], size_t size);
int sys_exec(const char* name, char* const argv[]);
int sys_fork();
void sys_exit(int status);
int sys_mbox_call(unsigned char ch, unsigned int* mbox);
int sys_kill(int pid);

#endif // SYSCALL_H 