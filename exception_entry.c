
#include <stdint.h>
void uart_send_string(char *str);
void uart_send_hex(uint32_t value);
void uart_send_int(int value);

#define CORE0_IRQ_SOURCE ((volatile unsigned int *)(0x40000060))

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
void el0_irq_entry(void)
{
    if (*CORE0_IRQ_SOURCE == (1 << 1)) // 檢查計時器中斷位
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
    else
    {
        uart_send_string("Unknown IRQ!\r\n");
    }
    return;
}