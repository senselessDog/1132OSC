#include <stdint.h>
#include "thread.h"
#include "uart.h"
#include "buddy_alloc.h"
#include "strcmp.h"
#include "syscall.h"
#include "thread.h"
#include "mailbox.h"
extern uint32_t g_initramfs_addr;
// System call handler function
void handle_syscall(uint64_t parent_sp) {
    // Get system call arguments from registers
    trap_frame_t *frame = (trap_frame_t *)parent_sp;
    uint64_t syscall_number = frame->x8; // 直接读取 x8 的值
    // uart_send_string("syscall_number: ");
    // uart_send_hex(syscall_number);
    // uart_send_string("\r\n");
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
            frame->x0 = sys_exec((const char*)arg0, (char* const)arg1,frame);
            break;
        case 4: // SYS_FORK
            frame->x0 = sys_fork(frame);
            break;
        case 5: // SYS_EXIT
            sys_exit(frame);
            break;
        case 6: // SYS_MBOX_CALL
            frame->x0 = sys_mbox_call((unsigned char)arg0, (unsigned int*)arg1);
            break;
        case 7: // SYS_KILL
            sys_kill(frame,(int)arg0);
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
    thread_t* current = get_current();
    return current ? current->id : -1;
}
// static int user_buffer_index=0;
static int buffer_size=0;
size_t sys_uart_read(char* buf, size_t size) {
    // uart_send_string("sys_uart_read\r\n");
    // uart_send_string("buf address: ");
    // uart_send_hex(buf);
    // uart_send_string("\r\n");
    // uart_send_string("size: ");
    // uart_send_int(size);
    // uart_send_string("\r\n");
    // uart_send_string("read index: ");
    // uart_send_int(user_buffer_index);
    // uart_send_string("\r\n");
    // buf[user_buffer_index] = uart_recv();
    // uart_send(buf[user_buffer_index]);
    // uart_send_string("\r\n");
    for (size_t i = 0; i < size; i++) {
        buf[i] = uart_recv();
        if (buf[i] == '\r' || buf[i] == '\n') {
            //buf[i] = '\0';
        }else{
            buffer_size++;
        }
    }
    // if (buf[user_buffer_index] == '\r' || buf[user_buffer_index] == '\n') {
    //     buf[user_buffer_index] = '\0';
        
    //     user_buffer_index=0;
    //     uart_send_string("read result\r\n");
    //     uart_send_string(buf);
    //     uart_send_string("\r\n");
    //     // return i;
    // }
    // user_buffer_index++;
    // uart_send_string("buffer_size: ");
    // uart_send_int(buffer_size);
    // uart_send_string("\r\n");
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
    // uart_send_string(buf);
    // uart_send_string("\r\n");
    for (size_t i = 0; i < size && buf[i] != '\0'; i++) {
        uart_send(buf[i]);
    }
    
    return size;
}

int sys_exec(const char* name, char* const argv[],trap_frame_t *frame) {
    // TODO: Implement exec system call
    save_trap_frame(frame,get_current_thread());
    //create new thread
    thread_t *execute_thread = thread_create(name, NULL);
    //allocate user space
    void *user_base=run_user(g_initramfs_addr);
    frame->x0=execute_thread->id;
    //allocate new thread context and pass to frame
    execute_thread->trap_frame.elr_el1=user_base;
    execute_thread->trap_frame.sp_el0=execute_thread->fp;
    execute_thread->trap_frame.spsr_el1=0x0;
    execute_thread->trap_frame.tpidr_el1=execute_thread;
    frame->elr_el1=execute_thread->trap_frame.elr_el1;
    frame->sp_el0=execute_thread->trap_frame.sp_el0;
    frame->spsr_el1=execute_thread->trap_frame.spsr_el1;
    frame->tpidr_el1=execute_thread;
    uart_send_string("execute_thread: ");
    uart_send_hex(execute_thread);
    uart_send_string("\r\n");
    uart_send_string("execute_thread->id: ");
    uart_send_int(execute_thread->id);
    uart_send_string("\r\n");
    return 1;
}
 
int sys_fork(trap_frame_t *frame) {
    
    
    uart_send_string("[sys_fork]frame->elr_el1: ");
    uart_send_hex(frame->elr_el1);
    uart_send_string("\r\n");
    uart_send_string("[sys_fork]frame->sp_el0: ");
    uart_send_hex(frame->sp_el0);
    uart_send_string("\r\n");
    uart_send_string("[sys_fork]frame->tpidr_el1: ");
    uart_send_hex(frame->tpidr_el1);
    uart_send_string("\r\n");
    // 創建新進程 (copy elr_el1 and regs(x19~x31))
    thread_t *child = thread_create(NULL, frame);
    //for parent thread
    frame->x0=child->id; 
    save_trap_frame(frame,get_current_thread());
    // 手動保存父進程的寄存器狀態
    uart_send_string("Parent Thread id: ");
    uart_send_int(get_current()->id);
    uart_send_string("\r\n");
    uart_send_string("Child Thread id: ");
    uart_send_int(child->id);
    uart_send_string("\r\n");
    
    //copy stack space
    // uart_send_string("[system call] frame->sp_el0: ");
    // uart_send_hex(frame->sp_el0);
    // uart_send_string("\r\n");
    // uart_send_string("[system call] frame->x29: ");
    // uart_send_hex(frame->x29);
    // uart_send_string("\r\n");
    // uint32_t parent_thread_stack_size=frame->x29-frame->sp_el0;
    // uart_send_string("Parent thread_stack_size ");
    // uart_send_int(parent_thread_stack_size);
    // uart_send_string("\r\n");
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
    frame->tpidr_el1=child;
    fork_schedule(frame);
    uint64_t sp_el0;
    //asm volatile("mrs %0, sp_el0" : "=r"(sp_el0));
    sp_el0=frame->sp_el0;
    if (sp_el0==parent_sp_el0){ //parent thread
        return child->id;
    }
    else{ //child thread
        return 0;
    }
}

void sys_exit(trap_frame_t *frame) {
    user_thread_exit(frame,NULL);
    uart_send_string("[sys_exit]frame->elr_el1: ");
    uart_send_hex(frame->elr_el1);
    uart_send_string("\r\n");
    uart_send_string("[sys_exit]frame->sp_el0: ");
    uart_send_hex(frame->sp_el0);
    uart_send_string("\r\n");
    uart_send_string("[sys_exit]frame->tpidr_el1: ");
    uart_send_hex(frame->tpidr_el1);
    uart_send_string("\r\n");
    return;
}

int sys_mbox_call(unsigned char ch, unsigned int* user_mbox) {
    // TODO: Implement mailbox system call
    extern int mailbox_call_lowlevel(unsigned char ch, volatile unsigned int *kernel_mbox);

    // 1. 驗證參數 (通道號)
    if (ch > 15) {
        uart_send_string("KERN: Invalid mailbox channel.\r\n");
        return -1; // 回傳錯誤碼給使用者
    }

    // 2. 讀取使用者空間的 buffer 大小 (假設 user_mbox 指標是有效的)
    //    !!! 警告：沒有 MMU，直接讀取使用者指標有風險 !!!
    unsigned int buffer_size;
    // 這裡需要一種安全的方式讀取，或者暫時直接讀取
    //memcpy(&buffer_size, user_mbox, sizeof(unsigned int)); // 稍微安全一點點
    buffer_size=user_mbox[0];

    // 基本的大小檢查
    if (buffer_size < 8 || buffer_size > 1024*4 ) { // 最小 2 個 uint, 合理最大值
        uart_send_string("KERN: Invalid mailbox buffer size.\r\n");
        return -1;
    }
    // 確保大小是 4 的倍數
     if (buffer_size % 4 != 0) {
         uart_send_string("KERN: Mailbox buffer size not multiple of 4.\r\n");
         return -1;
     }


    // 3. 分配 16 位元組對齊的核心緩衝區
    void *kernel_mbox =dynamic_malloc(buffer_size);

    // 4. 複製請求 (User -> Kernel)
    //    !!! 警告：沒有 MMU，直接 memcpy 有風險 !!!
    memcpy((void *)kernel_mbox, user_mbox, buffer_size);

    // 5. 呼叫底層 Mailbox 函數 (使用核心緩衝區)
    int result = mailbox_call_lowlevel(ch, kernel_mbox);

    // 6. 複製結果 (Kernel -> User) - 即使 lowlevel 失敗，也可能需要複製錯誤碼回去
    //    !!! 警告：沒有 MMU，直接 memcpy 有風險 !!!
    //    通常 mailbox[1] 會被 GPU 更新為 RESPONSE_SUCCESS 或 RESPONSE_ERROR
    memcpy(user_mbox, (void *)kernel_mbox, buffer_size);

    // 7. 釋放核心緩衝區
    dynamic_free((void*)kernel_mbox);

    // 8. 回傳結果 (lowlevel 的回傳值, 0 代表成功)
    return result;
}

int sys_kill(trap_frame_t *frame,int pid) {
    user_thread_exit(frame,NULL);
    // TODO: Implement kill system call
    return 0;
} 