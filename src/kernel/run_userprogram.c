#include <stdint.h>
#include <stddef.h>
#include "strcmp.h"
#include "thread.h"
struct file_information find_program_in_initramfs(char *archive, const char *filename);
void uart_send_string(const char *str);
void uart_send(char c);
char uart_recv();
void * user_stack;
#define USER_PROGRAM_BASE 0x20000000
#define USER_STACK_POINTER_BASE 0x21000000
uint32_t user_space_size=1048576;
struct file_information
{
    void *filecontext;
    int filesize;
};
static int first_thread=1;
#define CORE0_TIMER_IRQ_CTRL 0xFFFF000040000040
void timer_disable(void){
    //disable time interrupt
    asm volatile("mov x0, #0");
    asm volatile("msr cntp_ctl_el0, x0");
}
void el0_core_timer_enable(void)
{
    // mov x0, 1
    // msr cntp_ctl_el0, x0 // enable
    

    // mrs x0, cntfrq_el0
    // msr cntp_tval_el0, x0 // set expired time
    uint64_t timer_freq;
    asm volatile("mrs %0, cntfrq_el0" : "=r"(timer_freq));
    uint64_t timeout=timer_freq>>5;
    asm volatile("msr cntp_tval_el0, %0" : : "r"(timeout));
    
    // unmask timer interrupt
    // mov x0, 2
    // ldr x1, =CORE0_TIMER_IRQ_CTRL
    // str w0, [x1] 
    uint32_t *ctrl_reg = (volatile uint32_t *)CORE0_TIMER_IRQ_CTRL;
    *ctrl_reg = 2;
    //Enable timer access for EL0
    uint64_t tmp;
    asm volatile("mrs %0, cntkctl_el1" : "=r"(tmp));
    tmp |= 1;// Allow EL0 to access physical timer
    asm volatile("msr cntkctl_el1, %0" : : "r"(tmp));
    //enable time interrupt
    asm volatile("mov x0, #1");
    asm volatile("msr cntp_ctl_el0, x0");
    
}
void switch_to_el0(void *start_addr, void *stack_ptr)
{
    // perpare for this function
    // el0_core_timer_enable();
    uart_send_string("switch_to_el0\r\n");
    uart_send_string("start_addr: ");
    uart_send_hex(start_addr);
    uart_send_string("\r\n");
    uart_send_string("stack_ptr: ");
    uart_send_hex(stack_ptr);
    uart_send_string("\r\n");
    asm volatile(
        "msr spsr_el1, %0\n"
        "msr elr_el1, %1\n"
        "msr sp_el0, %2\n"
         ::"r"(0x0),
        "r"(start_addr), "r"(stack_ptr));
    asm volatile("eret\n");
    
}

void *run_user(char *archive)
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
    uart_send_string("[run user] archive: ");
    uart_send_hex((uint32_t)archive);
    uart_send_string("\r\n");
    uart_send_string("[run user] filename: ");
    uart_send_string(filename);
    uart_send_string("\r\n");

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
    uart_send_string("Executing program: ");
    uart_send_string(filename);
    uart_send_string("\r\n");
    uart_send_string("program_info.filesize: ");
    uart_send_hex(program_info.filesize);
    uart_send_string("\r\n");
    // Stack pointer should point to the top because stack grows downward
    //Warning: Please take carefully about program size, if the 
    if (user_space_size<=program_info.filesize){
        uart_send_string("Error: [run_program] user_space size lower than program size");
        uart_send_string("\r\n");
    }
    void * user_base=dynamic_malloc(user_space_size);
    // uart_send_string("Create parent thread: ");
    // thread_t * parent_thread=thread_create(user_base,NULL);
    // uart_send_string("Create parent thread success\r\n");
    user_stack=user_base+user_space_size;
    
    memcpy(user_base,(const void *)program_info.filecontext, (uint32_t)program_info.filesize);
    // program_info = find_program_in_initramfs(archive, filename);
    //init user thread
    if (first_thread){
        thread_init_user(0,0);
        
    }
    uart_send_string("user_base: ");
    uart_send_hex(user_base);
    uart_send_string("\r\n");
    uart_send_string("user_stack: ");
    uart_send_hex(user_stack);
    uart_send_string("\r\n");
    
    if (first_thread){
        first_thread=0;
        switch_to_el0(user_base, user_stack);
    }
    // uart_send_string("Parent thread start\r\n");
    //asm volatile("eret\n");
    // idle();
    return user_base;
}