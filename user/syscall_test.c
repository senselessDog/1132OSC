#include <stdint.h>

// System call numbers
#define SYS_GETPID      0
#define SYS_UART_READ   1
#define SYS_UART_WRITE  2
#define SYS_EXEC        3
#define SYS_FORK        4
#define SYS_EXIT        5
#define SYS_MBOX_CALL   6
#define SYS_KILL        7

// System call wrapper functions
static inline int getpid() {
    int ret;
    asm volatile("mov x8, %1\n"
                 "svc #0\n"
                 "mov %0, x0"
                 : "=r"(ret)
                 : "i"(SYS_GETPID)
                 : "x8");
    return ret;
}

static inline size_t uart_read(char buf[], size_t size) {
    size_t ret;
    asm volatile("mov x8, %2\n"
                 "svc #0\n"
                 "mov %0, x0"
                 : "=r"(ret)
                 : "r"(buf), "i"(SYS_UART_READ)
                 : "x8");
    return ret;
}

static inline size_t uart_write(const char buf[], size_t size) {
    size_t ret;
    asm volatile("mov x8, %2\n"
                 "svc #0\n"
                 "mov %0, x0"
                 : "=r"(ret)
                 : "r"(buf), "i"(SYS_UART_WRITE)
                 : "x8");
    return ret;
}

static inline int exec(const char* name, char* const argv[]) {
    int ret;
    asm volatile("mov x8, %2\n"
                 "svc #0\n"
                 "mov %0, x0"
                 : "=r"(ret)
                 : "r"(name), "i"(SYS_EXEC)
                 : "x8");
    return ret;
}

static inline int fork() {
    int ret;
    asm volatile("mov x8, %1\n"
                 "svc #0\n"
                 "mov %0, x0"
                 : "=r"(ret)
                 : "i"(SYS_FORK)
                 : "x8");
    return ret;
}

static inline void exit(int status) {
    asm volatile("mov x8, %1\n"
                 "svc #0"
                 :
                 : "r"(status), "i"(SYS_EXIT)
                 : "x8");
}

static inline int mbox_call(unsigned char ch, unsigned int* mbox) {
    int ret;
    asm volatile("mov x8, %2\n"
                 "svc #0\n"
                 "mov %0, x0"
                 : "=r"(ret)
                 : "r"(ch), "r"(mbox), "i"(SYS_MBOX_CALL)
                 : "x8");
    return ret;
}

static inline int kill(int pid) {
    int ret;
    asm volatile("mov x8, %2\n"
                 "svc #0\n"
                 "mov %0, x0"
                 : "=r"(ret)
                 : "r"(pid), "i"(SYS_KILL)
                 : "x8");
    return ret;
}

void main() {
    char buf[256];
    int pid = getpid();
    
    // Test uart_write
    const char* msg = "Hello from user program! PID: ";
    uart_write(msg, 30);
    
    // Convert PID to string
    int i = 0;
    int temp = pid;
    do {
        buf[i++] = '0' + (temp % 10);
        temp /= 10;
    } while (temp > 0);
    
    // Reverse the string
    for (int j = 0; j < i/2; j++) {
        char t = buf[j];
        buf[j] = buf[i-j-1];
        buf[i-j-1] = t;
    }
    buf[i] = '\n';
    buf[i+1] = '\0';
    
    uart_write(buf, i+2);
    
    // Test uart_read
    uart_write("Enter a message: ", 17);
    size_t len = uart_read(buf, 256);
    uart_write("You entered: ", 13);
    uart_write(buf, len);
    
    // Exit
    exit(0);
} 