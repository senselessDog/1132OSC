#include <stdint.h>
#include <string.h>

void uart_init();
void uart_send_string(const char *str);
char uart_recv();
void uart_send(char c);

#define KERNEL_LOAD_ADDRESS 0x80000
#define HEADER_MAGIC 0x544F4F42 // "BOOT" in hex

typedef struct
{
    uint32_t magic;
    uint32_t size;
    uint32_t checksum;
} header_t;

uint32_t calculate_checksum(const uint8_t *data, uint32_t size)
{
    uint32_t checksum = 0;
    for (uint32_t i = 0; i < size; i++)
    {
        checksum += data[i];
    }
    return checksum;
}

void load_kernel()
{
    header_t header;
    uint8_t *kernel = (uint8_t *)KERNEL_LOAD_ADDRESS;

    // Receive header
    uart_send_string("Waiting for kernel header...\r\n");
    for (uint32_t i = 0; i < sizeof(header_t); i++)
    {
        ((uint8_t *)&header)[i] = uart_recv();
    }

    // Validate header
    if (header.magic != HEADER_MAGIC)
    {
        uart_send_string("Invalid header magic!\r\n");
        return;
    }

    // Receive kernel
    uart_send_string("Receiving kernel...\r\n");
    for (uint32_t i = 0; i < header.size; i++)
    {
        kernel[i] = uart_recv();
    }

    // Validate checksum
    if (header.checksum != calculate_checksum(kernel, header.size))
    {
        uart_send_string("Invalid checksum!\r\n");
        return;
    }

    uart_send_string("Kernel loaded successfully!\r\n");

    // Jump to kernel
    void (*kernel_entry)() = (void (*)())KERNEL_LOAD_ADDRESS;
    kernel_entry();
}

void bootloader_main()
{
    uart_init();
    load_kernel();
    while (1)
    {
        // Loop indefinitely
    }
}