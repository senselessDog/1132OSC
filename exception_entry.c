
#include <stdint.h>
#include "uart.h"
#include "gpu_interrupt.h"
#define MMIO_BASE 0x3F000000
#define IRQ_PENDING1 ((volatile uint32_t *)(MMIO_BASE + 0x0000B204))

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
void lower_el_irq_entry(void)
{
    if (is_core_timer_irq()) // 檢查計時器中斷位
    {
        // uart_send_string("Timer IRQ!\r\n");
        //  get count and frequency
        unsigned long long count, freq;
        asm volatile("mrs %0, cntpct_el0" : "=r"(count));
        asm volatile("mrs %0, cntfrq_el0" : "=r"(freq));

        // print the time since boot
        int seconds = count / freq;
        // uart_send_string("time since boot:");
        // uart_send_string("Time since boot ");
        uart_send_int(seconds);
        uart_send_string(" seconds\r\n");

        // set timeout to 2 seconds
        unsigned long next_timeout = 2 * freq;
        asm volatile("msr cntp_tval_el0, %0" ::"r"(next_timeout));
    }
}
void irq_entry(void)
{
    // asm volatile("msr DAIFSet, 0xf");
    // uint64_t spsr;
    // asm volatile("mrs %0, spsr_el1\n" : "=r"(spsr));

    // int from_el0 = ((spsr & 0xF) == 0x0);
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
    // asm volatile("msr DAIFClr, 0xf");
    return;
}