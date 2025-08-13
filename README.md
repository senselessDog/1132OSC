# Lab 6: Virtual Memory Implementation

This lab transitions the operating system from a direct physical memory model to a full virtual memory system. This involves initializing the Memory Management Unit (MMU), creating isolated address spaces for the kernel and user processes, and implementing advanced memory management techniques like demand paging and the `mmap` system call.

---

## Basic Exercise 1 - Virtual Memory in Kernel Space

This foundational exercise focused on enabling the MMU and migrating the kernel to its own protected, high-memory address space.

### Implementation Details

* **MMU Configuration (`mmu.h`, `mmu_init.S`):**
    * **Translation Control Register (`TCR_EL1`):** The TCR was configured to use a 48-bit virtual address space and a 4KB page granularity for both kernel (`TTBR1_EL1`) and user (`TTBR0_EL1`) spaces.
    * **Memory Attribute Indirection Register (`MAIR_EL1`):** Two primary memory types were defined in the MAIR. `MAIR_DEVICE_nGnRnE` is used for memory-mapped I/O peripherals to prevent reordering and speculation, while `MAIR_NORMAL_NOCACHE` is used for general-purpose RAM.

* **Page Table Setup and Kernel Mapping (`mmu_init.S`, `linker_kernel.ld`):**
    * **Linker Script:** The kernel's linker script (`linker_kernel.ld`) was modified to place the entire kernel at a high virtual address, starting from `0xffff000000080000`. This separates the kernel's address space from the user's.
    * **Identity Mapping:** An initial identity mapping is first created for the first few gigabytes of physical memory. This is crucial because the MMU must be enabled while the CPU is still executing code at a low physical address.
    * **Finer-Grained Mapping:** This initial mapping is quickly replaced by a more precise three-level page table (PGD -> PUD -> PMD) that linearly maps the physical RAM and peripherals to the high kernel virtual address space (i.e., `virtual_addr = physical_addr + 0xffff000000000000`). This mapping uses 2MB blocks (PMD entries) and correctly assigns the "Normal" attribute to RAM and the "Device" attribute to the MMIO region, preventing alignment faults.
    * **Enabling the MMU:** After the page tables are set up, `ttbr1_el1` is loaded with the physical address of the kernel's PGD. The MMU is then enabled by setting the `M` bit in `sctlr_el1`. An indirect branch (`br`) is immediately used to jump to a symbol in the high virtual address space, completing the transition.
    * **Address Translation Macros (`mmu.h`):** The `PHYS_TO_KVA` and `KVA_TO_PHYS` macros were introduced to easily convert between physical and kernel virtual addresses throughout the kernel code.

---

## Basic Exercise 2 - Virtual Memory in User Space

This exercise built upon the kernel's virtual memory to provide each user process with its own completely isolated address space.

### Implementation Details

* **Per-Process Page Tables (`thread.h`, `mmu.c`):**
    * Each process's `thread_t` structure now contains the physical address of its own top-level page table (PGD).
    * The `trap_frame_t` was updated to include `ttbr0_el1`, ensuring that the correct user address space is restored upon returning from an exception.

* **User Space Mapping (`mmu.c`):**
    * The core of user space management is the `mappages()` function. This function can map a range of virtual addresses to physical addresses for any given PGD.
    * It works by "walking" the four-level page tables (PGD -> PUD -> PMD -> PTE). If an intermediate table (like a PUD or PMD) doesn't exist during the walk, `mappages()` allocates a new page frame for it and links it into the hierarchy.
    * This function is used to create the initial memory layout for a new user process, mapping its program code to virtual address `0x0` and creating a user stack at a high virtual address (`0xfffffffff000`).

* **Context Switching with Address Spaces (`process.c`, `timeout.c`):**
    * The context switch logic was significantly updated. When switching from process A to process B, the kernel now performs these critical steps:
        1.  Saves process A's registers.
        2.  Loads the physical address of process B's PGD from its `thread_t` struct.
        3.  Executes `msr ttbr0_el1, x0` to switch the CPU's active user address space to process B's.
        4.  Issues a `tlbi vmalle1is` (TLB Invalidate) instruction to flush any stale address translations from the previous process.
        5.  Loads process B's registers and returns from the exception.
    * This is encapsulated in the `switch_user_address_space()` function, which is called during every preemptive context switch (`user_timeout_handler`) and after a process exits (`user_thread_exit`).

* **Revisiting System Calls (`syscall.c`):**
    * **`fork()`:** The `fork()` system call is now a deep operation. It creates a new address space for the child, allocates new physical pages for the child's code and stack, and manually copies the data from the parent's physical pages to the child's. This ensures true process isolation.
    * **`mbox_call()`:** Since the mailbox hardware operates on physical addresses, any pointers passed from the user (which are virtual addresses) must first be translated by the kernel to their corresponding kernel virtual address, and then to a physical address using `KVA_TO_PHYS()` before being written to the mailbox registers.

---

## Advanced Exercise 1 - `mmap`

The `mmap` system call was implemented to allow processes to dynamically request memory regions, laying the groundwork for features like heap allocation and demand paging.

### Implementation Details

* **Virtual Memory Areas (VMAs) (`mmu.h`, `thread.c`):**
    * A `vm_area_struct` (VMA) was introduced. Each VMA represents a contiguous region of virtual memory with defined start/end addresses and access permissions (`PROT_READ`, `PROT_WRITE`, `PROT_EXEC`).
    * Each `thread_t` now has a `vma_list` pointer, which is the head of a linked list of all VMAs for that process.

* **`sys_mmap()` Implementation (`syscall.c`, `mmu.c`):**
    * A new system call, `sys_mmap` (syscall #10), was added.
    * When called, it first searches the process's VMA list to find an unallocated virtual address range large enough for the request (`find_available_vma_start`).
    * It then allocates a new `vm_area_struct`, populates it with the properties of the new mapping (address, size, protection flags), and adds it to the process's VMA list.
    * **Crucially, `sys_mmap` itself does not allocate any physical memory or modify page tables.** It only records the *intent* to use a virtual memory region. The actual physical allocation is deferred until the memory is first accessed, which is the core principle of demand paging.

---

## Advanced Exercise 2 - Demand Paging

Demand paging was implemented to make memory allocation efficient, allocating physical page frames only when a process actually tries to access a virtual page.

### Implementation Details

* **Page Fault Handler (`exception_entry.c`, `mmu.c`):**
    * The main exception entry point, `sync_lower_el_64_entry`, was enhanced to detect **Data Abort** exceptions, which signal a page fault.
    * When a fault occurs, it calls the `handle_page_fault(frame)` function, passing the trapped process's context.

* **Fault Resolution Logic (`mmu.c`):**
    * The fault handler retrieves the faulting virtual address from the `far_el1` (Fault Address Register).
    * It then calls `find_vma()` to search the current process's VMA list to see if the faulting address falls within a legally defined memory region.
    * **Segmentation Fault:** If `find_vma()` returns `NULL`, the access is invalid. The kernel prints a "Segmentation fault" message and terminates the process.
    * **Valid Page Fault:** If a VMA is found, the access is legal, and the fault is a demand paging request. The kernel then:
        1.  Allocates a single new physical page frame from the buddy allocator.
        2.  Calls `mappages()` to create the necessary PTE to map the faulting virtual page to the newly allocated physical frame. The page permissions (read/write/execute) are set according to the `vm_prot` flags stored in the VMA.
        3.  Returns from the exception. The CPU automatically re-executes the instruction that caused the fault, which now succeeds.
