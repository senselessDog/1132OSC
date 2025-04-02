#include <stdint.h>
#include <stddef.h>

struct file_information find_program_in_initramfs(char *archive, const char *filename);
void uart_send_string(const char *str);
void uart_send(char c);
char uart_recv();

#define USER_PROGRAM_BASE 0x10000000
#define USER_STACK_POINTER_BASE 0x20000000

struct file_information
{
    void *filecontext;
    int filesize;
};
#define CORE0_TIMER_IRQ_CTRL 0x40000040

void core_timer_enable(void)
{
    // mov x0, 1
    // msr cntp_ctl_el0, x0 // enable
    asm volatile("mov x0, #1");
    asm volatile("msr cntp_ctl_el0, x0");

    // mrs x0, cntfrq_el0
    // msr cntp_tval_el0, x0 // set expired time
    asm volatile("mrs x0, cntfrq_el0");
    asm volatile("msr cntp_tval_el0, x0");

    // mov x0, 2
    // ldr x1, =CORE0_TIMER_IRQ_CTRL
    // str w0, [x1] // unmask timer interrupt
    uint32_t *ctrl_reg = (uint32_t *)CORE0_TIMER_IRQ_CTRL;
    *ctrl_reg = 2;
}
void switch_to_el0(void *start_addr, void *stack_ptr)
{
    // perpare for this function
    core_timer_enable();
    asm volatile(
        "msr spsr_el1, %0\n"
        "msr elr_el1, %1\n"
        "msr sp_el0, %2\n"
        "eret\n" ::"r"(0x0),
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
    uart_send_string("\r\n");
    switch_to_el0((void *)USER_PROGRAM_BASE, (void *)USER_STACK_POINTER_BASE);
}