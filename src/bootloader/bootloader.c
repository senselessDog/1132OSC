#include <stdint.h>
#include <string.h>

void uart_init();
void uart_send_string(const char *str);
void uart_send_hex(uint32_t value);
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

void load_kernel(void *dtb_addr)
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
    void (*kernel_entry)(void *) = (void (*)(void *))KERNEL_LOAD_ADDRESS;
    kernel_entry(dtb_addr);

    return;
}

void bootloader_main(void *dtb_addr)
{
    uart_init();
    uart_send_string("[bootloader_main] dtb address: \r\n");
    uart_send_hex((uint64_t)dtb_addr);
    uart_send_string("\r\n");
    uart_send_string("[bootloader_main] start bootloader!\r\n");
    while (1)
    {
        load_kernel(dtb_addr);
        uart_send_string("[bootloader_main] can't load kernel error!\r\n");
        // Loop indefinitely
    }
}