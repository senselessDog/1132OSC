#include <stdint.h>
#include <string.h>

void uart_init();
void uart_send_string(const char *str);
char uart_recv();
void uart_send(char c);
void get_board_revision();
void get_arm_memory();
void uart_send_hex(uint32_t value);

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
    uart_send_string("Core ID: 0x");
    uart_send_hex((uint32_t)core_id);
    uart_send_string("\r\n");
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

            if (strcmp(buffer, "help") == 0)
            {
                uart_send_string("Available commands:\r\n");
                uart_send_string("help - print all available commands\r\n");
                uart_send_string("hello - print Hello World!\r\n");
                uart_send_string("modinfo - get board revision & ARM memory information\r\n");
                uart_send_string("boardrev - get board revision\r\n");
                uart_send_string("armmem - get ARM memory information\r\n");
                uart_send_string("coreid - print current core ID\r\n");
            }
            else if (strcmp(buffer, "hello") == 0)
            {
                uart_send_string("Hello World!\r\n");
            }
            else if (strcmp(buffer, "modinfo") == 0)
            {
                get_board_revision();
                get_arm_memory();
            }
            else if (strcmp(buffer, "boardrev") == 0)
            {
                get_board_revision();
            }
            else if (strcmp(buffer, "armmem") == 0)
            {
                get_arm_memory();
            }
            else if (strcmp(buffer, "coreid") == 0)
            {
                print_core_id();
            }
            else
            {
                uart_send_string("Unknown command: ");
                uart_send_string(buffer);
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
                uart_send_string("\b \b"); // Handle backspace
            }
        }
        else
        {
            buffer[index++] = c;
        }
    }
}

void kernel_main()
{
    uart_init();
    print_core_id(); // 在 shell 啟動前打印核心 ID
    shell();
    while (1)
    {
        // Loop indefinitely
    }
}