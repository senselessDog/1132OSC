#ifndef MMU_H_GUARD // 防止重複包含的標準做法
#define MMU_H_GUARD


//virtual physic offset for kernel
#define kernel_virtual_offset 0xffff000000000000

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

#define USER_FRAMEBUFFER_ATTR (PD_PAGE | PD_ACCESS | \
                              (MAIR_IDX_NORMAL_NOCACHE<<2)  | \
                              (0b01UL << 6) | /* AP: EL0 Read/Write */ \
                              PD_UXN | PD_PXN)
//user detail
#define USER_CODE_VA         0x00000000UL
#define USER_STACK_TOP_VA    0xfffffffff000UL // Top of a 16KB stack (4 pages)
#define USER_STACK_SIZE      (4 * PAGE_SIZE)     // 16KB
#define USER_STACK_BOTTOM_VA 0xffffffffb000UL

#define HANDLER_TOP_STACK 0xffffffffb000UL
#define HANDLER_BOTTOM_STACK 0xffffffffa000UL
#ifndef PHYS_TO_KVA
#define PHYS_TO_KVA(pa) ((void*)((uint64_t)(pa) + kernel_virtual_offset))
#endif
#ifndef KVA_TO_PHYS
#define KVA_TO_PHYS(kva) ((uint64_t)((uint64_t)(kva) - kernel_virtual_offset))
#endif
// --- mmap 相關定義 ---
// Protection flags (來自 sys/mman.h 的常見值)
#define PROT_NONE       0x00    // Page can not be accessed
#define PROT_READ       0x01    // Page can be read
#define PROT_WRITE      0x02    // Page can be written
#define PROT_EXEC       0x04    // Page can be executed

// Flags for mmap (來自 sys/mman.h 的常見值)
#define MAP_ANONYMOUS   0x20    // Don't use a file. The mapping is not backed by any file; 
                                // its contents are initialized to zero.
                                // (有些系統也用 MAP_ANON)
#define MAP_PRIVATE     0x02    // Create a private copy-on-write mapping.
                                // (Lab 6 進階練習3會用到類似概念，但 mmap 本身可以先不處理 COW)
#define MAP_FIXED       0x10    // Interpret addr exactly. (你的實驗指導似乎不要求嚴格的 MAP_FIXED)
#define MAP_POPULATE    0x8000  // Populate (prefault) page tables. (實驗要求)

#define MAP_FAILED      ((void *)-1) // mmap 失敗的返回值

// 假設使用者空間的低位址從某個 USER_VMA_AREA_START 開始
#define USER_VMA_AREA_START 0x0000000010000000UL // 範例：從 256MB 虛擬位址開始尋找
#define USER_VMA_AREA_END   0x0000000020000000UL // 範例：到 512GB 虛擬位址結束 (給 VMA 留出約 2.25GB
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
int mappages(uint64_t pgd_phys, uint64_t va_start, uint64_t size, uint64_t pa_start, uint64_t attributes);
void run_user_vm(char *archive_va);
void switch_user_address_space(uint64_t next_pgd_phys_addr);
//mmap
struct vm_area_struct {
    uint64_t vm_start;          // 區域的起始虛擬位址 (頁對齊)
    uint64_t vm_end;            // 區域的結束虛擬位址 (vm_start + size, 頁對齊)
    uint64_t vm_size;           // 區域大小 (vm_end - vm_start)
    int vm_prot;               // 保護屬性 (PROT_READ, PROT_WRITE, PROT_EXEC)
    int vm_flags;              // 旗標 (例如 MAP_ANONYMOUS)
    // struct file *vm_file;    // 對應的檔案 (匿名映射時為 NULL)
    // unsigned long vm_pgoff;  // 在檔案中的位移 (頁為單位)
    struct vm_area_struct *vm_next; // 指向行程的下一個 VMA
    struct vm_area_struct *vm_prev; // 指向行程的上一個 VMA (如果用雙向鏈結串列)
};
struct thread;
typedef struct thread thread_t;
struct trap_frame;
typedef struct trap_frame trap_frame_t;
// mmap 系統呼叫的處理函式原型
void* sys_mmap(void* addr, size_t len, int prot, int flags, int fd, int file_offset, trap_frame_t *frame);

// 輔助函數：將 prot 轉換為 PTE 屬性
uint64_t get_pte_attributes_from_prot(int prot, int flags);

// 輔助函數：在行程的 VMA 列表中尋找一個可用的虛擬位址區域
uint64_t find_available_vma_start(thread_t *process, size_t length);
#endif /* __ASSEMBLER__ */

#endif /* MMU_H_GUARD */