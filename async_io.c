#include "uart.h"
#include <stdint.h>
#include <string.h>
#include <stdio.h>
void async_io_test()
{
    char buffer[256] = {0};
    int count = 0;

    // 啟用 UART 中斷
    uart_enable_interrupt();
    uart_enable_rx_interrupt();
    *AUX_MU_IER_REG |= (3 << 2);

    // 顯示歡迎訊息
    uart_async_send_string("\r\n===== UART Asynchronous I/O Test =====\r\n");

    while (1)
    {
        // 顯示提示符
        uart_async_send_string("\r\n> ");

        // 清空緩衝區
        for (int i = 0; i < 256; i++)
        {
            buffer[i] = 0;
        }

        // 讀取命令直到按下 Enter
        char c = 0;
        int pos = 0;

        while (c != '\r' && c != '\n')
        {
            // 非阻塞等待數據
            if (uart_async_recv(&c))
            {
                // 回顯字符
                uart_async_send(c);

                // 存儲字符
                if (c != '\r' && c != '\n')
                {
                    buffer[pos++] = c;
                }
            }

            // 簡單延遲（模擬其他任務）
            for (int i = 0; i < 500000; i++)
            {
                asm volatile("nop");
            }

            // 每隔一段時間顯示計數器（演示其他任務可以並行執行）
            count++;
            if (count % 20 == 0)
            {
                char counter[32];
                uart_async_send_string("\r\n[Background task: ");
                uart_async_send_string(int2str(count));
                uart_async_send_string("\r\n> ");
                uart_async_send_string(buffer);
            }
        }

        uart_async_send_string("\r\n");

        // 處理命令
        if (strcmp(buffer, "exit") == 0)
        {
            uart_async_send_string("Exiting test...\r\n");
            break;
        }
        else if (strcmp(buffer, "help") == 0)
        {
            uart_async_send_string("Available commands:\r\n");
            uart_async_send_string("  help - Show this help\r\n");
            uart_async_send_string("  echo [text] - Echo back text\r\n");
            uart_async_send_string("  exit - Exit the test\r\n");
        }
        else if (strncmp(buffer, "echo ", 5) == 0)
        {
            uart_async_send_string("Echo: ");
            uart_async_send_string(buffer + 5);
            uart_async_send_string("\r\n");
        }
        else if (strlen(buffer) > 0)
        {
            uart_async_send_string("Unknown command: ");
            uart_async_send_string(buffer);
            uart_async_send_string("\r\n");
        }
    }

    // 禁用中斷
    uart_disable_tx_interrupt();
    uart_disable_rx_interrupt();
}