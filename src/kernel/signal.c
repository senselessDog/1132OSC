#include<thread.h>
#include<syscall.h>
#include "buddy_alloc.h"
#include "mmu.h"
int check_signals(trap_frame_t *frame) {
    thread_t *current = get_current();
    uint32_t pending_signals = current->sigpending;

    if (pending_signals == 0 || current->is_handling_signal) {
        // uart_send_string("[check_signals] Thread: ");
        // uart_send_int(current->id);
        // uart_send_string(" No signal or can't exe signal\r\n");
        // 沒有 pending signals，或者正在處理一個 signal (不處理嵌套)
        return 0;
    }

    int signal_num = -1;
    // 找到優先權最高的 (或編號最小的) pending signal
    // 通常 SIGKILL 有較高優先權，但簡單起見可以從 1 開始找
    for (int i = 1; i < NSIG; i++) {
        if ((pending_signals >> i) & 1) {
            signal_num = i;
            // 如果是 SIGKILL，優先處理 (或者按照 signal 優先順序規則)
            if (signal_num == SIGKILL) break;
        }
    }

    if (signal_num == -1) {
        return 0; // 沒有找到可處理的 signal
    }

    // 清除此 pending signal (原子操作或在禁用中斷下進行，如果 sigpending 可能被異步修改)
    current->sigpending &= ~(1 << signal_num);

    void (*handler_func)(int) = current->sighand[signal_num];

    // uart_send_string("Kernel: Handling signal "); uart_send_int(signal_num);
    // uart_send_string(" for pid "); uart_send_int(current->id);
    // uart_send_string(" with handler at "); uart_send_hex((uint64_t)handler_func); uart_send_string("\r\n");

    if (handler_func == SIG_DFL) {
        // uart_send_string("Kernel: Default handler for signal "); uart_send_int(signal_num); uart_send_string("\r\n");
        if (signal_num == SIGKILL) {
            uart_send_string("[check_singals]SIGKILL default action - Terminating pid "); uart_send_int(current->id); uart_send_string("\r\n");
            // 執行行程終止邏輯
            user_thread_exit(frame, current->id); // 假設這個函式會進行調度，不會返回此處
            return 2; // 行程被終止
        }
        // 處理其他 signal 的預設行為 (許多預設是終止，有些是忽略)
        // 為了簡化，除了 SIGKILL，其他 SIG_DFL 可以暫時當作忽略或印訊息
        uart_send_string("[check_singals]Default action for signal "); 
        uart_send_int(signal_num); 
        uart_send_string(" (unimplemented, treating as ignore).\r\n");
        return 0;
    } else if (handler_func == SIG_IGN) {
        // uart_send_string("Kernel: Ignoring signal "); uart_send_int(signal_num); uart_send_string("\r\n");
        return 0; // 忽略，繼續正常執行
    } else {
        timer_disable();

        // User-defined handler
        uart_send_string("[check_singals]Execute user handler for signal "); 
        uart_send_int(signal_num); 
        uart_send_string("\r\n");
        //check frame address
        // uart_send_string("[check_singals] frame_address= ");
        // uart_send_hex(frame);
        // uart_send_string("\r\n");
        // 備份原始的 user context (當前 trap frame 的內容)
        uart_send_string("[check_singals] Thread Frame x30= ");
        uart_send_hex((uint64_t)frame->x30);
        uart_send_string("\r\n");
        uart_send_string("[check_singals] Thread Frame elr_el1= ");
        uart_send_hex((uint64_t)frame->elr_el1);
        uart_send_string("\r\n");
        save_trap_frame(frame, current);
        
        current->is_handling_signal = 1; // 標記正在處理 signal

        // 修改當前的 trap frame，使其返回到 user-mode handler
        // 1. 設定 handler 的返回地址 (user-mode lr/x30) 指向 sigreturn trampoline
        //這裡多注意一下
        // extern void* sigreturn_trampoline_entry; // 在組合語言中定義
        extern void sigreturn_trampoline_entry(void);
        frame->x30 = (uint64_t)sigreturn_trampoline_entry;
        // 2. 設定 user-mode handler 的參數 (通常 x0 = signal_num)
        frame->x0 = signal_num;

        // 3. 設定 user-mode handler 的執行地址 (elr_el1)
        frame->elr_el1 = (uint64_t)handler_func;

        // 4. 設定 user-mode handler 的堆疊指標 (sp_el0)
        //    您需要為 user-mode handler 準備一個堆疊。
        //    方案 A: 分配一個新的小塊記憶體作為 handler 的 user stack。
        void * handler_stack_kva = dynamic_malloc(Handler_STACK_SIZE);
        current->handler_stack_ptr=handler_stack_kva;
        uint64_t handler_stack_pa=(uint64_t)KVA_TO_PHYS(handler_stack_kva);
        if (mappages(get_current_ttbr0_el1(),HANDLER_BOTTOM_STACK, PAGE_SIZE, handler_stack_pa,USER_DATA_STACK_ATTR)!=1){
        uart_send_string("Error: [sys_mbox_call] Can't map framebuffer to VA\r\n");
        return 0;
        }
        frame->sp_el0 = (uint64_t)HANDLER_TOP_STACK;
        uart_send_string("[check_singals]Finish setting signal "); 
        uart_send_int(signal_num); 
        uart_send_string("\r\n");

        return 1; // 指示 frame 已修改，準備執行 user-mode handler
    }
}