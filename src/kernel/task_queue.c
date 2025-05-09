#ifndef NULL
#define NULL ((void *)0)
#endif
#include "task_queue.h"
#include <stdint.h>
// Global task queue
task_queue_t global_task_queue;
// 全局變數跟踪當前執行的任務
task_t *current_task = NULL;

void task_queue_init(task_queue_t *queue)
{
    queue->head = NULL;
    queue->tail = NULL;
    queue->size = 0;
}
void enqueue_task(task_queue_t *queue, void (*callback)(void *), void *data, int priority)
{
    // 关闭中断以保护队列
    disable_interrupts();

    // 为新任务分配内存
    task_t *new_task = (task_t *)simple_alloc(sizeof(task_t));
    if (!new_task)
    {
        // 内存分配失败
        enable_interrupts();
        return;
    }

    new_task->callback = callback;
    new_task->data = data;
    new_task->priority = priority;
    new_task->next = NULL;

    // 添加到队列
    if (queue->head == NULL)
    {
        // 空队列
        queue->head = new_task;
        queue->tail = new_task;
    }
    else
    {
        // 添加到队尾
        queue->tail->next = new_task;
        queue->tail = new_task;
    }

    queue->size++;

    // 恢复中断
    // enable_interrupts();
}
void process_task_queue()
{
    // 启用中断以允许嵌套中断
    // uint64_t spsr;
    // uart_send_string("spsr_el1\r\n");
    // asm volatile("mrs %0, spsr_el1" : "=r"(spsr));
    // uart_send_hex(spsr);
    // uart_send_string("\r\n");
    extern int should_run_task_queue;
    if (should_run_task_queue == 0)
    {
        return;
    }
    enable_interrupts();
    // uart_send_string("spsr_el1\r\n");
    // asm volatile("mrs %0, spsr_el1" : "=r"(spsr));
    // uart_send_hex(spsr);
    // uart_send_string("\r\n");

    // 处理队列中的所有任务
    task_t *task;
    while ((task = get_highest_priority_task(&global_task_queue)) != NULL)
    {
        // 执行任务
        task->callback(task->data);
        task = NULL;
    }
    // uint64_t spsr;
    // uart_send_string("spsr_el1\r\n");
    // asm volatile("mrs %0, spsr_el1" : "=r"(spsr));
    // uart_send_hex(spsr);
    // uart_send_string("\r\n");
    // 防止竞态条件
    disable_interrupts();
    
}
// Get the highest priority task from the queue
task_t *get_highest_priority_task(task_queue_t *queue)
{
    disable_interrupts();

    if (queue->head == NULL)
    {
        enable_interrupts();
        return NULL; // 队列为空
    }

    // 查找优先级最高的任务
    task_t *current = queue->head;
    task_t *highest = current;
    task_t *prev = NULL;
    task_t *highest_prev = NULL;

    while (current != NULL)
    {
        if (current->priority > highest->priority)
        {
            highest = current;
            highest_prev = prev;
        }
        prev = current;
        current = current->next;
    }

    // 从队列中移除最高优先级任务
    if (highest_prev == NULL)
    {
        // 最高优先级任务在队头
        queue->head = highest->next;
    }
    else
    {
        highest_prev->next = highest->next;
    }

    // 如果需要，更新尾指针
    if (highest == queue->tail)
    {
        queue->tail = highest_prev;
    }

    queue->size--;
    enable_interrupts();
    return highest;
}