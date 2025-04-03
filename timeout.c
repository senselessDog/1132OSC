#include <stdint.h>
#include "task_queue.h"
#include "uart.h"
#include "gpu_interrupt.h"
void process_timer_tasks(void *arg);
#ifndef NULL
#define NULL ((void *)0)
#endif
typedef struct
{
    char message[256];
    uint64_t execute_time;
} timeout_data_t;

// 計時器結構
typedef struct timer
{
    void (*callback)(void *data); // 回調函數
    void *data;                   // 回調函數的數據
    uint64_t expire_time;         // 過期時間
    struct timer *next;           // 下一個計時器
} timer_t;

// 全局計時器隊列
timer_t *timer_queue = NULL;

uint64_t get_timer_freq()
{
    uint64_t freq;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(freq));
    return freq;
}

// setTimeout 的回調函數
void timeout_callback(void *arg)
{
    timeout_data_t *data = (timeout_data_t *)arg;

    // 取得當前時間
    uint64_t current_time;
    asm volatile("mrs %0, cntpct_el0" : "=r"(current_time));

    // 顯示訊息與時間資訊
    uart_send_string(data->message);
    uart_send_string("\r\n");
    uart_send_string("(execute at ");
    uart_send_int((int)(data->execute_time / (get_timer_freq())));
    uart_send_string(", current time ");
    uart_send_int((int)(current_time / (get_timer_freq())));
    uart_send_string(")\r\n");
}

void add_timer(void (*callback)(void *data), void *data, uint64_t duration)
{

    // 禁止中斷保護臨界區
    asm volatile("msr DAIFSet, 0xf\n");

    // 創建新計時器
    timer_t *new_timer = (timer_t *)simple_alloc(sizeof(timer_t));
    new_timer->callback = callback;
    new_timer->data = data;

    // 獲取當前時間並計算過期時間
    uint64_t current_time;
    asm volatile("mrs %0, cntpct_el0" : "=r"(current_time));
    new_timer->expire_time = current_time + duration * (get_timer_freq()); // 轉換秒為計時器滴答

    // 按過期時間插入隊列
    if (timer_queue == NULL || new_timer->expire_time < timer_queue->expire_time)
    {
        // 如果是新的最早計時器，更新硬體計時器
        new_timer->next = timer_queue;
        timer_queue = new_timer;

        // 設置硬體計時器
        uint64_t timer_value = new_timer->expire_time - current_time;
        asm volatile("msr cntp_tval_el0, %0" ::"r"(timer_value));
        asm volatile("msr cntp_ctl_el0, %0" ::"r"(1)); // 啟用計時器
    }
    else
    {
        // 否則找到適當的位置插入
        timer_t *current = timer_queue;
        while (current->next != NULL && current->next->expire_time < new_timer->expire_time)
        {
            current = current->next;
        }
        new_timer->next = current->next;
        current->next = new_timer;
    }
    // 檢查並顯示輸入數據
    // timeout_data_t *tdata = (timeout_data_t *)data;
    // uart_send_string("[ADD_TIMER] Input execute_time = ");
    // uart_send_int((int)(tdata->execute_time / get_timer_freq()));
    // uart_send_string("\r\n");

    // 恢復中斷
    asm volatile("msr DAIFClr, 0xf\n");
}

void cmd_setTimeout(int argc, char **argv)
{
    int seconds = atoi(argv[2]);
    if (seconds <= 0)
    {
        uart_send_string("Error: second must be positive integer\r\n");
        return;
    }

    // 創建 timeout 數據
    timeout_data_t *data = (timeout_data_t *)simple_alloc(sizeof(timeout_data_t));
    strcpy(data->message, argv[1]);
    data->message[255] = '\0';
    // 記錄命令執行時間
    asm volatile("mrs %0, cntpct_el0" : "=r"(data->execute_time));

    // 添加計時器
    add_timer(timeout_callback, data, seconds);

    // uart_send_string("After ");
    // uart_send_int(seconds);
    // uart_send_string("sec show message\r\n");
}

void timer_interrupt_handler()
{
    // 檢查計時器隊列
    // 檢查是否有過期的定時器
    if (timer_queue != NULL)
    {
        uint64_t current_time;
        asm volatile("mrs %0, cntpct_el0" : "=r"(current_time));

        if (current_time >= timer_queue->expire_time)
        {
            // 關閉定時器中斷
            asm volatile("msr cntp_ctl_el0, %0" ::"r"(0));

            // 將處理任務加入任務佇列（高優先級2）
            enqueue_task(&global_task_queue, process_timer_tasks, NULL, 2);
        }
        else
        {
            // 設置下一個定時器
            uint64_t timer_value = timer_queue->expire_time - current_time;
            asm volatile("msr cntp_tval_el0, %0" ::"r"(timer_value));
            asm volatile("msr cntp_ctl_el0, %0" ::"r"(1)); // 啟用定時器
        }
    }
    // 如果沒有更多計時器，停用硬體計時器
    if (timer_queue == NULL)
    {
        asm volatile("msr cntp_ctl_el0, %0" ::"r"(0)); // 停用計時器
    }
}

// 定時器任務處理函數
void process_timer_tasks(void *arg)
{
    // 啟用中斷以允許嵌套
    enable_interrupts();

    uint64_t current_time;

    // 處理所有已過期的定時器
    while (timer_queue != NULL)
    {
        asm volatile("mrs %0, cntpct_el0" : "=r"(current_time));

        if (current_time >= timer_queue->expire_time)
        {
            // 計時器已過期，從佇列中移除
            timer_t *expired = timer_queue;
            timer_queue = timer_queue->next;

            // 執行回調
            if (expired->callback)
            {
                expired->callback(expired->data);
            }
        }
        else
        {
            // 沒有更多過期的定時器
            break;
        }
    }

    // 設置下一個定時器（如果有）
    if (timer_queue != NULL)
    {
        asm volatile("mrs %0, cntpct_el0" : "=r"(current_time));
        uint64_t timer_value = timer_queue->expire_time - current_time;
        asm volatile("msr cntp_tval_el0, %0" ::"r"(timer_value));
        asm volatile("msr cntp_ctl_el0, %0" ::"r"(1)); // 啟用定時器
    }

    // 關閉中斷
    disable_interrupts();
}