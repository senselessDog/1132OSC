#include <stdint.h>
#include "thread.h"
#include "uart.h"
#include "buddy_alloc.h"
#include "strcmp.h"
#include "syscall.h"
#include "thread.h"
#include "mailbox.h"
#include "task_queue.h"
extern uint32_t g_initramfs_addr;
// System call handler function
// 任務回呼函數的原型
void execute_syscall_task(void *data);

// System call handler function
// kernel_sp 指向 Trap Frame
void handle_syscall(uint64_t kernel_sp) {
    disable_interrupts(); // 禁用中斷
    trap_frame_t *frame = (trap_frame_t *)kernel_sp;
    uint64_t syscall_number = frame->x8; // 從 x8 讀取 syscall 號
    uint64_t arg0 = frame->x0;
    uint64_t arg1 = frame->x1;
    uint64_t arg2 = frame->x2;
    // ... 可以讀取更多參數 ...

    // uart_send_string("[Syscall Entry] SYSCALL_NUM="); uart_send_hex(syscall_number); uart_send_string("\r\n");

    switch (syscall_number) {
        // --- 延遲處理的系統呼叫 (放入 Task Queue) ---
        case SYS_UART_READ:
        case SYS_UART_WRITE:
        {
            syscall_task_data_t *task_data = (syscall_task_data_t *)dynamic_malloc(sizeof(syscall_task_data_t));
            if (!task_data) {
                frame->x0 = -1;
                return;
            }
            task_data->syscall_number = syscall_number;
            task_data->args[0] = arg0;
            task_data->args[1] = arg1;
            // ... copy more args if needed ...
            task_data->frame_ptr = frame;

            // 設定優先級
            int priority = 0; // UART_WRITE 預設優先級
            if (syscall_number == SYS_UART_READ) {
                priority = -1; // UART_READ 低優先級
            }

            enqueue_task(&global_task_queue, execute_syscall_task, task_data, priority);
            // 結果由 execute_syscall_task 寫回 frame->x0
            break;
        }

        // --- 直接處理的系統呼叫 ---
        case SYS_GETPID:
            frame->x0 = sys_getpid();
            break;
        case SYS_MBOX_CALL:
            // !!! 警告：直接執行 MBOX Call 可能會因為等待 GPU 而增加中斷延遲 !!!
            // !!! 因為它在 SVC handler (中斷屏蔽) 中執行 !!!
            frame->x0 = sys_mbox_call((unsigned char)arg0, (unsigned int*)arg1);
            break;
        case SYS_FORK:
            frame->x0 = sys_fork(frame);
            break;
        case SYS_EXEC:
            frame->x0 = sys_exec((const char*)arg0, (char* const*)arg1, frame);
            // Note: Successful exec won't return here
            break;
        case SYS_EXIT:
            sys_exit(frame); // Does not return
            break; // Should not be reached
        case SYS_KILL:
            sys_kill(frame, (int)arg0);
            frame->x0 = 0; // Assume kill returns 0 on success?
            break;

        default:
            uart_send_string("Unknown system call: ");
            uart_send_hex(syscall_number);
            uart_send_string("\r\n");
            frame->x0 = -1;
            break;
    }
    // C 函數返回，組合語言會接著呼叫 process_task_queue (處理延遲的 READ/WRITE)
}

// System call implementations
int sys_getpid() {
    thread_t* current = get_current();
    return current ? current->id : -1;
}
// static int user_buffer_index=0;
// static int buffer_size=0;
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
    //non-blocking
    // for (size_t i = 0; i < size; i++) {
    //     char c=0;
    //     int result=uart_async_recv(&c);
    //     uart_send_string("result: ");
    //     uart_send_int(result);
    //     uart_send_string("\r\n");
    //     if(result){
    //         buf[i]=c;
    //         return size;
    //     }
    // }
    //blocking
    for (size_t i = 0; i < size; i++) {
        buf[i]=uart_recv();
        return size;
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
    return 0;
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
    save_trap_frame(frame,get_current());
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
    
    uart_send_string("[sys_fork]parent thread id: ");
    thread_t *parent = get_current();
    uart_send_int(parent->id);
    uart_send_string("\r\n");
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
    
    // 手動保存父進程的寄存器狀態
    save_trap_frame(frame,get_current());
    uart_send_string("[sys_fork]Parent Thread id: ");
    parent = get_current();
    uart_send_int(parent->id);
    uart_send_string("\r\n");
    uart_send_string("[sys_fork]Child Thread id: ");
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
    // uart_send_string("[sys_mbox_call] Allocating kernel mailbox buffer...\r\n");
    // uart_send_hex((uint32_t)kernel_mbox);
    // uart_send_string("\r\n");
    // 4. 複製請求 (User -> Kernel)
    //    !!! 警告：沒有 MMU，直接 memcpy 有風險 !!!
    memcpy((void *)kernel_mbox, user_mbox, buffer_size);

    // 5. 呼叫底層 Mailbox 函數 (使用核心緩衝區)
    int result = mailbox_call_lowlevel(ch, kernel_mbox);

    // 6. 複製結果 (Kernel -> User) - 即使 lowlevel 失敗，也可能需要複製錯誤碼回去
    //    !!! 警告：沒有 MMU，直接 memcpy 有風險 !!!
    //    通常 mailbox[1] 會被 GPU 更新為 RESPONSE_SUCCESS 或 RESPONSE_ERROR
    memcpy(user_mbox, (void *)kernel_mbox, buffer_size);
    // uart_send_string("[sys_mbox_call] Mailbox call result: ");
    // uart_send_int(result);
    // uart_send_string("\r\n");
    // 7. 釋放核心緩衝區
    dynamic_free((void*)kernel_mbox);
    // uart_send_string("[sys_mbox_call] Sucess finish kernel mailbox buffer...\r\n");

    // 8. 回傳結果 (lowlevel 的回傳值, 0 代表成功)
    return result;
}

int sys_kill(trap_frame_t *frame,int pid) {
    user_thread_exit(frame,pid);
    // TODO: Implement kill system call
    return 0;
} 

// 這個函數在 process_task_queue 中被呼叫
void execute_syscall_task(void *data) {
    if (!data) return;

    syscall_task_data_t *task_data = (syscall_task_data_t *)data;
    trap_frame_t *frame = task_data->frame_ptr; // 取得對應的 Trap Frame

    if (!frame) {
        uart_send_string("KERN Error: Task data frame pointer is NULL!\r\n");
        dynamic_free(task_data);
        return;
    }

    uint64_t ret_val = 0; // 預設回傳值

    // 根據系統呼叫編號執行
    switch (task_data->syscall_number) {
        case SYS_UART_READ:
            // !!! 警告：如果 sys_uart_read 阻塞，會卡住 Task Queue !!!
            ret_val = sys_uart_read((char*)task_data->args[0], (size_t)task_data->args[1]);
            break;
        case SYS_UART_WRITE:
            ret_val = sys_uart_write((const char*)task_data->args[0], (size_t)task_data->args[1]);
            break;

        // --- 不再處理其他系統呼叫 ---
        default:
            uart_send_string("Error: Unexpected syscall in task queue: ");
            uart_send_hex(task_data->syscall_number);
            uart_send_string("\r\n");
            ret_val = -1; // 寫入錯誤碼
            break;
    }

    // 將結果寫回 Trap Frame 的 x0
    frame->x0 = ret_val;

    // 釋放儲存任務資料的記憶體
    dynamic_free(task_data);
}