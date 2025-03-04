#include <stdint.h>

#define MMIO_BASE 0x3F000000
#define MAILBOX_BASE (MMIO_BASE + 0xB880)

#define MAILBOX_READ ((volatile uint32_t *)(MAILBOX_BASE))
#define MAILBOX_STATUS ((volatile uint32_t *)(MAILBOX_BASE + 0x18))
#define MAILBOX_WRITE ((volatile uint32_t *)(MAILBOX_BASE + 0x20))

#define MAILBOX_EMPTY 0x40000000
#define MAILBOX_FULL 0x80000000

#define GET_BOARD_REVISION 0x00010002
#define GET_ARM_MEMORY 0x00010005
#define REQUEST_CODE 0x00000000
#define REQUEST_SUCCEED 0x80000000
#define REQUEST_FAILED 0x80000001
#define TAG_REQUEST_CODE 0x00000000
#define END_TAG 0x00000000

void uart_send_string(const char *str);
void uart_send_hex(uint32_t value);

int mailbox_call(uint32_t *mailbox)
{
    uint32_t address = ((uint32_t)(uintptr_t)mailbox) & ~0xF;
    uint32_t value = address | 8; // Combine address with channel number (8)

    // Wait until we can write to the mailbox
    while (*MAILBOX_STATUS & MAILBOX_FULL)
        asm volatile("nop");

    // Write the address of our message to the mailbox with channel identifier
    *MAILBOX_WRITE = value;

    // Wait for the response
    while (1)
    {
        // Wait until we can read from the mailbox
        while (*MAILBOX_STATUS & MAILBOX_EMPTY)
            asm volatile("nop");

        // Check if the response is for us
        if (*MAILBOX_READ == value)
        {
            uart_send_string("Get Mailbox response \r\n");
            uart_send_hex(mailbox[1]);
            uart_send_string("\r\n");
            return 1;
        }
    }

    return 0;
}

void get_board_revision()
{
    unsigned int mailbox[7];
    mailbox[0] = 7 * 4; // buffer size in bytes
    mailbox[1] = REQUEST_CODE;
    // tags begin
    mailbox[2] = GET_BOARD_REVISION; // tag identifier
    mailbox[3] = 4;                  // maximum of request and response value buffer's length.
    mailbox[4] = TAG_REQUEST_CODE;
    mailbox[5] = 0; // value buffer
    // tags end
    mailbox[6] = END_TAG;

    if (mailbox_call(mailbox))
    {
        uart_send_string("Board Revision: 0x");
        uart_send_hex(mailbox[5]);
        uart_send_string("\r\n");
    }
    else
    {
        uart_send_string("Failed to get board revision\r\n");
    }
}

void get_arm_memory()
{
    uint32_t mailbox[8] __attribute__((aligned(16)));
    mailbox[0] = 8 * 4; // buffer size in bytes
    mailbox[1] = REQUEST_CODE;
    // tags begin
    mailbox[2] = GET_ARM_MEMORY; // tag identifier
    mailbox[3] = 8;              // maximum of request and response value buffer's length.
    mailbox[4] = TAG_REQUEST_CODE;
    mailbox[5] = 0; // base address
    mailbox[6] = 0; // size
    // tags end
    mailbox[7] = END_TAG;

    if (mailbox_call(mailbox))
    {
        uart_send_string("ARM Memory Base Address: 0x");
        uart_send_hex(mailbox[5]);
        uart_send_string("\r\n");
        uart_send_string("ARM Memory Size: 0x");
        uart_send_hex(mailbox[6]);
        uart_send_string("\r\n");
    }
    else
    {
        uart_send_string("Failed to get ARM memory information\r\n");
    }
}