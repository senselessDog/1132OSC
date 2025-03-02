#include <stdint.h>

#define MMIO_BASE 0x3F000000

#define GPFSEL1 ((volatile uint32_t *)(MMIO_BASE + 0x00200004))
#define GPPUD ((volatile uint32_t *)(MMIO_BASE + 0x00200094))
#define GPPUDCLK0 ((volatile uint32_t *)(MMIO_BASE + 0x00200098))

#define AUX_ENABLES ((volatile uint32_t *)(MMIO_BASE + 0x00215004))
#define AUX_MU_IO_REG ((volatile uint32_t *)(MMIO_BASE + 0x00215040))
#define AUX_MU_IER_REG ((volatile uint32_t *)(MMIO_BASE + 0x00215044))
#define AUX_MU_IIR_REG ((volatile uint32_t *)(MMIO_BASE + 0x00215048))
#define AUX_MU_LCR_REG ((volatile uint32_t *)(MMIO_BASE + 0x0021504C))
#define AUX_MU_MCR_REG ((volatile uint32_t *)(MMIO_BASE + 0x00215050))
#define AUX_MU_LSR_REG ((volatile uint32_t *)(MMIO_BASE + 0x00215054))
#define AUX_MU_CNTL_REG ((volatile uint32_t *)(MMIO_BASE + 0x00215060))
#define AUX_MU_BAUD_REG ((volatile uint32_t *)(MMIO_BASE + 0x00215068))

void uart_init()
{
    // Disable UART
    *AUX_MU_CNTL_REG = 0;

    // Set GPIO 14, 15 to ALT5 (mini UART)
    *GPFSEL1 &= ~(7 << 12); // Clear bits 12-14
    *GPFSEL1 |= 2 << 12;    // Set ALT5 for GPIO 14
    *GPFSEL1 &= ~(7 << 15); // Clear bits 15-17
    *GPFSEL1 |= 2 << 15;    // Set ALT5 for GPIO 15

    // Disable GPIO pull-up/down
    *GPPUD = 0;
    for (int i = 0; i < 150; i++)
        asm volatile("nop");
    *GPPUDCLK0 = (1 << 14) | (1 << 15);
    for (int i = 0; i < 150; i++)
        asm volatile("nop");
    *GPPUDCLK0 = 0;

    // Enable mini UART
    *AUX_ENABLES |= 1;

    // Disable interrupts
    *AUX_MU_IER_REG = 0;

    // Set data size to 8 bits
    *AUX_MU_LCR_REG = 3;

    // Disable auto flow control
    *AUX_MU_MCR_REG = 0;

    // Set baud rate to 115200
    *AUX_MU_BAUD_REG = 270;

    // Clear FIFOs
    *AUX_MU_IIR_REG = 6;

    // Enable transmitter and receiver
    *AUX_MU_CNTL_REG = 3;
}

void uart_send(char c)
{
    while (!(*AUX_MU_LSR_REG & 0x20))
        asm volatile("nop");
    *AUX_MU_IO_REG = c;
}

char uart_recv()
{
    while (!(*AUX_MU_LSR_REG & 0x01))
        asm volatile("nop");
    return (char)(*AUX_MU_IO_REG);
}

void uart_send_string(const char *str)
{
    for (int i = 0; str[i] != '\0'; i++)
    {
        uart_send(str[i]);
    }
}