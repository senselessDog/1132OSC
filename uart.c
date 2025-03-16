#include <stdint.h>

// Function prototypes
void uart_init();
void uart_send(char c);
char uart_recv();
void uart_send_string(const char *str);
void uart_send_hex(uint32_t value);

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
        asm volatile("nop");            // Wait 150 cycles
    *GPPUDCLK0 = (1 << 14) | (1 << 15); // Apply to GPIO 14 and 15
    for (int i = 0; i < 150; i++)
        asm volatile("nop"); // Wait 150 cycles
    *GPPUDCLK0 = 0;          // Remove the clock

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
    uart_send_string("UART initialized!\r\n");
}

void uart_send(char c)
{
    // Wait until the transmitter is empty
    while (!(*AUX_MU_LSR_REG & 0x20))
        asm volatile("nop");
    // Write the character to the I/O register
    *AUX_MU_IO_REG = c;
}

char uart_recv()
{
    // Wait until data is ready to be read
    while (!(*AUX_MU_LSR_REG & 0x01))
        asm volatile("nop");
    // Read the character from the I/O register
    return (char)(*AUX_MU_IO_REG);
}

void uart_send_string(const char *str)
{
    // Send each character in the string
    for (int i = 0; str[i] != '\0'; i++)
    {
        uart_send(str[i]);
    }
}

void uart_send_hex(uint32_t value)
{
    const char hex_chars[] = "0123456789ABCDEF";
    // Send each nibble (4 bits) as a hex character
    for (int i = 28; i >= 0; i -= 4)
    {
        uart_send(hex_chars[(value >> i) & 0xF]);
    }
}