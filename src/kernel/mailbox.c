#include <stdint.h>
#include "uart.h"
#include "mailbox.h"


int mailbox_call(uint64_t *mailbox)
{
    uint64_t address = ((uint64_t)(uintptr_t)mailbox) & ~0xF;
    uint64_t value = address | 8; // Combine address with channel number (8)

    // Wait until we can write to the mailbox
    while (*MAILBOX_STATUS & MAILBOX_FULL)
        asm volatile("nop");

    // Write the address of our message to the mailbox with channel identifier
    *MAILBOX_WRITE = KVA_TO_PHYS(value);

    // Wait for the response
    while (1)
    {
        // Wait until we can read from the mailbox
        while (*MAILBOX_STATUS & MAILBOX_EMPTY)
            asm volatile("nop");

        // Check if the response is for us
        if (*MAILBOX_READ == KVA_TO_PHYS(value))
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
    uint64_t mailbox[8] __attribute__((aligned(16)));
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
    uint64_t addr = (uint64_t)(uintptr_t)kernel_mbox;
    uart_send_string("[mailbox_call_lowlevel] addr=");
    uart_send_hex(addr);
    uart_send_string("\r\n");
    if (addr & 0xF) { // 檢查是否 16 位元組對齊
         uart_send_string("Error: [mailbox_call_lowlevel] buffer not 16-byte aligned.\r\n");
         return -1;
    }
    uint64_t value = addr | (ch & 0xF); // 使用傳入的 channel
    uart_send_string("[mailbox_call_lowlevel] value=");
    uart_send_hex(value);
    uart_send_string("\r\n");
    // uart_send_string("[mailbox_call_lowlevel] MAILBOX_STATUS=");
    // uart_send_hex(MAILBOX_STATUS);
    // uart_send_string("\r\n");
    // for (int i=0; i<=30;i++){
    //     uart_send_string("[mailbox_call_lowlevel] Request MAILBOX[");
    //     uart_send_int(i);
    //     uart_send_string("]= ");
    //     uart_send_hex(kernel_mbox[i]);
    //     uart_send_string("\r\n");
    // }
    // 等待 Mailbox 可寫入
    while ( *MAILBOX_STATUS& MAILBOX_FULL) {
        asm volatile("nop");
    }

    // 寫入訊息位址和通道號
    *MAILBOX_WRITE = KVA_TO_PHYS(value);
    // 等待回應
    while (1) {
        // 等待 Mailbox 可讀取
        while (*MAILBOX_STATUS & MAILBOX_EMPTY) {
            asm volatile("nop");
        }

        // 讀取回應，看是否是給我們的
        if (*MAILBOX_READ == KVA_TO_PHYS(value))
        {
            uart_send_string("[mailbox_call_lowlevel]Get Mailbox response \r\n");
            uart_send_hex(kernel_mbox[1]);
            uart_send_string("\r\n");
            uart_send_string("[mailbox_call_lowlevel]KERNEL Frame_buffer address=");
            uart_send_hex(kernel_mbox[28]);
            uart_send_string("\r\n");
            // for (int i=0; i<=30;i++){
            //     uart_send_string("[mailbox_call_lowlevel] Response MAILBOX[");
            //     uart_send_int(i);
            //     uart_send_string("]= ");
            //     uart_send_hex(kernel_mbox[i]);
            //     uart_send_string("\r\n");
            // }   
            return 1;
        }
        // 如果讀到的不是我們要的 value，理論上應該繼續等，
        // 但如果一直等不到也可能是個問題 (超時)。這裡簡化，假設最終會收到。
    }
    // 理論上這裡不會到，可以加上超時處理
    return -1; // 表示超時或其他錯誤
}
int map_framebuffer_for_user(uint64_t user_pgd_pa,  uint64_t va_start, uint64_t size, uint64_t pa_start) {
    if (pa_start == 0 || size == 0) {
        uart_send_string("Error: [map_framebuffer_for_user] Framebuffer not initialized or size is zero.\r\n");
        return -1;
    }

    // 確保 Framebuffer 大小是頁對齊的，如果不是，向上取整
    // (通常 GPU 返回的大小本身就是頁對齊的，或者至少是某種對齊)

    uart_send_string("[map_framebuffer_for_user] Mapping Framebuffer for user:\r\n");
    uart_send_string("  User PGD PA: 0x"); uart_send_hex(user_pgd_pa); uart_send_string("\r\n");
    uart_send_string("  FB PA: 0x"); uart_send_hex(pa_start);
    uart_send_string(", FB Size: 0x"); uart_send_hex(size);
    uart_send_string("  Target User VA Start: 0x"); uart_send_hex(va_start);
    uart_send_string("\r\n");

    // 呼叫 mappages 進行映射
    // pgd_pa: 使用者行程的 PGD 實體位址
    // va_start: 我們為使用者 Framebuffer 選擇的虛擬起始位址
    // size: Framebuffer 的大小 (對齊後)
    // pa_start: Framebuffer 的實際實體位址
    // attributes: 允許使用者讀寫的屬性
    if (mappages(user_pgd_pa, 
                 va_start, 
                 size, 
                 pa_start, // Framebuffer 的實體位址
                 USER_FRAMEBUFFER_ATTR) != 0) {
        uart_send_string("Error: [map_framebuffer_for_user] Failed to map framebuffer into user space.\r\n");
        return -1;
    }

    uart_send_string("[map_framebuffer_for_user]Framebuffer successfully mapped for user.\r\n");
    // 核心可以將 USER_FRAMEBUFFER_VA_START 和 fb_size_aligned 這些資訊
    // 透過某種方式 (例如 syscall 返回值或共享記憶體) 告知使用者程式。
    return 1;
}