#include <stdint.h>
#include "uart.h"
#include "mailbox.h"


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

int mailbox_call_lowlevel(unsigned char ch, volatile unsigned int *kernel_mbox) {//for user thread
    // 將 kernel_mbox 位址 (虛擬==物理，無MMU) 和通道號結合
    // 位址需要是實體位址，且 16 位元組對齊
    // 在沒有 MMU 的情況下，我們假設 kernel_mbox 的虛擬位址就是實體位址
    // 你的 & ~0xF 操作確保了對齊，但前提是 kernel_mbox 指標本身是對齊的
    uint32_t addr = (uint32_t)(uintptr_t)kernel_mbox;
    if (addr & 0xF) { // 檢查是否 16 位元組對齊
         uart_send_string("Error: mailbox_call_lowlevel buffer not 16-byte aligned.\r\n");
         return -1;
    }
    uint32_t value = addr | (ch & 0xF); // 使用傳入的 channel

    // 等待 Mailbox 可寫入
    while (*MAILBOX_STATUS & MAILBOX_FULL) {
        asm volatile("nop");
    }

    // 寫入訊息位址和通道號
    *MAILBOX_WRITE = value;

    // 等待回應
    while (1) {
        // 等待 Mailbox 可讀取
        while (*MAILBOX_STATUS & MAILBOX_EMPTY) {
            asm volatile("nop");
        }

        // 讀取回應，看是否是給我們的
        if (*MAILBOX_READ == value)
        {
            uart_send_string("Get Mailbox response \r\n");
            uart_send_hex(kernel_mbox[1]);
            uart_send_string("\r\n");
            return 1;
        }
        // 如果讀到的不是我們要的 value，理論上應該繼續等，
        // 但如果一直等不到也可能是個問題 (超時)。這裡簡化，假設最終會收到。
    }
    // 理論上這裡不會到，可以加上超時處理
    return -1; // 表示超時或其他錯誤
}