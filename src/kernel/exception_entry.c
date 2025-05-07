
#include <stdint.h>
#include "uart.h"
#include "gpu_interrupt.h"
#include "task_queue.h"
#include "syscall.h"
#include "thread.h"
#define MMIO_BASE 0x3F000000
#define IRQ_PENDING1 ((volatile uint32_t *)(MMIO_BASE + 0x0000B204))
void display_timer_info(void *arg);
#define CORE0_INTERRUPT_SOURCE ((volatile uint32_t *)(0x40000060))
// 定時器顯示數據結構
typedef struct
{
    uint64_t count;
    uint64_t freq;
} timer_display_data_t;

void empty()
{
}
uint32_t is_core_timer_irq()
{
    return *CORE0_INTERRUPT_SOURCE == (1 << 1);
}
uint32_t is_uart_interrupt()
{
    return *IRQ_PENDING1 & (1 << 29);
}
void sync_lower_el_64_entry(uint64_t parent_sp)
{
    
    uint64_t esr;
    asm volatile("mrs %0, esr_el1" : "=r"(esr));     // 讀取 ESR_EL1
    unsigned int ec = (esr >> 26) & 0x3f;           // 取得 Exception Class
    // uart_send_string("\r\nESR_EL1: 0x");
    // uart_send_hex(esr);
    // uart_send_string("\r\n");
    if (ec == 0b010101) { // 判斷是不是 SVC
        handle_syscall(parent_sp);
        // uint64_t tmp;
        // asm volatile("mrs %0,cntkctl_el1" :  "=r"(tmp));
        // uart_send_string("[sync_lower_el_64_entry] cntkctl_el1: ");
        // uart_send_hex(tmp);
        // uart_send_string("\r\n");
        // asm volatile("mrs %0, cntp_ctl_el0" :  "=r"(tmp));
        // uart_send_string("[sync_lower_el_64_entry] cntp_ctl_el0: ");
        // uart_send_hex(tmp);
        // uart_send_string("\r\n");
        // uart_send_string("[sync_lower_el_64_entry] cntp_tval_el0: ");
        // asm volatile("mrs %0, cntp_tval_el0" :  "=r"(tmp));
        // uart_send_hex(tmp);
        // uart_send_string("\r\n");
    }
    // // Read exception-related registers
    // unsigned long spsr, elr;

    // asm volatile("mrs %0, spsr_el1" : "=r"(spsr));
    // asm volatile("mrs %0, elr_el1" : "=r"(elr));
    //asm volatile("mrs %0, esr_el1" : "=r"(esr));

    // Print the register values
    // uart_send_string("Exception taken!\r\n");
    // uart_send_string("SPSR_EL1: 0x");
    // uart_send_hex(spsr);
    // uart_send_string("\r\nELR_EL1: 0x");
    // uart_send_hex(elr);
    // uart_send_string("\r\nESR_EL1: 0x");
    // uart_send_hex(esr);
    // uart_send_string("\r\n");
    // int result;
    // asm volatile("mov %0, x0" :"=r"(result));
    // uart_send_string("[sync_lower_el_64_entry] result: ");
    // trap_frame_t *frame = (trap_frame_t *)parent_sp;
    // uart_send_hex(frame->x0);
    // uart_send_string("\r\n");

    return;
}

void el1_irq_entry(uint64_t frame_sp)
{
    // disable_interrupts();
    // uart_send_string("EL1 IRQ taken!\r\n");
    //empty();
    if (is_core_timer_irq())
    {
        *CORE0_INTERRUPT_SOURCE &= ~(1 << 1);
        trap_frame_t *frame = (trap_frame_t *)frame_sp;
        user_timeout_handler(frame);
        // // timer_interrupt_handler(); //先暫時關掉
        // uint32_t irq_id;
        // // asm volatile("mrc p15, 0, %0, c12, c12, 0" : "=r"(irq_id)); // 讀 GICC_IAR
        // // 處理計時器中斷
        
        // *CORE0_INTERRUPT_SOURCE &= ~(1 << 1);
        // // *CORE0_TIMER_IRQ_CTRL |= (1 << 1);  // 寫1清除中斷
        // // uart_send_string("[el1_irq_entry] \r\n");
        // user_thread_schedule();
        // // 創建定時器顯示任務數據
        // timer_display_data_t *data = (timer_display_data_t *)simple_alloc(sizeof(timer_display_data_t));


        // // 獲取當前計數和頻率
        // asm volatile("mrs %0, cntpct_el0" : "=r"(data->count));
        // asm volatile("mrs %0, cntfrq_el0" : "=r"(data->freq));
        // // 將任務加入佇列（優先級1）
        // // enqueue_task(&global_task_queue, display_timer_info, data, 1);

        // // 設置下一個計時器
        // unsigned long next_timeout = data->freq>>5;
        // asm volatile("msr cntp_tval_el0, %0" ::"r"(next_timeout));
        // asm volatile("mov x0, #1");
        // asm volatile("msr cntp_ctl_el0, x0");
        // // asm volatile("mcr p15, 0, %0, c12, c12, 1" :: "r"(irq_id)); // 寫 GICC_EOIR
    }
    if (is_gpu_interrupt())
    {
        // 檢查是否為 UART 中斷
        if (is_uart_interrupt())
        {
            uart_irq_handler();
        }
        // 處理其他可能的 GPU 中斷...
    }
    // enable_interrupts();
}
// 顯示定時器信息的任務處理函數 (for EL0)
void display_timer_info(void *arg)
{
    timer_display_data_t *data = (timer_display_data_t *)arg;

    // 計算並顯示啟動後的秒數
    int seconds = data->count / data->freq;
    uart_send_int(seconds);
    uart_send_string(" seconds\r\n");
}
void lower_el_irq_entry(uint64_t frame_sp)
{
    
    // uart_send_string("[lower_el_irq_entry] \r\n");
    if (is_core_timer_irq()) // 檢查計時器中斷位
    {
        *CORE0_INTERRUPT_SOURCE &= ~(1 << 1);
        trap_frame_t *frame = (trap_frame_t *)frame_sp;
        user_timeout_handler(frame);
        // user_thread_schedule(frame);
        // // 創建定時器顯示任務數據
        // timer_display_data_t *data = (timer_display_data_t *)simple_alloc(sizeof(timer_display_data_t));


        // // 獲取當前計數和頻率
        // asm volatile("mrs %0, cntpct_el0" : "=r"(data->count));
        // asm volatile("mrs %0, cntfrq_el0" : "=r"(data->freq));
        // // 將任務加入佇列（優先級1）
        // enqueue_task(&global_task_queue, display_timer_info, data, 1);

        // // 設置下一個計時器
        // unsigned long next_timeout = data->freq>>5;
        // asm volatile("msr cntp_tval_el0, %0" ::"r"(next_timeout));
        // asm volatile("mov x0, #1");
        // asm volatile("msr cntp_ctl_el0, x0");
    }
}
