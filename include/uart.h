#ifndef UART_H // 防止重複包含的保護（Header Guard）
#define UART_H

#include <stdint.h>

// MMIO base address for peripherals
#define MMIO_BASE 0x3F000000

// GPIO Function Select 1
#define GPFSEL1 ((volatile uint32_t *)(MMIO_BASE + 0x00200004))
// GPIO Pull-up/down Register
#define GPPUD ((volatile uint32_t *)(MMIO_BASE + 0x00200094))
// GPIO Pull-up/down Clock Register 0
#define GPPUDCLK0 ((volatile uint32_t *)(MMIO_BASE + 0x00200098))

// Auxiliary enables
#define AUX_ENABLES ((volatile uint32_t *)(MMIO_BASE + 0x00215004))
// Mini UART I/O Data
#define AUX_MU_IO_REG ((volatile uint32_t *)(MMIO_BASE + 0x00215040))
// Mini UART Interrupt Enable
#define AUX_MU_IER_REG ((volatile uint32_t *)(MMIO_BASE + 0x00215044))
// Mini UART Interrupt Identify
#define AUX_MU_IIR_REG ((volatile uint32_t *)(MMIO_BASE + 0x00215048))
// Mini UART Line Control
#define AUX_MU_LCR_REG ((volatile uint32_t *)(MMIO_BASE + 0x0021504C))
// Mini UART Modem Control
#define AUX_MU_MCR_REG ((volatile uint32_t *)(MMIO_BASE + 0x00215050))
// Mini UART Line Status
#define AUX_MU_LSR_REG ((volatile uint32_t *)(MMIO_BASE + 0x00215054))
// Mini UART Extra Control
#define AUX_MU_CNTL_REG ((volatile uint32_t *)(MMIO_BASE + 0x00215060))
// Mini UART Baudrate
#define AUX_MU_BAUD_REG ((volatile uint32_t *)(MMIO_BASE + 0x00215068))

// 第一級中斷控制器
#define CORE0_INTERRUPT_SOURCE ((volatile uint32_t *)(0x40000060))
#define GPU_INTERRUPTS 0x100 // 位元 8 代表 GPU 中斷

// 第二級中斷控制器寄存器
#define ENABLE_IRQS1 ((volatile uint32_t *)(MMIO_BASE + 0x0000B210))
#define IRQ_PENDING1 ((volatile uint32_t *)(MMIO_BASE + 0x0000B204))
// 循環緩衝區定義
#define BUFFER_SIZE 256

// 緩衝區聲明
extern char rx_buffer[BUFFER_SIZE];
extern int rx_head;
extern int rx_tail;

extern char tx_buffer[BUFFER_SIZE];
extern int tx_head;
extern int tx_tail;

int is_buffer_full(int head, int tail);
int is_buffer_empty(int head, int tail);
void buffer_push(char *buffer, int *head, char data);
char buffer_pop(char *buffer, int *tail);
int uart_async_send(char c);
void uart_async_send_string(const char *str);
int uart_async_recv(char *c);

void uart_enable_interrupt();
int is_gpu_interrupt();
void uart_enable_tx_interrupt();
void uart_disable_tx_interrupt();
void uart_enable_rx_interrupt();
void uart_disable_rx_interrupt();
void uart_send(char c);
char uart_recv();
void uart_send_string(const char *str);
void uart_send_hex(uint64_t value);
void uart_send_int(int value);

#endif // UART_H