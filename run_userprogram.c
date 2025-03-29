void* find_program_in_initramfs(char *archive, const char *filename);
void uart_send_string(const char *str);
void uart_send(char c);
char uart_recv();
void switch_to_el0(void* start_addr, void* stack_ptr) {
    //perpare for this function
    asm volatile(
        "msr spsr_el1, %0\n"
        "msr elr_el1, %1\n"
        "msr sp_el0, %2\n"
        "eret\n"
        :: "r" (0x3c0), "r" (start_addr), "r" (stack_ptr)
    );
}

void run_user(char *archive){
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
    
    void* start_addr = find_program_in_initramfs(archive, filename);
    if (!start_addr) {
        uart_send_string("Program not found: ");
        uart_send_string(filename);
        uart_send_string("\r\n");
        return;
    }
    // Allocate user stack (4KB)
    // For bare metal, we'll use a static buffer instead of malloc
    static char user_stack[4096] __attribute__((aligned(16)));
    
    // Stack pointer should point to the top because stack grows downward
    void* stack_top = user_stack + 4096;
    
    uart_send_string("Executing program: ");
    uart_send_string(filename);
    switch_to_el0(start_addr, stack_top);
}