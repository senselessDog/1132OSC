#define TASK_NOT_STARTED 0
#define TASK_RUNNING 1
#define TASK_COMPLETED 2

typedef struct task
{
    void (*callback)(void *); // 函数指针，指向任务处理函数
    void *data;               // 任务的数据
    int priority;             // 优先级（值越大优先级越高）
    struct task *next;        // 指向队列中下一个任务
} task_t;

// Task queue
typedef struct
{
    task_t *head; // Head of the queue
    task_t *tail; // Tail of the queue
    int size;     // Current size of the queue
} task_queue_t;

extern task_queue_t global_task_queue;
void task_queue_init(task_queue_t *queue);
void process_task_queue();
void enqueue_task(task_queue_t *queue, void (*callback)(void *), void *data, int priority);
task_t *get_highest_priority_task(task_queue_t *queue);