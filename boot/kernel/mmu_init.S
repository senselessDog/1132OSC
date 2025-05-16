#include<mmu.h>

// Arguments:
//   write_to_reg: Register holding the address where the descriptor is to be written
//   phys_addr_reg_or_imm: Register or immediate value for the physical base address component of the descriptor
//   attributes_symbol: A symbol (#define) whose value contains the attribute bits for the descriptor
.macro WRITE_PAGE_DESCRIPTOR write_to_addr physical_addr attributes
  mov x0, \write_to_addr
  mov x1, \physical_addr
  ldr x2, = \attributes
  orr x2, x1, x2
  str x2, [x0]
.endm

.global mmu_setup
mmu_setup:
    // 1. Setup TCR_EL1
    ldr x0, =TCR_CONFIG_DEFAULT
    msr tcr_el1, x0

    // 2. Setup MAIR_EL1
    ldr x0, =((MAIR_DEVICE_nGnRnE << (MAIR_IDX_DEVICE_nGnRnE * 8)) | \
               (MAIR_NORMAL_NOCACHE << (MAIR_IDX_NORMAL_NOCACHE * 8)))
    msr mair_el1, x0
    // 3. Setup Identity Paging Page Tables (PGD and PUD)
    mov x5, PGD_BASE_PHYS 
    mov x6, PUD_BASE_PHYS
    mov x7, PMD_BASE_PHYS

    // 清理 PGD 和 PUD 頁框 (假設它們是 4KB 大小，初始化為0代表無效entry)

    // PGD[0] -> PUD_BASE_PHYS
    // PGD 條目: [PUD_BASE_PHYS (bits 47:12) | ... | BOOT_PGD_ATTR (bits 1:0 = 0b11)]
    WRITE_PAGE_DESCRIPTOR PGD_BASE_PHYS, PUD_BASE_PHYS, PGD_TABLE_ATTR
    
    // PUD[0] -> maps 0x00000000 - 0x3FFFFFFF (1GB Block) to PA 0x00000000
    // PUD 條目: [Block Address (bits 38:30) | ... | BLOCK_PUD_ATTR]
    WRITE_PAGE_DESCRIPTOR PUD_BASE_PHYS, PMD_BASE_PHYS, PUD_TABLE_ATTR
    
    // PUD[1] -> maps 0x40000000 - 0x7FFFFFFF (1GB Block) to PA 0x40000000
    // ARM local peripherals (0x40000000 - 0x7fffffff)
    WRITE_PAGE_DESCRIPTOR (PUD_BASE_PHYS + 8), 0x40000000, PUD_DEVICE_BLOCK_ATTR
    
    // --- PMD Setup for the first 1GB (VA 0x00000000 - 0x3FFFFFFF) ---
    // This PMD table is located at PMD_BASE_PHYS (in x7)
    // It will have 512 entries, each mapping a 2MB block.
    // x10: loop counter 'i' (0 to 511)
    // x11: current PMD entry address (PMD_BASE_PHYS + i * 8) - will be passed to macro in x0
    // x12: physical address of the 2MB block to be mapped (i * 2MB) - will be passed to macro in x1
    mov x10, #0               // i = 0 (loop counter)

pmd_loop_start:
    cmp x10, #512             // if i >= 512, loop done
    b.ge pmd_loop_end

    // Calculate current PMD entry address: x11 = x7 (PMD_BASE_PHYS) + x10 * 8
    add x11, x7, x10, LSL #3

    // Calculate physical address for this 2MB block: x12 = x10 * PMD_BLOCK_SIZE
    ldr x15, =PMD_BLOCK_SIZE  // Load 2MB block size
    mul x12, x10, x15         // x12 = i * 2MB

    // Determine attributes based on the region
    // Entries 0-503 (VA 0x0 to 0x3EFFFFFF, PA 0x0 to 0x3EFFFFFF) are Normal Memory
    // Entries 504-511 (VA 0x3F000000 to 0x3FFFFFFF, PA 0x3F000000 to 0x3FFFFFFF) are Device Memory
    cmp x10, #504             // if i < 504
    b.lt pmd_set_normal_macro
    // else (i >= 504), set Device Memory attributes
    WRITE_PAGE_DESCRIPTOR x11, x12, PMD_DEVICE_BLOCK_ATTR
    b pmd_attr_done_macro
pmd_set_normal_macro:
    WRITE_PAGE_DESCRIPTOR x11, x12, PMD_NORMAL_BLOCK_ATTR
pmd_attr_done_macro:
    // The str instruction is now inside the macro

    add x10, x10, #1          // i++
    b pmd_loop_start

pmd_loop_end:
    //4. --- Map the Kernel Space & Enable MMU ---
    // Lab 要求將 identity paging 的 PGD 同時載入 TTBR0_EL1 和 TTBR1_EL1
    // x5 (PGD_BASE_PHYS) 是我們 identity paging 的 PGD
    msr ttbr0_el1, x5       // 低位址空間使用 identity PGD
    msr ttbr1_el1, x5       // 高位址空間也暫時使用這個 identity PGD 
                            // (這需要 linker script 和 PGD內容能配合，讓高VA能對應到低PA)
    
    // 確保所有記憶體存取和設定完成
    dsb ish
    isb
    // Enable MMU
    mrs x2, sctlr_el1
    orr x2, x2, #1          // Set M bit (bit 0) to enable MMU
    msr sctlr_el1, x2

    // 確保所有記憶體存取和設定完成
    dsb ish
    isb
    
    ldr x2, = after_high_kernel_return
    br x2