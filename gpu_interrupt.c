#include <stdint.h>
#include "uart.h"

// 啟用 UART 中斷
void uart_enable_interrupt()
{
    // 啟用 AUX 中斷 (位元 29)
    // uart_send_string("Enable AUX IRQ\r\n");
    *ENABLE_IRQS1 = (1 << 29);
    // uart_send_hex((uint32_t)*AUX_MU_LCR_REG);
    // uart_send_string("\r\n");
    // uart_send_hex((uint32_t)*AUX_MU_IER_REG);
    // uart_send_string("\r\n");
    // uart_send_hex((uint32_t)*ENABLE_IRQS1);
    // uart_send_string("\r\n");
}

// 檢查是否為 GPU 中斷
int is_gpu_interrupt()
{
    return *CORE0_INTERRUPT_SOURCE & GPU_INTERRUPTS;
}
void uart_irq_handler()
{
    // 讀取中斷狀態暫存器確定中斷來源
    uint32_t iir = *AUX_MU_IIR_REG & 0x6;

    // 處理接收中斷
    if (iir == 4)
    {
        uart_disable_rx_interrupt();
        // 當有資料可讀且緩衝區未滿時
        while ((*AUX_MU_LSR_REG & 0x01) && !is_buffer_full(rx_head, rx_tail))
        {
            char c = (char)(*AUX_MU_IO_REG);
            buffer_push(rx_buffer, &rx_head, c);
        }
        uart_enable_rx_interrupt();
    }

    // 處理發送中斷
    if (iir == 2)
    {
        uart_disable_tx_interrupt();
        // 持續發送數據直到發送緩衝區為空或 UART TX FIFO 滿
        while (!is_buffer_empty(tx_head, tx_tail) && (*AUX_MU_LSR_REG & 0x20))
        {
            *AUX_MU_IO_REG = buffer_pop(tx_buffer, &tx_tail);
        }

        // 如果緩衝區為空，禁用發送中斷
        if (is_buffer_empty(tx_head, tx_tail))
        {
            *AUX_MU_IER_REG &= ~2; // 關閉發送中斷
        }
        else
        {
            // 還有更多數據要發送，保持中斷啟用
            uart_enable_tx_interrupt();
        }
    }
}