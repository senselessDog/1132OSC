
#include <stdint.h>
#include "uart.h"
#include "gpu_interrupt.h"
#include "task_queue.h"
#define MMIO_BASE 0x3F000000
#define IRQ_PENDING1 ((volatile uint32_t *)(MMIO_BASE + 0x0000B204))

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
void sync_lower_el_64_entry(void)
{
    // Read exception-related registers
    unsigned long spsr, elr, esr;

    asm volatile("mrs %0, spsr_el1" : "=r"(spsr));
    asm volatile("mrs %0, elr_el1" : "=r"(elr));
    asm volatile("mrs %0, esr_el1" : "=r"(esr));

    // Print the register values
    uart_send_string("Exception taken!\r\n");
    uart_send_string("SPSR_EL1: 0x");
    uart_send_hex(spsr);
    uart_send_string("\r\nELR_EL1: 0x");
    uart_send_hex(elr);
    uart_send_string("\r\nESR_EL1: 0x");
    uart_send_hex(esr);
    uart_send_string("\r\n");

    return;
}

void el1_irq_entry(uint64_t spsr)
{
    // uart_send_string("EL1 IRQ taken!\r\n");
    //empty();
    if (is_core_timer_irq())
    {
        // 處理計時器中斷
        timer_interrupt_handler();
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
void lower_el_irq_entry(void)
{
    if (is_core_timer_irq()) // 檢查計時器中斷位
    {
        // 創建定時器顯示任務數據
        timer_display_data_t *data = (timer_display_data_t *)simple_alloc(sizeof(timer_display_data_t));

        // 獲取當前計數和頻率
        asm volatile("mrs %0, cntpct_el0" : "=r"(data->count));
        asm volatile("mrs %0, cntfrq_el0" : "=r"(data->freq));

        // 將任務加入佇列（優先級1）
        enqueue_task(&global_task_queue, display_timer_info, data, 1);

        // 設置下一個2秒的超時
        unsigned long next_timeout = 2 * data->freq;
        asm volatile("msr cntp_tval_el0, %0" ::"r"(next_timeout));
        asm volatile("mov x0, #1");
        asm volatile("msr cntp_ctl_el0, x0");
    }
}
