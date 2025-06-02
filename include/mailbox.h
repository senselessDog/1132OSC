#ifndef MAILBOX_H_GUARD // 防止重複包含的標準做法
#define MAILBOX_H_GUARD
#include "uart.h"
#include "mmu.h"
#define MAILBOX_BASE (MMIO_BASE + 0xB880)

#define MAILBOX_READ ((volatile uint32_t *)(MAILBOX_BASE))
#define MAILBOX_STATUS ((volatile uint32_t *)(MAILBOX_BASE + 0x18))
#define MAILBOX_WRITE ((volatile uint32_t *)(MAILBOX_BASE + 0x20))

#define MAILBOX_EMPTY 0x40000000U
#define MAILBOX_FULL 0x80000000U

#define GET_BOARD_REVISION 0x00010002
#define GET_ARM_MEMORY 0x00010005
#define REQUEST_CODE 0x00000000
#define REQUEST_SUCCEED 0x80000000
#define REQUEST_FAILED 0x80000001
#define TAG_REQUEST_CODE 0x00000000
#define END_TAG 0x00000000
struct framebuffer_info {
    unsigned int width;
    unsigned int height;
    unsigned int pitch;
    unsigned int isrgb; // 1 for RGB, 0 for BGR is typical
    // 你可能還需要 depth, x_offset, y_offset 等，根據你的 mailbox 初始化細節
    // unsigned int depth; // 例如 32 bits per pixel
};

int mailbox_call_lowlevel(unsigned char ch, volatile unsigned int *kernel_mbox);
int map_framebuffer_for_user(uint64_t user_pgd_pa,  uint64_t va_start, uint64_t size, uint64_t pa_start);
#endif