// #include <stdio.h>  // Required for sprintf
// #include <string.h> // Required for strlen
#include <stddef.h>
// Assume delay is available or implement it elsewhere (e.g., busy loop)
// void delay(volatile unsigned int count) { while(count--); }
void fork_test();
void _start() {
  fork_test();
}
int strlen(const char *str) {
    int len = 0;
    while (str[len]!='\0') len++;
    return len;
}
char *strcpy(char *dest, const char *src) {
    char *ptr = dest;
    while (*src != '\0') {
        *ptr++ = *src++;
    }
    *ptr = '\0'; // 確保目標字符串以 '\0' 結尾
    return dest;
}
// 實現簡單的數字轉字符串
void int_to_str(int num, char *str) {
    int i = 0;
    int is_neg = 0;
    
    if (num < 0) {
        is_neg = 1;
        num = -num;
    }
    
    do {
        str[i++] = '0' + (num % 10);
        num /= 10;
    } while (num > 0);
    
    if (is_neg) {
        str[i++] = '-';
    }
    
    // 反轉字符串
    for (int j = 0; j < i/2; j++) {
        char temp = str[j];
        str[j] = str[i-1-j];
        str[i-1-j] = temp;
    }
    str[i] = '\0';
}

// 實現簡單的十六進制轉字符串
void hex_to_str(unsigned long long num, char *str) {
    const char hex_chars[] = "0123456789ABCDEF";
    int i = 0;
    
    str[i++] = '0';
    str[i++] = 'x';
    
    if (num == 0) {
        str[i++] = '0';
    } else {
        while (num > 0) {
            str[i++] = hex_chars[num & 0xF];
            num >>= 4;
        }
    }
    
    // 反轉十六進制部分
    for (int j = 2; j < (i+2)/2; j++) {
        char temp = str[j];
        str[j] = str[i-1-(j-2)];
        str[i-1-(j-2)] = temp;
    }
    str[i] = '\0';
}

// Helper function for get_pid system call (syscall 0)
int sys_get_pid() {
    int pid;
    asm volatile(
        "mov x8, 0 \n"  // syscall number 0 for getpid
        "svc 0     \n"
        "mov %0, x0\n"  // return value in x0
        : "=r"(pid)     // output
        :               // no inputs
        : "x0", "x8"    // clobbered registers
    );
    return pid;
}

// Helper function for fork system call (syscall 4)
int sys_fork() {
    int ret;
    asm volatile(
        "mov x8, 4 \n"  // syscall number 4 for fork
        "svc 0     \n"
        "mov %0, x0\n"  // return value in x0
        : "=r"(ret)     // output
        :               // no inputs
        : "x0", "x8"    // clobbered registers
    );
    return ret;
}

// Helper function for exit system call (syscall 5)
// Takes an exit status code as argument
void sys_exit(int status) {
    asm volatile(
        "mov x0, %0\n"  // argument 0: status code
        "mov x8, 5 \n"  // syscall number 5 for exit
        "svc 0     \n"
        :               // no outputs
        : "r"(status)   // input: status
        : "x0", "x8"    // clobbered registers
    );
    // This function should not return
    while(1); // Loop indefinitely if svc doesn't terminate
}

// Helper function for uart_write system call (syscall 2)
// Returns number of bytes written
int sys_uart_write(const char *buf, unsigned long size) {
    int bytes_written;
    asm volatile(
        "mov x0, %1\n"  // argument 0: buffer address
        "mov x1, %2\n"  // argument 1: size
        "mov x8, 2 \n"  // syscall number 2 for uart_write
        "svc 0     \n"
        "mov %0, x0\n"  // return value in x0
        : "=r"(bytes_written) // output
        : "r"(buf), "r"(size) // inputs: buffer, size
        : "x0", "x1", "x8"    // clobbered registers
    );
    return bytes_written;
}

// Helper function to print strings via uart_write
void print_str(const char *str) {
    sys_uart_write(str, strlen(str));
}

void fork_test() {
    char buffer[512]; // Buffer for formatting strings
    int pid = sys_get_pid();
    
    // 打印初始信息
    print_str("Fork Test, pid ");
    int_to_str(pid, buffer);
    print_str(buffer);
    print_str("\r\n");

    int cnt = 1;
    int ret = 0;

    ret = sys_fork();

    if (ret == 0) { // Child process
        long long cur_sp;
        asm volatile("mov %0, sp" : "=r"(cur_sp));
        pid = sys_get_pid();
        
        // 打印第一個子進程信息
        print_str("first child pid: ");
        int_to_str(pid, buffer);
        print_str(buffer);
        print_str(", cnt: ");
        int_to_str(cnt, buffer);
        print_str(buffer);
        print_str(", ptr: ");
        hex_to_str((unsigned long long)&cnt, buffer);
        print_str(buffer);
        print_str(", sp: ");
        hex_to_str(cur_sp, buffer);
        print_str(buffer);
        print_str("\r\n");
        
        ++cnt;

        ret = sys_fork(); // Fork again

        if (ret != 0) { // First child (now parent of second child)
            asm volatile("mov %0, sp" : "=r"(cur_sp));
            pid = sys_get_pid();
            
            // 打印第一個子進程（現在是第二個子進程的父進程）的信息
            print_str("first child (parent) pid: ");
            int_to_str(pid, buffer);
            print_str(buffer);
            print_str(", cnt: ");
            int_to_str(cnt, buffer);
            print_str(buffer);
            print_str(", ptr: ");
            hex_to_str((unsigned long long)&cnt, buffer);
            print_str(buffer);
            print_str(", sp: ");
            hex_to_str(cur_sp, buffer);
            print_str(buffer);
            print_str("\r\n");
            // print_str(", child_pid: ");
            // int_to_str(ret, buffer);
            // print_str(buffer);
            // print_str("\r\n");

        } else { // Second child process
            while (cnt < 5) {
                asm volatile("mov %0, sp" : "=r"(cur_sp));
                pid = sys_get_pid();
                
                // 打印第二個子進程的信息
                print_str("second child pid: ");
                int_to_str(pid, buffer);
                print_str(buffer);
                print_str(", cnt: ");
                int_to_str(cnt, buffer);
                print_str(buffer);
                print_str(", ptr: ");
                hex_to_str((unsigned long long)&cnt, buffer);
                print_str(buffer);
                print_str(", sp: ");
                hex_to_str(cur_sp, buffer);
                print_str(buffer);
                print_str("\r\n");
                
                // Simple delay
                for(int j = 0; j < 1000000; j++) {
                    asm volatile("nop");
                }
                ++cnt;
            }
        }
        sys_exit(0); // Children exit

    } else { // Original parent process
        long long cur_sp;
        asm volatile("mov %0, sp" : "=r"(cur_sp));
        
        // 打印父進程信息
        print_str("parent here, pid ");
        int_to_str(pid, buffer);
        print_str(buffer);
        print_str(", child ");
        int_to_str(ret, buffer);
        print_str(buffer);
        print_str("\r\n");
        sys_exit(0);
    }
}