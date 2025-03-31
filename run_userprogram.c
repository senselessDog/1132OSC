#include <stdint.h>
#include <stddef.h>

struct file_information find_program_in_initramfs(char *archive, const char *filename);
void uart_send_string(const char *str);
void uart_send(char c);
char uart_recv();

#define USER_PROGRAM_BASE 0x160000
#define USER_STACK_POINTER_BASE 0x200000

struct file_information
{
    void *filecontext;
    int filesize;
};
void switch_to_el0(void *start_addr, void *stack_ptr)
{
    // perpare for this function
    asm volatile(

        "msr spsr_el1, %0\n"
        "msr elr_el1, %1\n"
        "msr sp_el0, %2\n"
        "eret\n" ::"r"(0x3c0),
        "r"(start_addr), "r"(stack_ptr));
}

void run_user(char *archive)
{
    char filename[1024];
    int index = 0;
    // Receive the filename from the user
    uart_send_string("\r\n");
    uart_send_string("Filename: ");
    while (1)
    {

        char c = uart_recv();
        uart_send(c); // Echo the received character
        if (c == '\r' || c == '\n')
        {
            filename[index] = '\0'; // Null-terminate the string
            index = 0;
            uart_send_string("\r\n");
            break;
        }
        else
        {
            filename[index] = c;
            index++;
        }
    }

    struct file_information program_info = find_program_in_initramfs(archive, filename);
    if (!program_info.filecontext)
    {
        uart_send_string("Program not found: ");
        uart_send_string(filename);
        uart_send_string("\r\n");
        return;
    }
    // Allocate user stack (4KB)
    // For bare metal, we'll use a static buffer instead of malloc
    // static char user_stack[4096] __attribute__((aligned(16)));

    // Stack pointer should point to the top because stack grows downward
    void *user_program = (void *)USER_PROGRAM_BASE;
    memcpy(user_program, program_info.filecontext, (uint32_t)program_info.filesize);

    uart_send_string("Executing program: ");
    uart_send_string(filename);
    switch_to_el0((void *)USER_PROGRAM_BASE, (void *)USER_STACK_POINTER_BASE);
}