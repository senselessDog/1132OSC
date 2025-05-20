#include<mmu.h>
#include<buddy_alloc.h>
#include<uart.h>
#include<strcmp.h>
#include<thread.h>
static int first_thread=1;
static inline uint64_t va_to_index(uint64_t va, int level_shift) {
    return (va >> level_shift) & TABLE_INDEX_MASK;
}

uint64_t* walk_and_create_pte(uint64_t pgd_pa, uint64_t va, int alloc) {
    // 核心透過KVA存取目標PGD的內容
    uint64_t* current_table_access_kva = (uint64_t*)PHYS_TO_KVA(pgd_pa);
    uint64_t  next_table_alloc_kva;   // dynamic_malloc 返回的 KVA
    uint64_t  next_table_pa_to_store; // 要存入分頁表條目的 PA
    uint64_t* entry_access_kva_ptr;   // 指向當前分頁表條目的核心虛擬位址指標

    int level_shifts[] = {PGD_SHIFT, PUD_SHIFT, PMD_SHIFT}; // PGD->PUD, PUD->PMD, PMD->PTE_Table

    for (int i = 0; i < 3; ++i) { // 遍歷 PGD, PUD, PMD 層級
        entry_access_kva_ptr = &current_table_access_kva[va_to_index(va, level_shifts[i])];

        if (!(*entry_access_kva_ptr & PD_TABLE)) { // 檢查是否為有效的表描述符
            if (!alloc) {
                uart_send_string("[walk] No alloc and table entry missing.\r\n");
                return NULL;
            }
            //alloc new nable
            next_table_alloc_kva = (uint64_t)dynamic_malloc(PAGE_SIZE);
            if (!next_table_alloc_kva) {
                uart_send_string("[walk] dynamic_malloc for new table failed.\r\n");
                return NULL;
            }
            // 使用KVA進行memset
            memset((void*)next_table_alloc_kva, 0, PAGE_SIZE);
            // 將KVA轉換為PA以存入分頁表條目
            next_table_pa_to_store = KVA_TO_PHYS(next_table_alloc_kva);
            *entry_access_kva_ptr = next_table_pa_to_store | PD_TABLE;
            uart_send_string("[walk] Created new table. KVA: 0x"); uart_send_hex(next_table_alloc_kva);
            uart_send_string(" -> PA: 0x"); uart_send_hex(next_table_pa_to_store);
            uart_send_string("\r\n[walk]stored in entry @ KVA 0x"); uart_send_hex((uint64_t)entry_access_kva_ptr);
            uart_send_string("\r\n");
        }
        // 從條目中讀取下一級表的PA，並轉換為KVA供核心存取
        current_table_access_kva = (uint64_t*)PHYS_TO_KVA((*entry_access_kva_ptr) & TABLE_ADDR_MASK);
    }

    // 現在 current_table_access_kva 指向PTE表的基底KVA
    // 返回指向特定PTE條目的KVA指標
    return &current_table_access_kva[va_to_index(va, PTE_SHIFT)];
}
int mappages(uint64_t pgd_phys, uint64_t va_start, uint64_t size, uint64_t pa_start, uint64_t attributes) {
    if ((va_start % PAGE_SIZE != 0) || (pa_start % PAGE_SIZE != 0) || (size % PAGE_SIZE != 0)) {
        uart_send_string("Error: [mappages] - addresses or size not page aligned.\r\n");
        return -1;
    }

    uint64_t current_va = va_start;
    uint64_t current_pa = pa_start;
    uint64_t end_va = va_start + size;

    uart_send_string("[mappages] Mapping VA 0x");
    uart_send_hex(va_start);
    uart_send_string(" to PA 0x");
    uart_send_hex(pa_start);
    uart_send_string("\r\n[mappages] size 0x");
    uart_send_hex(size);
    uart_send_string("\r\n");


    for (; current_va < end_va; current_va += PAGE_SIZE, current_pa += PAGE_SIZE) {
        uint64_t* pte_phys_ptr = walk_and_create_pte(pgd_phys, current_va, 1 /* alloc */);
        if (!pte_phys_ptr) {
            uart_send_string("mappages: Error - walk_and_create_pte failed for VA 0x");
            uart_send_hex(current_va);
            uart_send_string("\r\n");
            return -1;
        }
        // Construct the PTE: physical page frame address ORed with attributes
        *pte_phys_ptr = (current_pa & TABLE_ADDR_MASK) | attributes;
        // uart_send_string("Mapped VA 0x"); uart_send_hex(current_va);
        // uart_send_string(" -> PA 0x"); uart_send_hex(current_pa);
        // uart_send_string(" PTE@0x"); uart_send_hex((uint64_t)pte_phys_ptr);
        // uart_send_string(" = 0x"); uart_send_hex(*pte_phys_ptr);
        // uart_send_string("\r\n");
    }
    return 0;
}

void switch_to_el0_vm(uint64_t user_pgd_phys, void *start_va, void *stack_top_va) {
    // el0_core_timer_enable(); // Prepare timer for user space (if needed by user)

    uart_send_string("[switch_to_el0_vm]\r\n");
    uart_send_string("User PGD PA: 0x"); uart_send_hex(user_pgd_phys); uart_send_string("\r\n");
    uart_send_string("Start VA: 0x"); uart_send_hex((uint64_t)start_va); uart_send_string("\r\n");
    uart_send_string("Stack Top VA: 0x"); uart_send_hex((uint64_t)stack_top_va); uart_send_string("\r\n");

    // 1. Set TTBR0_EL1 to the user's PGD physical address
    asm volatile("dsb ish"); 
    asm volatile("msr ttbr0_el1, %0" :: "r"(user_pgd_phys));
    
    // 2. Invalidate all TLB entries for the current VMID (or all if ASID not used)
    //    This ensures old translations are flushed.
    asm volatile("dsb ish");    // Ensure prior memory accesses complete
    asm volatile("tlbi vmalle1is"); // Invalidate TLB entries for EL1 and EL0, inner shareable
    asm volatile("dsb ish");      // Ensure TLB invalidation completes
    asm volatile("isb");          // Synchronize context, clear pipeline
    // 3. Prepare SPSR_EL1 and ELR_EL1 for eret
    // SPSR_EL1: Target EL0, AArch64, No interrupts masked (D,A,I,F = 0)
    uint64_t spsr_el0 = 0x0; // EL0t (using SP_EL0), AArch64, all interrupts unmasked
    el0_core_timer_enable();
    asm volatile(
        "msr spsr_el1, %0\n"  // Status to restore (EL0 mode)
        "msr elr_el1, %1\n"   // Exception Link Register (return address in EL0)
        "msr sp_el0, %2\n"    // Stack Pointer for EL0
        : : "r"(spsr_el0), "r"(start_va), "r"(stack_top_va)
    );
    uart_send_string("[switch_to_el0_vm]Test_ttbr0_el1\r\n");
    asm volatile("eret\n");
}

// --- Modified run_user ---
// static int first_thread = 1; // This should be managed per-process if you have multiple

// User program's target virtual addresses


void run_user_vm(char *archive_kva_addr) {
    char filename[1024];
    int index = 0;
    // Receive the filename from the user
    uart_send_string("\r\n");
    uart_send_string("Filename: ");
    while (1)
    {

        char c = uart_recv();
        uart_send(c); // Echo the received character
        if (c == '\r' || c == '\n')
        {
            filename[index] = '\0'; // Null-terminate the string
            index = 0;
            uart_send_string("\r\n");
            break;
        }
        else
        {
            filename[index] = c;
            index++;
        }
    }

    // (Optional: keep your UART filename input if needed, or use default_filename)
    // For simplicity, I'll use default_filename. If you uncomment your UART input,
    // make sure filename_buffer is used for find_program_in_initramfs.

    uart_send_string("[run_user_vm] Archive PA: 0x"); uart_send_hex((uint64_t)archive_kva_addr); uart_send_string("\r\n");
    uart_send_string("[run_user_vm] Program: "); uart_send_string(filename); uart_send_string("\r\n");

    struct file_information program_info = find_program_in_initramfs(archive_kva_addr, filename);
    if (!program_info.filecontext || program_info.filesize <= 0) {
        uart_send_string("Error: [run_user_vm] Program not found or empty: "); uart_send_string(filename); uart_send_string("\r\n");
        return;
    }
    if (program_info.filesize>user_space_size){
        uart_send_string("Error: [run_user_vm] Program Size too large "); uart_send_string(filename); uart_send_string("\r\n");
        return;
    }
    uart_send_string("[run_user_vm]Program found. PA: 0x"); uart_send_hex((uint64_t)program_info.filecontext);
    uart_send_string(", Size: "); uart_send_hex(program_info.filesize); uart_send_string(" bytes\r\n");


    // 1. Allocate PGD for the new user address space
    uint64_t user_pgd_kva = (uint64_t)dynamic_malloc(4096);
    if (!user_pgd_kva) {
        uart_send_string("Error:[run_user_vm] Failed to allocate PGD for user space!\r\n");
        return;
    }
    uint64_t user_pgd_pa=KVA_TO_PHYS(user_pgd_kva);
    memset((void*)user_pgd_kva, 0, PAGE_SIZE); // Clear PGD
    uart_send_string("[run_user_vm] User PGD allocated at PA: 0x"); uart_send_hex(user_pgd_pa); uart_send_string("\r\n");

    // 2. Map user program's code
    // Assuming program_info.filecontext is the PHYSICAL address of the code
    uint64_t user_base_kva=(uint64_t)dynamic_malloc(user_space_size);
    uint64_t user_base_pa=KVA_TO_PHYS(user_base_kva);
    // uart_send_string("Create parent thread: ");
    // thread_t * parent_thread=thread_create(user_base,NULL);
    // uart_send_string("Create parent thread success\r\n");
    // void *user_stack=user_base+user_space_size;
    
    memcpy((void*)user_base_kva,(const void *)program_info.filecontext, (uint32_t)program_info.filesize);
    uart_send_string("[run_user_vm] pte_attributes=");
    uart_send_hex(USER_CODE_ATTR);
    uart_send_string("\r\n");
    // if (mappages(user_pgd_pa, USER_CODE_VA, (uint64_t)user_space_size, user_base_pa, USER_CODE_ATTR) != 0) {
    //     uart_send_string("[run_user_vm] Failed to map user code!\r\n");
    //     // Potentially free PGD and other allocated tables here
    //     return;
    // }
    // uart_send_string("[run_user_vm] User code mapped: VA 0x0 to PA 0x"); uart_send_hex((uint64_t)user_base_pa);
    // uart_send_string(" (size 0x"); uart_send_hex(user_space_size); uart_send_string(")\r\n");


    // 3. Allocate and map user stack (4 pages = 16KB)
    uint64_t user_stack_kva = (uint64_t)dynamic_malloc(PAGE_SIZE*4);
    if (!user_stack_kva){
        uart_send_string("Error: [run_user_vm] user_stack_pa_pages can't alloc");
    }
    uint64_t user_stack_pa = KVA_TO_PHYS(user_stack_kva);
    // No need to memset stack pages, user program will use them.
    // uart_send_string("[run_user_vm]User stack page ");
    // uart_send_string(" allocated at PA: 0x"); uart_send_hex(user_stack_pa); uart_send_string("\r\n");

    //     // Map this single page
    //     // Stack grows downwards, so USER_STACK_BOTTOM_VA is the start of the VA range.
    //     // We map pa_pages[0] to USER_STACK_BOTTOM_VA, pa_pages[1] to USER_STACK_BOTTOM_VA + PAGE_SIZE, etc.
    // if (mappages(user_pgd_pa, USER_STACK_BOTTOM_VA, USER_STACK_SIZE, user_stack_pa, USER_DATA_STACK_ATTR) != 0) {
    //     uart_send_string("Error: [run_user_vm]Failed to map user stack page!\r\n");
    //     // Potentially free PGD and other allocated tables/pages here
    //     return;
    // }
    uart_send_string("[run_user_vm]User stack mapped: VA 0x"); uart_send_hex(USER_STACK_BOTTOM_VA);
    uart_send_string(" - 0x"); uart_send_hex(USER_STACK_TOP_VA -1); uart_send_string("\r\n");


    // The stack pointer SP_EL0 should point to the top of the allocated stack region.
    if (first_thread){
        thread_init_user((void*)user_base_pa,(void*)user_stack_kva);
    }
    if (first_thread){
        first_thread=0;
        switch_to_el0_vm(user_pgd_pa, (void*)USER_CODE_VA, (void*)USER_STACK_TOP_VA);
    }else{
        thread_t * current_thread=get_current();
        current_thread->thread_stack_alloc_kva=user_stack_kva;
        current_thread->user_code_start_pa=user_base_pa;
        change_ttbr0_el1(user_pgd_pa);
    }
    return;
    // Kernel should not reach here after eret if switch is successful
    // uart_send_string("Error: Returned from switch_to_el0_vm unexpectedly!\r\n");
}
//To clear TLB and set next_pgd_phys_addr
void switch_user_address_space(uint64_t next_pgd_phys_addr) {
    // uart_send_string("[MMU] Switching user address space to PGD PA: 0x");
    // uart_send_hex(next_pgd_phys_addr);
    // uart_send_string("\r\n");

    // 執行記憶體屏障，確保所有在切換 TTBR0_EL1 之前的記憶體寫入操作都已完成。
    // DSB (Data Synchronization Barrier): 確保所有在此指令之前的資料存取操作
    // (讀取或寫入) 都已完成，才會執行後續指令。
    // ISH (Inner Shareable): 屏障作用於內部共享域 (通常是同一個處理器內的所有核心)。
    asm volatile("dsb ish" : : : "memory");

    // 將 next_pgd_phys_addr (新的 PGD 實體位址) 載入到 TTBR0_EL1 系統暫存器。
    // %0 代表第一個 C 語言變數 (next_pgd_phys_addr)。
    // "r" 表示這個變數會透過一個通用暫存器傳遞。
    asm volatile("msr ttbr0_el1, %0" : : "r"(next_pgd_phys_addr) : "memory");

    // 再次執行 DSB ISH，確保 TTBR0_EL1 的更新對後續的 TLB 操作可見。
    asm volatile("dsb ish" : : : "memory");

    // TLBI (TLB Invalidate): 使 TLB 條目失效。
    // VMALLE1IS (VMID All, EL1 and Stage 1, Inner Shareable):
    // 使所有屬於目前 VMID (虛擬機器 ID，如果未使用則為全域) 的、
    // EL1 和 EL0 的、第一階段翻譯的、內部共享域的 TLB 條目失效。
    // 簡單來說，因為我們切換了整個使用者位址空間 (PGD)，所以需要讓所有相關的舊翻譯快取失效。
    asm volatile("tlbi vmalle1is" : : : "memory");

    // 再次執行 DSB ISH，確保 TLB 失效操作已完成。
    asm volatile("dsb ish" : : : "memory");

    // ISB (Instruction Synchronization Barrier): 清空處理器的管線 (pipeline)，
    // 確保所有後續執行的指令都會從快取或記憶體中重新獲取，
    // 並且使用最新的系統設定 (例如新的 TTBR0_EL1 和失效後的 TLB)。
    asm volatile("isb" : : : "memory");

    // uart_send_string("[MMU] User address space switched.\r\n");
}

// [For mmap]將 mmap 的 prot 旗標轉換為我們 PTE 的屬性
// 注意：這需要與你的 USER_CODE_ATTR, USER_DATA_STACK_ATTR 等定義的位元對應起來
uint64_t get_pte_attributes_from_prot(int prot, int flags) {
    uint64_t attributes = PD_PAGE | PD_ACCESS ; // 基本屬性

    // 記憶體類型 (假設匿名頁面都是 Normal Non-Cacheable)
    attributes |= (MAIR_IDX_NORMAL_NOCACHE << 2);

    // 存取權限 (AP bits for EL0)
    if (prot & PROT_WRITE) {
        attributes |= PD_USER_ACCESS; // EL0 R/W (0b01 << 6)
    } else if (prot & PROT_READ) {
        attributes |= (0b11UL << 6);  // EL0 R/O (0b11 << 6)
    } else {
        // 如果 PROT_NONE，則不設定 AP[1] (AP[2:1]=0b00)，但這通常還需要其他位元來完全禁止存取
        // 這裡簡化，如果沒有 READ，就認為是不可存取 (或者你可以定義一個 PROT_NONE 的專用屬性)
    }

    // 執行權限
    if (prot & PROT_EXEC) {
        // UXN 必須為 0 (不設定 PD_UXN)
    } else {
        attributes |= PD_UXN; // 不可執行
    }
    attributes |= PD_PXN; // 核心不應執行使用者 mmap 的區域

    return attributes;
}

// 在行程的 VMA 列表中尋找一個可用的虛擬位址區域的起始位址

uint64_t find_available_vma_start(thread_t *process, size_t length_aligned) {
    uint64_t current_search_addr = USER_VMA_AREA_START;
    struct vm_area_struct *vma = process->vma_list; // 假設 vma_list 是按起始位址排序的

    if (!vma) { // 如果沒有 VMA，直接返回起始搜尋位址
        if (USER_VMA_AREA_START + length_aligned <= USER_VMA_AREA_END) {
            return USER_VMA_AREA_START;
        } else {
            uart_send_string("Error: [find_available_vma_start] Address is not enough\r\n");
            return 0; // 空間不足
        }
    }

    // 檢查 USER_VMA_AREA_START 到第一個 VMA 之間的空間
    if (vma->vm_start >= current_search_addr + length_aligned) {
        return current_search_addr;
    }
    current_search_addr = vma->vm_end; // 從第一個 VMA 的結束處開始找

    while (vma->vm_next) {
        // 檢查目前 VMA 和下一個 VMA 之間的空隙
        if (vma->vm_next->vm_start >= current_search_addr + length_aligned) {
            return current_search_addr; // 找到足夠的空間
        }
        current_search_addr = vma->vm_next->vm_end;
        vma = vma->vm_next;
    }

    // 檢查最後一個 VMA 之後的空間
    if (USER_VMA_AREA_END >= current_search_addr + length_aligned) {
        return current_search_addr;
    }

    uart_send_string("[find_available_vma_start] No suitable VMA region found.\r\n");
    return 0; // 沒有找到合適的空間 (返回 0 表示失敗)
}
//for demand paging
void handle_page_fault(trap_frame_t *frame) {
    uint64_t far_el1; // Fault Address Register (EL1) - 儲存導致錯誤的虛擬位址
    uint64_t esr_el1; // Exception Syndrome Register (EL1) - 儲存錯誤的詳細資訊

    // 從系統暫存器讀取錯誤資訊
    asm volatile("mrs %0, far_el1" : "=r"(far_el1));
    asm volatile("mrs %0, esr_el1" : "=r"(esr_el1));

    thread_t *current_process = get_current(); // 獲取目前行程的指標
    if (!current_process) {
        uart_send_string("PANIC: Page Fault but no current process!\r\n");
        // 嚴重錯誤，通常會導致系統停機
        while(1);
    }

    uint64_t fault_va = far_el1; // 導致錯誤的虛擬位址
    uint32_t ec = (esr_el1 >> 26) & 0x3F; // 提取 Exception Class

    // 檢查故障虛擬位址是否屬於目前行程的某個 VMA
    // uart_send_string("[handle_page_fault]Test handle_page_handler1\r\n");
    // uart_send_string(" at VA 0x"); uart_send_hex(fault_va);uart_send_string("\r\n");
    struct vm_area_struct *vma = find_vma(current_process, fault_va);
    // uart_send_string("[handle_page_fault]Test handle_page_handler2\r\n");
    if (!vma) {
        // --- 情況 A: Segmentation Fault ---
        // 故障位址不屬於任何已定義的 VMA，這是一個非法的記憶體存取
        uart_send_string("[Segmentation fault]: Kill Process PID ");
        uart_send_int(current_process->id); // 假設 uart_send_int 存在
        uart_send_string(" at VA 0x"); uart_send_hex(fault_va);
        uart_send_string("\r\nESR_EL1: 0x"); uart_send_hex(esr_el1); uart_send_string("\r\n");
        
        user_thread_exit(frame,NULL);
        // 在真實系統中，這裡會呼叫 schedule()，並且這個行程不會再被執行
        // 這裡我們模擬，讓它在 eret 後可能再次 trap 或進入一個安全迴圈
        // 或者，如果你的 syscall 有 exit，可以呼叫類似 sys_exit 的邏輯
        // frame->elr_el1 = SOME_SAFE_EXIT_POINT_IN_USER_SPACE; // 不太好
        // 最簡單的是讓這個行程不再被排程
        return;
    }

    // --- 情況 B: Demand Paging / Translation Fault ---
    // 故障位址屬於一個合法的 VMA，但對應的實體頁面尚未映射
    // (我們假設這是一個匿名頁面，因為實驗只要求這個)

    // 打印 Translation fault 日誌
    uart_send_string("[Translation fault]: 0x"); uart_send_hex(fault_va);
    uart_send_string(" in VMA [0x"); uart_send_hex(vma->vm_start);
    uart_send_string("-0x"); uart_send_hex(vma->vm_end);
    uart_send_string("] for PID "); uart_send_int(current_process->id);
    uart_send_string("\r\n");

    // 1. 計算故障虛擬位址所在的頁面的起始位址 (頁對齊)
    uint64_t fault_page_va = fault_va & ~(PAGE_SIZE - 1);
    uint64_t map_page_pa;
    // // 2. 分配一個新的實體頁框
    // //    dynamic_malloc 返回核心虛擬位址 (KVA)
    // uint64_t new_page_kva = (uint64_t)dynamic_malloc(PAGE_SIZE);
    // if (!new_page_kva) {
    //     uart_send_string("PANIC: [Page Fault] Failed to allocate physical page for VA 0x");
    //     uart_send_hex(fault_page_va); uart_send_string(". Killing process.\r\n");
    //     // 處理記憶體不足的嚴重錯誤，終止行程
    //     current_process->state = THREAD_DEAD;
    //     // schedule();
    //     while(1);
    //     return;
    // }

    // // 3. 初始化頁框內容 (對於匿名頁面，清零)
    // //    核心透過 KVA 操作這個新分配的頁框
    // memset((void*)new_page_kva, 0, PAGE_SIZE);

    // // 4. 將 KVA 轉換為 PA，用於填寫 PTE
    // uint64_t new_page_pa = KVA_TO_PHYS(new_page_kva);

    // uart_send_string("  Allocated new page: PA 0x"); uart_send_hex(new_page_pa);
    // uart_send_string(" (KVA 0x"); uart_send_hex(new_page_kva);
    // uart_send_string(") for VA 0x"); uart_send_hex(fault_page_va); uart_send_string("\r\n");

    // 5. 獲取該 VMA 的 PTE 屬性
    uint64_t pte_attributes = get_pte_attributes_from_prot(vma->vm_prot, vma->vm_flags);
    uart_send_string("[handle_page_fault] pte_attributes=");
    uart_send_hex(pte_attributes);
    uart_send_string("\r\n");
    uint64_t offset_page;
    switch (vma->vm_area_tag) {
        case VMA_AREA_CODE:
            uart_send_string("[handle_page_fault]Fault in CODE VMA. Loading from backing store PA: 0x");
            uart_send_hex(current_process->user_code_start_pa); uart_send_string("\r\n");
            offset_page=fault_page_va-vma->vm_start;
            map_page_pa=current_process->user_code_start_pa+offset_page;
            break;
        case VMA_AREA_STACK:
            uart_send_string("[handle_page_fault]Fault in STACK VMA. Loading from backing store PA: 0x");
            uart_send_hex(current_process->user_code_start_pa); uart_send_string("\r\n");
            offset_page=fault_page_va-vma->vm_start;
            map_page_pa=KVA_TO_PHYS(current_process->thread_stack_alloc_kva)+offset_page;
            break;
        case VMA_AREA_FRAMEBUFFER:
            uart_send_string("[handle_page_fault]Fault in Framebuffer VMA. Loading from backing store PA: 0x");
            uart_send_hex(current_process->user_code_start_pa); uart_send_string("\r\n");
            offset_page=fault_page_va-vma->vm_start;
            map_page_pa=offset_page+vma->vm_start;
            break;
        case VMA_AREA_NONE:
            uart_send_string("[handle_page_fault]Fault in anonymous pages PA: 0x");
            uart_send_hex(vma->vm_start); uart_send_string("\r\n");
            //  2. 分配一個新的實體頁框
            uint64_t new_page_kva = (uint64_t)dynamic_malloc(PAGE_SIZE);
            if (!new_page_kva) {
                uart_send_string("PANIC: [Page Fault] Failed to allocate physical page for VA 0x");
                uart_send_hex(fault_page_va); uart_send_string(". Killing process.\r\n");
                // 處理記憶體不足的嚴重錯誤，終止行程
                current_process->state = THREAD_DEAD;
                // schedule();
                user_thread_exit(frame,get_current());
                return;
            }
            // 3. 初始化Frame內容
            memset((void*)new_page_kva, 0, PAGE_SIZE);
            // 4. 將 KVA 轉換為 PA，用於填寫 PTE
            uint64_t new_page_pa = KVA_TO_PHYS(new_page_kva);
            map_page_pa=new_page_pa;
            break;
        default:
            uart_send_string("[handle_page_fault] Unknown VMA_AREA\r\n");
            break;
    }

    // 6. 建立/更新分頁表映射 (只映射這一個出錯的頁面)
    //    current_process->user_pgd_pa 應該儲存該行程 PGD 的實體位址
    if (mappages(get_current_ttbr0_el1(), fault_page_va, PAGE_SIZE, map_page_pa, pte_attributes) != 0) {
        uart_send_string("PANIC: [Page Fault] mappages failed for VA 0x");
        uart_send_hex(fault_page_va); uart_send_string(". Killing process.\r\n");
        // dynamic_free((void*)new_page_kva); // 釋放剛分配的頁框
        current_process->state = THREAD_DEAD;
        // schedule();
        while(1);
        return;
    }

    uart_send_string("  Page mapped successfully. Resuming user process at ELR 0x");
    uart_send_hex(frame->elr_el1); // ELR_EL1 應該是 fault_va 所在的指令
    uart_send_string("\r\n");

    // 不需要修改 frame->elr_el1。
    // 從異常處理常式 `eret` 返回後，CPU 會自動重新執行導致錯誤的那條指令。
    switch_user_address_space(get_current_ttbr0_el1());
    // `switch_user_address_space` 中的 `tlbi vmalle1is` 在行程切換時會清空 TLB，
    // 但在同一個行程內發生 page fault 並修復後，可能需要更精確的 TLB 操作，
    // 或者依賴 CPU 自動處理。為了簡化，我們先假設 CPU 重試時會看到新的映射。
}

struct vm_area_struct* find_vma(thread_t *process, uint64_t addr) {
    if (!process) return NULL;
    uart_send_string("[handle_page_fault]Test handle_page_handler3\r\n");
    struct vm_area_struct *vma = process->vma_list;
    uart_send_string("[handle_page_fault]Test handle_page_handler4\r\n");
    while (vma) {
        uart_send_string("[handle_page_fault] vma address");
        uart_send_hex((uint64_t)vma);
        uart_send_string("\r\n");
        uart_send_string("[handle_page_fault] from=");
        uart_send_hex(vma->vm_start);
        uart_send_string(" to=");
        uart_send_hex(vma->vm_end);
        uart_send_string("\r\n");
        if (addr >= vma->vm_start && addr < vma->vm_end) {
            return vma;
        }
        vma = vma->vm_next;
    }
    uart_send_string("[handle_page_fault]Test handle_page_handler5\r\n");
    return NULL; // 未找到
}