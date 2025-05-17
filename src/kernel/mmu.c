#include<mmu.h>
#include<buddy_alloc.h>
#include<uart.h>
#include<strcmp.h>
#include<thread.h>
static int first_thread=1;
static inline uint64_t va_to_index(uint64_t va, int level_shift) {
    return (va >> level_shift) & TABLE_INDEX_MASK;
}

uint64_t* walk_and_create_pte(uint64_t pgd_phys, uint64_t va, int alloc) {
    uint64_t* current_table_phys = (uint64_t*)pgd_phys;
    uint64_t  next_table_base_phys;
    uint64_t* entry_phys_ptr;

    // Level 1: PGD -> PUD
    entry_phys_ptr = &current_table_phys[va_to_index(va, PGD_SHIFT)];
    if (!(*entry_phys_ptr & PD_TABLE)) { // Check if valid table descriptor
        if (!alloc) return NULL;
        next_table_base_phys = (uint64_t)(dynamic_malloc(4096)-kernel_virtual_offset);
        if (!next_table_base_phys) return NULL; // Allocation failed
        memset((void*)next_table_base_phys, 0, PAGE_SIZE);
        *entry_phys_ptr = next_table_base_phys | PD_TABLE;
    }
    current_table_phys = (uint64_t*)((*entry_phys_ptr) & TABLE_ADDR_MASK);

    // Level 2: PUD -> PMD
    entry_phys_ptr = &current_table_phys[va_to_index(va, PUD_SHIFT)];
    if (!(*entry_phys_ptr & PD_TABLE)) {
        if (!alloc) return NULL;
        next_table_base_phys = (uint64_t)(dynamic_malloc(4096)-kernel_virtual_offset);
        if (!next_table_base_phys) return NULL;
        memset((void*)next_table_base_phys, 0, PAGE_SIZE);
        *entry_phys_ptr = next_table_base_phys | PD_TABLE;
    }
    current_table_phys = (uint64_t*)((*entry_phys_ptr) & TABLE_ADDR_MASK);

    // Level 3: PMD -> PTE Table
    entry_phys_ptr = &current_table_phys[va_to_index(va, PMD_SHIFT)];
    if (!(*entry_phys_ptr & PD_TABLE)) {
        if (!alloc) return NULL;
        next_table_base_phys = (uint64_t)(dynamic_malloc(4096)-kernel_virtual_offset);
        if (!next_table_base_phys) return NULL;
        memset((void*)next_table_base_phys, 0, PAGE_SIZE);
        *entry_phys_ptr = next_table_base_phys | PD_TABLE;
    }
    current_table_phys = (uint64_t*)((*entry_phys_ptr) & TABLE_ADDR_MASK);

    // Now current_table_phys points to the base of the PTE table
    // Return pointer to the specific PTE entry
    return &current_table_phys[va_to_index(va, PTE_SHIFT)];
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
#define USER_CODE_VA         0x00000000UL
#define USER_STACK_TOP_VA    0xfffffffff000UL // Top of a 16KB stack (4 pages)
#define USER_STACK_SIZE      (4 * PAGE_SIZE)     // 16KB
#define USER_STACK_BOTTOM_VA 0xffffffffb000UL


void run_user_vm(char *archive_phys_addr) {
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

    uart_send_string("[run_user_vm] Archive PA: 0x"); uart_send_hex((uint64_t)archive_phys_addr); uart_send_string("\r\n");
    uart_send_string("[run_user_vm] Program: "); uart_send_string(filename); uart_send_string("\r\n");

    struct file_information program_info = find_program_in_initramfs(archive_phys_addr, filename);
    if (!program_info.filecontext || program_info.filesize <= 0) {
        uart_send_string("Error: [run_user_vm] Program not found or empty: "); uart_send_string(filename); uart_send_string("\r\n");
        return;
    }
    uart_send_string("[run_user_vm]Program found. PA: 0x"); uart_send_hex((uint64_t)program_info.filecontext);
    uart_send_string(", Size: "); uart_send_hex(program_info.filesize); uart_send_string(" bytes\r\n");


    // 1. Allocate PGD for the new user address space
    uint64_t user_pgd_virtual = (uint64_t)dynamic_malloc(4096);
    uint64_t user_pgd_phys=(uint64_t)(user_pgd_virtual-kernel_virtual_offset);
    if (!user_pgd_phys) {
        uart_send_string("Error:[run_user_vm] Failed to allocate PGD for user space!\r\n");
        return;
    }
    memset((void*)user_pgd_phys, 0, PAGE_SIZE); // Clear PGD
    uart_send_string("[run_user_vm] User PGD allocated at PA: 0x"); uart_send_hex(user_pgd_phys); uart_send_string("\r\n");

    // 2. Map user program's code
    // Assuming program_info.filecontext is the PHYSICAL address of the code
    void * user_base=dynamic_malloc(user_space_size);
    // uart_send_string("Create parent thread: ");
    // thread_t * parent_thread=thread_create(user_base,NULL);
    // uart_send_string("Create parent thread success\r\n");
    void *user_stack=user_base+user_space_size;
    
    memcpy(user_base,(const void *)program_info.filecontext, (uint32_t)program_info.filesize);
    if (mappages(user_pgd_phys, USER_CODE_VA, (uint64_t)user_space_size, (uint64_t)user_base-kernel_virtual_offset, USER_CODE_ATTR) != 0) {
        uart_send_string("[run_user_vm] Failed to map user code!\r\n");
        // Potentially free PGD and other allocated tables here
        return;
    }
    uart_send_string("[run_user_vm] User code mapped: VA 0x0 to PA 0x"); uart_send_hex((uint64_t)user_base);
    uart_send_string(" (size 0x"); uart_send_hex(user_space_size); uart_send_string(")\r\n");


    // 3. Allocate and map user stack (4 pages = 16KB)
    uint64_t user_stack_pa_pages = (uint64_t)dynamic_malloc(PAGE_SIZE*4);
    if (!user_stack_pa_pages){
        uart_send_string("Error: [run_user_vm] user_stack_pa_pages can't alloc");
    }
    // No need to memset stack pages, user program will use them.
    uart_send_string("User stack page ");
    uart_send_string(" allocated at PA: 0x"); uart_send_hex(user_stack_pa_pages); uart_send_string("\r\n");

        // Map this single page
        // Stack grows downwards, so USER_STACK_BOTTOM_VA is the start of the VA range.
        // We map pa_pages[0] to USER_STACK_BOTTOM_VA, pa_pages[1] to USER_STACK_BOTTOM_VA + PAGE_SIZE, etc.
    if (mappages(user_pgd_phys, USER_STACK_BOTTOM_VA, PAGE_SIZE*4, user_stack_pa_pages-kernel_virtual_offset, USER_DATA_STACK_ATTR) != 0) {
        uart_send_string("Error: [run_user_vm]Failed to map user stack page!\r\n");
        // Potentially free PGD and other allocated tables/pages here
        return;
    }
    uart_send_string("[run_user_vm]User stack mapped: VA 0x"); uart_send_hex(USER_STACK_BOTTOM_VA);
    uart_send_string(" - 0x"); uart_send_hex(USER_STACK_TOP_VA -1); uart_send_string("\r\n");


    // 4. Setup and Switch to EL0
    if (first_thread){
        thread_init_user();   
    }
    // The stack pointer SP_EL0 should point to the top of the allocated stack region.
    if (first_thread){
        first_thread=0;
        switch_to_el0_vm(user_pgd_phys, (void*)USER_CODE_VA, (void*)USER_STACK_TOP_VA);
    }

    // Kernel should not reach here after eret if switch is successful
    // uart_send_string("Error: Returned from switch_to_el0_vm unexpectedly!\r\n");
}