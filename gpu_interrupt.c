#include <stdint.h>
#include "uart.h"
#include "task_queue.h"
#ifndef NULL
#define NULL ((void *)0)
#endif
void disable_interrupts()
{
    asm volatile("msr DAIFSet, #0xf");
}
void enable_interrupts()
{
    asm volatile("msr DAIFClr, #0xf");
}
// 啟用 UART 中斷
void uart_enable_interrupt()
{
    // 啟用 AUX 中斷 (位元 29)
    // uart_send_string("Enable AUX IRQ\r\n");
    *ENABLE_IRQS1 |= (1 << 29);
    // uart_send_hex((uint32_t)*AUX_MU_LCR_REG);
    // uart_send_string("\r\n");
    // uart_send_hex((uint32_t)*AUX_MU_IER_REG);
    // uart_send_string("\r\n");
    // uart_send_hex((uint32_t)*ENABLE_IRQS1);
    // uart_send_string("\r\n");
}
void uart_disable_interrupt()
{
    *ENABLE_IRQS1 &= ~(1 << 29);
}

// 檢查是否為 GPU 中斷
int is_gpu_interrupt()
{
    return *CORE0_INTERRUPT_SOURCE & GPU_INTERRUPTS;
}

// 接收数据的结构体
typedef struct
{
    char data[256]; // 存储接收到的数据
    int length;     // 数据长度
} uart_rx_data_t;

// UART接收数据处理函数
// UART接收数据处理函数
void process_uart_rx_data(void *arg)
{
    uart_rx_data_t *rx_data = (uart_rx_data_t *)arg;

    // 处理接收到的数据 - 将数据放入全局接收缓冲区
    for (int i = 0; i < rx_data->length; i++)
    {
        if (!is_buffer_full(rx_head, rx_tail))
        {
            buffer_push(rx_buffer, &rx_head, rx_data->data[i]);
        }
    }
    // 处理完成后释放内存
}

// UART发送数据处理函数
void process_uart_tx_data(void *arg)
{
    // 不需要其他参数，因为tx_buffer已经是全局的

    // 启用中断
    // enable_interrupts();

    // 持续发送数据直到发送缓冲区为空或UART TX FIFO满
    while (!is_buffer_empty(tx_head, tx_tail) && (*AUX_MU_LSR_REG & 0x20))
    {
        *AUX_MU_IO_REG = buffer_pop(tx_buffer, &tx_tail);
    }

    // 如果还有数据要发送，重新启用TX中断
    if (!is_buffer_empty(tx_head, tx_tail))
    {
        uart_enable_tx_interrupt();
    }

    // 关闭中断防止竞态条件
    // disable_interrupts();
}

// 修改后的UART中断处理程序
void uart_irq_handler()
{
    // 读取中断状态寄存器确定中断来源
    uint32_t iir = *AUX_MU_IIR_REG & 0x6;

    // 处理接收中断
    if (iir == 4)
    {
        // 关闭接收中断
        uart_disable_rx_interrupt();

        // 创建一个新的数据结构来存储接收到的数据
        uart_rx_data_t *rx_data = (uart_rx_data_t *)simple_alloc(sizeof(uart_rx_data_t));
        if (rx_data)
        {
            int count = 0;

            // 快速从UART缓冲区复制数据
            while ((*AUX_MU_LSR_REG & 0x01) && count < 256)
            {
                rx_data->data[count++] = (char)(*AUX_MU_IO_REG);
            }

            rx_data->length = count;

            // 将处理任务加入队列（优先级为1）
            enqueue_task(&global_task_queue, process_uart_rx_data, rx_data, 1);
        }

        // 重新启用接收中断
        // uart_enable_rx_interrupt();
    }

    // 处理发送中断
    if (iir == 2)
    {
        // 关闭发送中断
        uart_disable_tx_interrupt();

        // 将处理任务加入队列（优先级为1）
        enqueue_task(&global_task_queue, process_uart_tx_data, NULL, 1);
    }
}