#include <stdint.h>
#include "thread.h"
#include "uart.h"
#include "buddy_alloc.h"
#include "strcmp.h"
#include "syscall.h"
#include "thread.h"
#include "mailbox.h"
#include "task_queue.h"
extern uint64_t g_initramfs_addr;
int should_run_task_queue = 0;
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

    // uart_send_string("[handle_syscall] SYSCALL_NUM="); uart_send_int(syscall_number); uart_send_string("\r\n");
    should_run_task_queue = 0;
    switch (syscall_number) {
        // --- 延遲處理的系統呼叫 (放入 Task Queue) ---
        case SYS_UART_READ:
        case SYS_UART_WRITE:
        {
            should_run_task_queue = 1;
            // void * test=dynamic_malloc(sizeof(syscall_task_data_t));
            // uart_send_string("[handle_syscall] Test dynamic_malloc= ");
            // uart_send_hex(test);
            // uart_send_string("\r\n");
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
        //for systmcall
        case SYS_SIGNAL: // syscall number 8
            // arg0 (frame->x0) is SIGNAL number
            // arg1 (frame->x1) is handler address (or SIG_DFL, SIG_IGN)
            frame->x0 = sys_signal((int)arg0, (void (*)(int))arg1);
            break;
        case SYS_KILL_SIG: // syscall number 9 (確認這是您用於 POSIX kill 的編號)
            // arg0 (frame->x0) is pid
            // arg1 (frame->x1) is SIGNAL number
            frame->x0 = sys_kill_signal((int)arg0, (int)arg1); // 避免與您之前的 sys_kill 混淆
            break;
        case SYS_SIGRETURN:
            // 此 system call 通常沒有參數，或者參數是隱含的 (trap frame)
            sys_sigreturn(frame); // frame 是當前 handler 返回時的 trap frame
            // sys_sigreturn 會修改 frame 以恢復到原始狀態，所以不需要返回值給 x0
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
    // 1. Save the current thread's state
    thread_t* current_thread=get_current();
    save_trap_frame(frame,current_thread);
    current_thread->state=THREAD_READY;
    //create new thread
    thread_t *execute_thread = thread_create(name, NULL);
    change_tpidr(execute_thread);
    //allocate user space
    run_user_vm((char *)g_initramfs_addr);
    switch_user_address_space(frame->ttbr0_el1);
    frame->x0=execute_thread->id;
    //allocate new thread context and pass to frame
    execute_thread->trap_frame.elr_el1=USER_CODE_VA;
    execute_thread->trap_frame.sp_el0=USER_STACK_TOP_VA;
    execute_thread->trap_frame.spsr_el1=0x0;
    execute_thread->trap_frame.tpidr_el1=execute_thread;
    //ttbr0 already done at run_user_vm
    frame->elr_el1=execute_thread->trap_frame.elr_el1;
    frame->sp_el0=execute_thread->trap_frame.sp_el0;
    frame->spsr_el1=execute_thread->trap_frame.spsr_el1;
    frame->tpidr_el1=execute_thread;
    frame->ttbr0_el1=get_current_ttbr0_el1();
    uart_send_string("execute_thread: ");
    uart_send_hex(execute_thread);
    uart_send_string("\r\n");
    uart_send_string("execute_thread->id: ");
    uart_send_int(execute_thread->id);
    uart_send_string("\r\n");
    return 1;
}
 
int sys_fork(trap_frame_t *frame) {
    thread_t *parent = get_current();
    uart_send_string("[sys_fork]parent thread id: ");
    uart_send_int(parent->id);
    uart_send_string("\r\n");
    uart_send_string("[sys_fork] ---- Parent's Target Trap Frame Values ----\r\n");
    uart_send_string("  elr_el1 (return to user VA): 0x"); uart_send_hex(frame->elr_el1); uart_send_string("\r\n");
    uart_send_string("  sp_el0 (user stack VA):    0x"); uart_send_hex(frame->sp_el0); uart_send_string("\r\n");
    uart_send_string("  spsr_el1 (user PSTATE):    0x"); uart_send_hex(frame->spsr_el1); uart_send_string("\r\n");
    uart_send_string("  ttbr0_el1 (user PGD PA):   0x"); uart_send_hex(frame->ttbr0_el1); uart_send_string("\r\n");
    uart_send_string("  tpidr_el1 (thread ptr):    0x"); uart_send_hex(frame->tpidr_el1); uart_send_string("\r\n");
    // uart_send_string("  x0 (return value):         0x"); uart_send_hex(frame->trap_frame.x0); uart_send_string("\r\n");
    uart_send_string("[sys_fork] ------------------------------------------\r\n");
    
    // Below is child thing
    // 創建新進程 (copy elr_el1 and regs(x19~x31))
    thread_t *child = thread_create(NULL, frame);
    //for parent thread result
    frame->x0=child->id; 
    // 手動保存父進程的寄存器狀態
    save_trap_frame(frame,parent);
    
    
    // uart_send_string("[sys_fork]Parent Thread id: ");
    // uart_send_int(parent->id);
    // uart_send_string("\r\n");
    uart_send_string("[sys_fork]Child Thread id: ");
    uart_send_int(child->id);
    uart_send_string("\r\n");
    //Clear TLB& change ttbr0_el1
    switch_user_address_space(frame->ttbr0_el1);
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
    // memcpy((void *)child->thread_context.fp-THREAD_STACK_SIZE, (void *)frame->x29-THREAD_STACK_SIZE, THREAD_STACK_SIZE);
    // frame->sp_el0=child->thread_context.fp-(frame->x29-frame->sp_el0);
    // frame->tpidr_el1=child;
    fork_schedule(frame,child);
    //Deal with signal
    // 1. 複製 sighand 陣列
    memcpy(child->sighand, parent->sighand, sizeof(void (*)(int)) * NSIG);

    // 2. 子行程的 pending signals 應該是空的
    child->sigpending = 0;

    // 3. 子行程不應該處於 is_handling_signal 狀態
    child->is_handling_signal = 0;
    uart_send_string("[sys_fork] Finished signal copy\r\n");
    // Then print from child_thread->trap_frame
    uart_send_string("[sys_fork] ---- Child's Target Trap Frame Values ----\r\n");
    uart_send_string("  elr_el1 (return to user VA): 0x"); uart_send_hex(frame->elr_el1); uart_send_string("\r\n");
    uart_send_string("  sp_el0 (user stack VA):    0x"); uart_send_hex(frame->sp_el0); uart_send_string("\r\n");
    uart_send_string("  spsr_el1 (user PSTATE):    0x"); uart_send_hex(frame->spsr_el1); uart_send_string("\r\n");
    uart_send_string("  ttbr0_el1 (user PGD PA):   0x"); uart_send_hex(frame->ttbr0_el1); uart_send_string("\r\n");
    uart_send_string("  tpidr_el1 (thread ptr):    0x"); uart_send_hex(frame->tpidr_el1); uart_send_string("\r\n");
    // uart_send_string("  x0 (return value):         0x"); uart_send_hex(frame->trap_frame.x0); uart_send_string("\r\n");
    uart_send_string("[sys_fork] ------------------------------------------\r\n");
    //child return value
    return 0;
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
    uart_send_string("[sys_mbox_call]USER Frame_buffer address=");
    uart_send_hex(user_mbox[28]);
    uart_send_string("\r\n");
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
    //8. mappings
    volatile unsigned int *mailbox=(volatile unsigned int *)kernel_mbox;
    //convert GPU address to ARM address
    mailbox[28] &= 0x3FFFFFFF;
    uint64_t frame_buffer_start_pa=(uint64_t)mailbox[28];
    uint64_t frame_buffer_size=(uint64_t)mailbox[29];
    uint64_t user_pgd_pa=get_current_ttbr0_el1();
    uart_send_string("[sys_mbox_call] Frame_buffer address=");
    uart_send_int(frame_buffer_start_pa);
    uart_send_string("\r\n");
    uart_send_string("[sys_mbox_call] user_pgd_pa=");
    uart_send_int(user_pgd_pa);
    uart_send_string("\r\n");
    if (map_framebuffer_for_user(user_pgd_pa,frame_buffer_start_pa, frame_buffer_size, frame_buffer_start_pa)!=1){
        uart_send_string("Error: [sys_mbox_call] Can't map framebuffer to VA\r\n");
        return 0;
    }
    uart_send_string("[sys_mbox_call] result=");
    uart_send_int(result);
    uart_send_string("\r\n");
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

void (*sys_signal(int signal_num, void (*handler_addr)(int)))(int) {
    thread_t *current = get_current();
    void (*old_handler)(int);

    if (signal_num <= 0 || signal_num >= NSIG) {
        uart_send_string("Error: [sys_signal] invalid signal number");
        // 無效的 signal number
        return -1; // SIG_ERR 通常是 (void (*)(int))-1
    }

    // // SIGKILL 和 SIGSTOP (如果實作) 的 handler 不能被改變
    // if (signal_num == SIGKILL /* || signal_num == SIGSTOP */) {
    //     uart_send_string("Error: [sys_signal] can't change SIGKILL handler");
    //     return -1; // 或者返回錯誤，表示不允許
    // }

    old_handler = current->sighand[signal_num];
    current->sighand[signal_num] = handler_addr;
    uart_send_string("[sys_signal] Sucessful define user_handler\r\n");
    return old_handler; // POSIX 要求返回舊的 handler
}

int sys_kill_signal(int pid, int signal_num) {
    thread_t *target_thread;

    if (signal_num <= 0 || signal_num >= NSIG) {
        uart_send_string("Error: [sys_kill_signal] invalid signal number");
        return -1; // 無效的 signal number (Error: EINVAL)
    }

    target_thread = find_thread_by_pid(pid); // 您需要實作這個函式
    if (!target_thread) {
        uart_send_string("Error: [sys_kill_signal] can't find target thread");
        return -1; // 目標行程不存在 (Error: ESRCH)
    }
    thread_t* current = get_current();

    uart_send_string("[sys_kill_signal] Pid ");
    uart_send_int(current->id);
    uart_send_string(" Delivering signal ");
    uart_send_int(signal_num);
    uart_send_string(" to pid "); 
    uart_send_int(pid); 
    uart_send_string("\r\n");

    // 設定 target_thread->sigpending 中對應 signal_num 的位元
    target_thread->sigpending |= (1 << signal_num);

    // 如果目標行程正在等待某個可被 signal 中斷的事件 (例如 sleep, wait)，
    // 則需要喚醒它，讓它可以檢查並處理 signal。 (進階功能，初期可省略)

    return 0; // 成功
}

void sys_sigreturn(trap_frame_t *current_handler_frame) {
    thread_t *current = get_current();

    if (!current->is_handling_signal) {
        // 錯誤：不在 signal handling 狀態卻呼叫了 sigreturn
        uart_send_string("Error: [sys_sigreturn] sigreturn called without sys_sigreturn\r\n");
        // 可以選擇終止行程或忽略
        //user_thread_exit(current_handler_frame, current->id); // 例如，終止
        return;
    }
    // uart_send_string("[sys_sigreturn] Enter sigreturn\r\n");
    
    // uart_send_string("Kernel: sys_sigreturn called by pid "); uart_send_int(current->id); uart_send_string("\r\n");
    uart_send_string("[sys_sigreturn] Before Thread Frame x30= ");
    uart_send_hex((uint64_t)current_handler_frame->x30);
    uart_send_string("\r\n");
    uart_send_string("[sys_sigreturn] Before Thread Frame elr_el1= ");
    uart_send_hex((uint64_t)current_handler_frame->elr_el1);
    uart_send_string("\r\n");
    // 恢復原始的 user context
    restore_trap_frame(current_handler_frame,current);
    uart_send_string("[sys_sigreturn] After Thread Frame x30= ");
    uart_send_hex((uint64_t)current_handler_frame->x30);
    uart_send_string("\r\n");
    uart_send_string("[sys_sigreturn] After Thread Frame elr_el1= ");
    uart_send_hex((uint64_t)current_handler_frame->elr_el1);
    uart_send_string("\r\n");

    // 清理 signal handling 狀態
    current->is_handling_signal = 0;

    dynamic_free(current->handler_stack_ptr);
    el0_core_timer_enable();

    // 返回後，組合語言會用這個恢復後的 current_handler_frame (現在是原始 frame) 來 eret
}