#include <stdint.h>
#include "thread.h"
#include "uart.h"
#include "buddy_alloc.h"

// System call handler function
void handle_syscall(uint64_t syscall_number) {
    // Get system call arguments from registers
    uint64_t arg0, arg1, arg2, arg3;
    asm volatile("mov %0, x0" : "=r"(arg0));
    asm volatile("mov %1, x1" : "=r"(arg1));
    asm volatile("mov %2, x2" : "=r"(arg2));
    asm volatile("mov %3, x3" : "=r"(arg3));

    // Handle different system calls
    switch (syscall_number) {
        case 0: // SYS_GETPID
            asm volatile("mov x0, %0" : : "r"(sys_getpid()));
            break;
        case 1: // SYS_UART_READ
            asm volatile("mov x0, %0" : : "r"(sys_uart_read((char*)arg0, arg1)));
            break;
        case 2: // SYS_UART_WRITE
            asm volatile("mov x0, %0" : : "r"(sys_uart_write((const char*)arg0, arg1)));
            break;
        case 3: // SYS_EXEC
            asm volatile("mov x0, %0" : : "r"(sys_exec((const char*)arg0, (char* const*)arg1)));
            break;
        case 4: // SYS_FORK
            asm volatile("mov x0, %0" : : "r"(sys_fork()));
            break;
        case 5: // SYS_EXIT
            sys_exit(arg0);
            break;
        case 6: // SYS_MBOX_CALL
            asm volatile("mov x0, %0" : : "r"(sys_mbox_call(arg0, (unsigned int*)arg1)));
            break;
        case 7: // SYS_KILL
            asm volatile("mov x0, %0" : : "r"(sys_kill(arg0)));
            break;
        default:
            uart_send_string("Unknown system call: ");
            uart_send_hex(syscall_number);
            uart_send_string("\r\n");
            asm volatile("mov x0, #-1");
            break;
    }
}

// System call implementations
int sys_getpid() {
    thread_t* current = get_current_thread();
    return current ? current->id : -1;
}

size_t sys_uart_read(char buf[], size_t size) {
    for (size_t i = 0; i < size; i++) {
        buf[i] = uart_recv();
        if (buf[i] == '\r' || buf[i] == '\n') {
            buf[i] = '\0';
            return i;
        }
    }
    buf[size - 1] = '\0';
    return size;
}

size_t sys_uart_write(const char buf[], size_t size) {
    for (size_t i = 0; i < size && buf[i] != '\0'; i++) {
        uart_send(buf[i]);
    }
    return size;
}

int sys_exec(const char* name, char* const argv[]) {
    // TODO: Implement exec system call
    return -1;
}

int sys_fork() {
    // 創建新進程
    thread_t *child = thread_create(NULL);
    
    // 手動保存父進程的寄存器狀態
    uart_send_string("Parent Thread id: ");
    uart_send_int(get_current_thread()->id);
    uart_send_string("\r\n");
    uint64_t parent_sp = get_current_thread()->sp;
    
    // 保存通用寄存器
    for(int i = 0; i < 31; i++) {
        asm volatile("mov x0, %0" : : "r"(i));
        asm volatile("mrs x1, tpidr_el1");
        asm volatile("str x0, [x1, %0]" : : "r"(i * 8));
    }
    
    // 保存 sp_EL0
    asm volatile("mrs x0, sp_el0");
    asm volatile("mrs x1, tpidr_el1");
    asm volatile("str x0, [x1, %0]" : : "r"(31 * 8));
    
    // 從棧上的 trap frame 中讀取 elr_el1 和 spsr_el1
    asm volatile("ldr x0, [sp, %0]" : : "r"(16 * 16));  // elr_el1
    asm volatile("mrs x1, tpidr_el1");
    asm volatile("str x0, [x1, %0]" : : "r"(32 * 8));
    
    asm volatile("ldr x0, [sp, %0]" : : "r"(16 * 16 + 8));  // spsr_el1
    asm volatile("mrs x1, tpidr_el1");
    asm volatile("str x0, [x1, %0]" : : "r"(33 * 8));
    
    return child->id;
}

void sys_exit(int status) {
    thread_exit();
}

int sys_mbox_call(unsigned char ch, unsigned int* mbox) {
    // TODO: Implement mailbox system call
    return -1;
}

int sys_kill(int pid) {
    // TODO: Implement kill system call
    return -1;
} 