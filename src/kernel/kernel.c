#include <stdint.h>
#include <string.h>
#include "uart.h"
#include "gpu_interrupt.h"
#include "task_queue.h"
#include "thread.h"
#include "alloc.h"
void get_board_revision();
void get_arm_memory();
// lab2
void reset(int tick);
void parse_cpio_archive(char *archive);
void list_cpio_files(const char *archive);
void uart_send_int(int value);
int fdt_init(void *fdt_addr);
// lab3
void *run_user(char *archive);
void cmd_setTimeout(int argc, char **argv);
void async_io_test();
// 從 DTB 中獲取 initramfs 信息的全局變量
extern uint64_t g_initramfs_addr;
extern uint64_t g_initramfs_size;
// 從 DTB 中獲取 initramfs 信息的函數
int get_initramfs_info(void *dtb_addr);
/**
 * @brief Prints the core ID of the current CPU core.
 *
 * This function retrieves the core ID from the MPIDR_EL1 register,
 * masks the relevant bits to extract the core ID, and then sends
 * the core ID as a hexadecimal string via UART.
 *
 * The core ID is extracted from bits 0:1 of the MPIDR_EL1 register.
 *
 */
void print_core_id()
{
    uint64_t core_id;
    asm volatile("mrs %0, mpidr_el1" : "=r"(core_id));
    core_id &= 0xFF; // 取出核心 ID (bits 0:1)
    uart_send_string("Below Core list -----\r\n");
    uart_send_string("Core ID: 0x");
    uart_send_hex((uint32_t)core_id);
    uart_send_string("\r\n");
    uart_send_string("Core list -----\r\n");
}
// 將輸入緩衝區分割成多個參數
void parse_arguments(char *buffer, char *argv[], int *argc)
{
    *argc = 0;
    char *token = strtok(buffer, " ");

    while (token != NULL && *argc < 10)
    { // 限制最多 10 個參數
        argv[(*argc)++] = token;
        token = strtok(NULL, " ");
    }
}
void shell()
{
    char buffer[256];
    int index = 0;

    uart_send_string("Welcome to the simple shell!\r\n");
    uart_send_string("> ");

    while (1)
    {
        char c = uart_recv();
        uart_send(c); // Echo the received character

        if (c == '\r' || c == '\n')
        {
            uart_send_string("\r\n");

            buffer[index] = '\0'; // Null-terminate the string
            // Prevent the empty input
            if (index == 0)
            {
                uart_send_string("> ");
                continue;
            }

            // 解析參數
            char *argv[10];
            int argc = 0;
            parse_arguments(buffer, argv, &argc);

            if (argc == 0)
            {
                uart_send_string("> ");
                continue;
            }

            if (strcmp(argv[0], "help") == 0)
            {
                uart_send_string("Available commands:\r\n");
                uart_send_string("help - print all available commands\r\n");
                uart_send_string("hello - print Hello World!\r\n");
                uart_send_string("modinfo - get board revision & ARM memory information\r\n");
                uart_send_string("boardrev - get board revision\r\n");
                uart_send_string("armmem - get ARM memory information\r\n");
                uart_send_string("coreid - print current core ID\r\n");
                // lab2
                uart_send_string("reboot - reboot the system\r\n");
                uart_send_string("ls - list the filename\r\n");
                uart_send_string("cat -show the file context\r\n");
                uart_send_string("simple_alloc <size> - allocate memory of given size\r\n");
                // lab3
                uart_send_string("run <user program> - run user program on EL0\r\n");
                uart_send_string("async_io - run async uart I/O test\r\n");
                uart_send_string("setTimeout <message> <seconds> - display message after specified seconds\r\n");
                //lab4
                uart_send_string("mem_alloc <size> - allocate memory using buddy allocator\r\n");
                uart_send_string("free <address> -free memory using buddy allocator\r\n");
            }
            else if (strcmp(argv[0], "hello") == 0)
            {
                uart_send_string("Hello World!\r\n");
            }
            else if (strcmp(argv[0], "modinfo") == 0)
            {
                get_board_revision();
                get_arm_memory();
            }
            else if (strcmp(argv[0], "boardrev") == 0)
            {
                get_board_revision();
            }
            else if (strcmp(argv[0], "armmem") == 0)
            {
                get_arm_memory();
            }
            else if (strcmp(argv[0], "coreid") == 0)
            {
                print_core_id();
            }
            else if (strcmp(argv[0], "reboot") == 0)
            {
                reset(10); // 10 ticks
            }
            else if (strcmp(argv[0], "ls") == 0)
            {
                list_cpio_files((char *)g_initramfs_addr);
                uart_send_string("\r\n");
            }
            else if (strcmp(argv[0], "cat") == 0)
            {
                parse_cpio_archive((char *)g_initramfs_addr);
                uart_send_string("\r\n");
            }
            else if (strcmp(argv[0], "simple_alloc") == 0 && argc > 1)
            {
                size_t size = strtol(argv[1], NULL, 10);
                uart_send_int((int)size);
                uart_send_string("\r\n");
                void *ptr = simple_alloc(size);
                if (ptr != NULL)
                {
                    uart_send_string("Memory allocated at address: ");
                    uart_send_hex((uint32_t)ptr);
                    uart_send_string("\r\n");
                }
            }
            else if (strcmp(argv[0], "run") == 0)
            {
                uart_send_string("run user program on EL0\r\n");
                // run_user((char *)g_initramfs_addr);
                run_user_vm((char *)g_initramfs_addr);
            }
            else if (strcmp(argv[0], "async_io") == 0)
            {
                uart_send_string("Async I/O test\r\n");
                async_io_test();
            }
            // 添加 setTimeout 命令
            else if (strcmp(argv[0], "setTimeout") == 0)
            {
                if (argc != 3)
                {
                    uart_send_string("Usage: setTimeout <message> <seconds>\r\n");
                }
                else
                {
                    // 調用 cmd_setTimeout 函數處理命令
                    cmd_setTimeout(argc, argv);
                }
            }
            else if (strcmp(argv[0], "mem_alloc") == 0)
            {
                size_t size = strtol(argv[1], NULL, 10);
                void * ptr=dynamic_malloc(size);
                uart_send_string("[main] Memory allocated at address: ");
                uart_send_hex((uint32_t)ptr);
                uart_send_string("\r\n");
                //break; // Exit the shell loop
            }
            else if (strcmp(argv[0], "free") == 0)
            {
                if (argc != 2)
                {
                    uart_send_string("Usage: free <address>\r\n");
                }
                else
                {
                    void *addr = (void *)strtol(argv[1], NULL, 16);
                    dynamic_free(addr);
                    uart_send_string("[main] Memory freed at address: ");
                    uart_send_hex((uint32_t)addr);
                    uart_send_string("\r\n");
                }
            }
            else
            {
                uart_send_string("Unknown command: ");
                uart_send_string(argv[0]);
                uart_send_string("\r\n");
            }

            index = 0;
            uart_send_string("> ");
        }
        /**
         * Handles backspace or delete character input.
         * If the character `c` is a backspace (`\b`) or delete (ASCII 127),
         * and there are characters to delete (index > 0), it decrements the index
         * and sends the backspace sequence to the UART to remove the character
         * from the display.
         *
         * @param c The character input to handle.
         * @param index The current position in the input buffer.
         */
        else if (c == '\b' || c == 127)
        {
            if (index > 0)
            {
                index--;
                uart_send_string(" \b"); // Handle backspace
            }
        }
        else
        {
            buffer[index++] = c;
        }
    }
}

void kernel_main(void *dtb_addr)
{
    // uart_init();
    print_core_id(); // 在 shell 啟動前打印核心 ID
    uart_send_hex((uint32_t)dtb_addr);
    uart_send_string("\r\n");
    uart_send_string("[main] start DTB\r\n");
    // Get the DTB address from x0 register
    // 初始化 DTB 解析器
    if (fdt_init(dtb_addr) != 0)
    {
        uart_send_string("Failed to initialize DTB parser\r\n");
    }
    //uart_send_string("[main] get_initramfs \r\n");
    // 獲取 initramfs 地址和大小
    get_initramfs_info(dtb_addr);
    //uart_send_string("[main] Finish DTB\r\n");
    
    //interrupt task init
    task_queue_init(&global_task_queue);
    
    // Enable interrupts
    uart_enable_interrupt();
    enable_interrupts();
    
    // Initialize memory allocator
    buddy_init();
    init_dynamic_allocator();
    // thread_init();
    // // create multiple threads
    // for(int i = 0; i < 3; i++) {
    //     thread_create(thread_test,NULL);
    // }
    
    // // Start idle thread
    // idle();
    uart_send_string("[main] start shell\r\n");
    shell();
    uart_send_string("[main] shell error\r\n");
    while (1)
    {
        // Loop indefinitely
    }
}