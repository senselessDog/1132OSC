#include <stdint.h>
#include <string.h>
void uart_init();
void uart_send_string(const char *str);
char uart_recv();
void uart_send(char c);

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
            }
            else if (strcmp(buffer, "hello") == 0)
            {
                uart_send_string("Hello World!\r\n");
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
    shell();
    while (1)
    {
        // Loop indefinitely
    }
}