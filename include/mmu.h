#ifndef MMU_H_GUARD // 防止重複包含的標準做法
#define MMU_H_GUARD

//TCR
#define TCR_CONFIG_REGION_48bit (((64-48) << 0) | ((64-48) << 16))
#define TCR_CONFIG_4KB ((0b00 << 14) | (0b10 << 30)) // TG0=4KB, TG1=4KB
#define TCR_CONFIG_DEFAULT (TCR_CONFIG_REGION_48bit | TCR_CONFIG_4KB)

//MAIR mode
#define MAIR_DEVICE_nGnRnE      0b00000000
#define MAIR_NORMAL_NOCACHE     0b01000100 // Normal, Inner/Outer Non-Cacheable
#define MAIR_IDX_DEVICE_nGnRnE  0
#define MAIR_IDX_NORMAL_NOCACHE 1

//attributes setting
#define PD_TABLE 0b11
#define PD_BLOCK 0b01 // For PUD block entries, bits[1:0] = 01 (Block entry)
                      // Bit[0] is 1 for Block/Page, Bit[1] is 0 for Block.
#define PD_ACCESS (1 << 10) // Access flag

// PGD attributes：指向下一級 Table
#define PGD_TABLE_ATTR PD_TABLE // Basic table descriptor

// PUD attribute：Block, Access, MAIR index for Device_nGnRnE
// MAIR index (bits 4:2)
#define PUD_TABLE_ATTR PD_TABLE
#define PUD_DEVICE_BLOCK_ATTR (PD_ACCESS | (MAIR_IDX_DEVICE_nGnRnE << 2) | PD_BLOCK)

// PMD attributes
// Attributes for PMD 2MB block descriptors
#define PMD_NORMAL_BLOCK_ATTR (PD_ACCESS | (MAIR_IDX_NORMAL_NOCACHE << 2) | PD_BLOCK)
#define PMD_DEVICE_BLOCK_ATTR (PD_ACCESS | (MAIR_IDX_DEVICE_nGnRnE << 2) | PD_BLOCK)
//PMD_size
#define PMD_BLOCK_SIZE 0x200000
// 為了 Identity Paging 和 Kernel Space Mapping，我們需要一些空間放 PGD 和 PUD
#define PGD_BASE_PHYS 0x0
#define PUD_BASE_PHYS 0x1000
#define PMD_BASE_PHYS 0x2000
// Virtual Address to Index Converters (for 4KB pages, 9 bits per level)
#define PGD_SHIFT 39
#define PUD_SHIFT 30
#define PMD_SHIFT 21
#define PTE_SHIFT 12
#define TABLE_INDEX_MASK 0x1FF // 9 bits

#define PAGE_SIZE 4096
#define PAGE_SHIFT 12 // log2(PAGE_SIZE)
#define TABLE_ADDR_MASK (~(PAGE_SIZE - 1)) // Mask to get the 4KB aligned base address

// Page Table Entry (PTE) and Descriptor Attributes
// These are simplified examples; refer to ARM ARM for full details.

// Level 3 Page Descriptor (points to a 4KB page)
#define PD_PAGE         (0b11UL << 0) // Valid page descriptor
#define PD_USER_ACCESS  (0b01UL << 6) // AP[2:1]: EL0 R/W, EL1 No Access (bits 7:6)
                                      // This is a common setting for user R/W pages.
                                      // For EL0 R/O, it would be (0b11UL << 6)
#define PD_UXN          (1UL << 54) // Unprivileged Execute Never (for data/stack)
#define PD_PXN          (1UL << 53) // Privileged Execute Never (kernel should not execute user data)


// User Code Page Attributes: Normal Memory, EL0 R/X, EL1 No Access, AF, Inner Shareable
#define USER_CODE_ATTR (PD_PAGE | PD_ACCESS |\
                        (MAIR_IDX_NORMAL_NOCACHE<<2) | \
                        (0b11UL << 6) | /* AP: EL0 Read-Only */ \
                        PD_PXN /* Kernel no execute */ \
                        /* UXN is 0 for executable code */ )


// User Stack/Data Page Attributes: Normal Memory, EL0 R/W, EL1 No Access, No Execute (UXN, PXN), AF, Inner Shareable
#define USER_DATA_STACK_ATTR (PD_PAGE | PD_ACCESS | \
                              (MAIR_IDX_NORMAL_NOCACHE<<2)  | \
                              (0b01UL << 6) | /* AP: EL0 Read/Write */ \
                              PD_UXN | PD_PXN)
// 這部分是 C 語言特有的宣告，組合器不應該看到
#ifndef __ASSEMBLER__
#include <stdint.h>
#include <stddef.h>
extern uint32_t user_space_size;
struct file_information
{
    void *filecontext;
    int filesize;
};
struct file_information find_program_in_initramfs(char *archive, const char *filename);
void el0_core_timer_enable(void);
#endif /* __ASSEMBLER__ */

#endif /* MMU_H_GUARD */