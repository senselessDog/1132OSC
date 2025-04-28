#include <stdint.h>
#include "thread.h"
#include "uart.h"
#include "buddy_alloc.h"
#include "strcmp.h"
#include "syscall.h"

// System call handler function
void handle_syscall(uint64_t parent_sp) {
    // Get system call arguments from registers
    trap_frame_t *frame = (trap_frame_t *)parent_sp;
    uint64_t syscall_number = frame->x8; // 直接读取 x8 的值
    uart_send_string("syscall_number: ");
    uart_send_hex(syscall_number);
    uart_send_string("\r\n");
    uint64_t arg0 = frame->x0;
    uint64_t arg1 = frame->x1;
    uint64_t arg2 = frame->x2;
    uint64_t arg3 = frame->x3;

    // Handle different system calls
    switch (syscall_number) {
        case 0: // SYS_GETPID
            frame->x0 = sys_getpid();
            break;
        case 1: // SYS_UART_READ
            frame->x0 = sys_uart_read((char*)arg0,(size_t)arg1);
            break;
        case 2: // SYS_UART_WRITE
            frame->x0 = sys_uart_write((const char*)arg0, (size_t)arg1);

            break;
        case 3: // SYS_EXEC
            frame->x0 = sys_exec((const char*)arg0, (char* const)arg1);
            break;
        case 4: // SYS_FORK
            frame->x0 = sys_fork(frame);
            break;
        case 5: // SYS_EXIT
            sys_exit((int)arg0);
            break;
        case 6: // SYS_MBOX_CALL
            frame->x0 = sys_mbox_call((unsigned char)arg0, (unsigned int*)arg1);
            break;
        case 7: // SYS_KILL
            frame->x0 = sys_kill((int)arg0);
            break;
        default:
            uart_send_string("Unknown system call: ");
            uart_send_hex(syscall_number);
            uart_send_string("\r\n");
            asm volatile("mov x0, #-1");
            break;
    }
    // int result;
    // asm volatile("mov %0, x0" :"=r"(result));
    // uart_send_string("[handle_syscall] result: ");
    // uart_send_hex(frame->x0);
    // uart_send_string("\r\n");
}

// System call implementations
int sys_getpid() {
    thread_t* current = get_current_thread();
    return current ? current->id : -1;
}

size_t sys_uart_read(char* buf, size_t size) {
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

size_t sys_uart_write(const char* buf, size_t size) {
    // uart_send_string("sys_uart_write\r\n");
    // uart_send_string("buf: ");
    // uart_send_hex(buf);
    // uart_send_string("\r\n");
    // uart_send_string("size: ");
    // uart_send_int(size);
    // uart_send_string("\r\n");
    uart_send_string(buf);
    // uart_send_string("\r\n");
    return size;
}

int sys_exec(const char* name, char* const argv[]) {
    // TODO: Implement exec system call
    return -1;
}

int sys_fork(trap_frame_t *frame) {
    // 創建新進程 (copy elr_el1 and regs(x19~x31))
    thread_t *child = thread_create(NULL, frame);
    
    // 手動保存父進程的寄存器狀態
    uart_send_string("Parent Thread id: ");
    uart_send_int(get_current_thread()->id);
    uart_send_string("\r\n");
    uart_send_string("Child Thread id: ");
    uart_send_int(child->id);
    uart_send_string("\r\n");
    
    //copy stack space
    uart_send_string("[system call] frame->sp_el0: ");
    uart_send_hex(frame->sp_el0);
    uart_send_string("\r\n");
    uart_send_string("[system call] frame->x29: ");
    uart_send_hex(frame->x29);
    uart_send_string("\r\n");
    uint32_t parent_thread_stack_size=frame->x29-frame->sp_el0;
    uart_send_string("Parent thread_stack_size ");
    uart_send_int(parent_thread_stack_size);
    uart_send_string("\r\n");
    // extern uint32_t user_space_size;
    // void * child_base=dynamic_malloc(user_space_size);
    // memcpy(child_base, (void *)frame->sp_el0, parent_thread_stack_size);
    
    // // 從棧上的 trap frame 中讀取 elr_el1 和 spsr_el1
    // asm volatile("ldr x0, [sp, %0]" : : "r"(16 * 16));  // elr_el1
    // asm volatile("mrs x1, tpidr_el1");
    // asm volatile("str x0, [x1, %0]" : : "r"(32 * 8));
    
    // asm volatile("ldr x0, [sp, %0]" : : "r"(16 * 16 + 8));  // spsr_el1
    // asm volatile("mrs x1, tpidr_el1");
    // asm volatile("str x0, [x1, %0]" : : "r"(33 * 8));

    uint64_t parent_sp_el0=frame->sp_el0;
    // memcpy((void *)child->thread_context.fp-THREAD_STACK_SIZE, (void *)frame->x29-THREAD_STACK_SIZE, THREAD_STACK_SIZE);
    // frame->sp_el0=child->thread_context.fp-(frame->x29-frame->sp_el0);
    fork_schedule(frame);
    uint64_t sp_el0;
    asm volatile("mrs %0, sp_el0" : "=r"(sp_el0));
    if (sp_el0==parent_sp_el0){ //parent thread
        return child->id;
    }
    else{ //child thread
        return 0;
    }
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