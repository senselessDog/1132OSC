#include <stdint.h>
#include "uart.h"

// Function prototypes
void uart_init();
void uart_send(char c);
char uart_recv();
void uart_send_string(const char *str);
void uart_send_hex(uint32_t value);

// constants
char rx_buffer[BUFFER_SIZE];
int rx_head;
int rx_tail;

char tx_buffer[BUFFER_SIZE];
int tx_head;
int tx_tail;

// Check if buffer is full
int is_buffer_full(int head, int tail)
{
    return ((head + 1) % BUFFER_SIZE) == tail;
}

// Check if buffer is empty
int is_buffer_empty(int head, int tail)
{
    return head == tail;
}

// Add a byte to the buffer
void buffer_push(char *buffer, int *head, char data)
{
    buffer[*head] = data;
    *head = (*head + 1) % BUFFER_SIZE;
}

// Get a byte from the buffer
char buffer_pop(char *buffer, int *tail)
{
    char data = buffer[*tail];
    *tail = (*tail + 1) % BUFFER_SIZE;
    return data;
}

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

    // Set data size to 8 bits
    *AUX_MU_LCR_REG = 3;

    // Disable auto flow control
    *AUX_MU_MCR_REG = 0;

    // Set baud rate to 115200
    *AUX_MU_BAUD_REG = 270;

    // Clear FIFOs
    *AUX_MU_IIR_REG = 6;
    // interrupts
    *AUX_MU_IER_REG |= 0x0C;
    // Enable transmitter and receiver
    *AUX_MU_CNTL_REG = 3;

    *ENABLE_IRQS1 |= (1 << 29);
}

// Asynchronous send (non-blocking)
int uart_async_send(char c)
{
    // Check if buffer is full
    if (is_buffer_full(tx_head, tx_tail))
    {
        return 0; // Buffer full, can't send
    }

    // Disable interrupts during critical section
    // (You'll need to implement disable/enable interrupt functions)
    // disable_interrupts();

    // Add character to buffer
    buffer_push(tx_buffer, &tx_head, c);

    // Enable TX interrupts to start sending
    uart_enable_tx_interrupt(); // Enable TX interrupts

    // Re-enable interrupts
    // enable_interrupts();

    return 1; // Successfully queued
}

// Asynchronous send string
void uart_async_send_string(const char *str)
{
    // uart_send_string("get into uart_async_send_string\r\n");
    for (int i = 0; str[i] != '\0'; i++)
    {
        // uart_send(str[i]); // Send each character
        //  Try until we can add to buffer
        while (!uart_async_send(str[i]))
        {
            uart_send_string("\r\nfull\r\n");
            // If buffer is full, we could yield here in a multitasking system
            asm volatile("nop");
        }
    }
}

// Asynchronous receive (non-blocking)
int uart_async_recv(char *c)
{
    // Check if buffer is empty
    if (is_buffer_empty(rx_head, rx_tail))
    {
        return 0; // No data available
    }
    uart_enable_rx_interrupt();
    // Disable interrupts during critical section
    // disable_interrupts();

    // Get character from buffer
    *c = buffer_pop(rx_buffer, &rx_tail);

    // Re-enable interrupts
    // enable_interrupts();

    return 1; // Successfully received a character
}

void uart_enable_tx_interrupt()
{
    *AUX_MU_IER_REG |= (1 << 1);
    // uart_send_string("Enable TX interrupt\r\n");
    // uart_send_hex((uint32_t)*AUX_MU_IER_REG);
    // uart_send_string("\r\n");
}

void uart_disable_tx_interrupt()
{
    *AUX_MU_IER_REG &= ~(1 << 1);
}

void uart_enable_rx_interrupt()
{
    *AUX_MU_IER_REG |= (1 << 0);
}

void uart_disable_rx_interrupt()
{
    *AUX_MU_IER_REG &= ~(1 << 0);
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

void uart_send_int(int value)
{
    char buffer[12]; // Buffer to hold the string representation of the integer
    int index = 0;

    // Handle negative numbers
    if (value < 0)
    {
        uart_send('-');
        value = -value;
    }

    // Convert integer to string
    do
    {
        buffer[index++] = (value % 10) + '0';
        value /= 10;
    } while (value > 0);

    // Send the string in reverse order
    while (index > 0)
    {
        uart_send(buffer[--index]);
    }
}