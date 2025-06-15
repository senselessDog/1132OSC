# Lab 4: Allocator

This project transitions from a simple bump allocator to a sophisticated two-level memory management system. It features a buddy system for physical page frame allocation and a slab-like dynamic allocator for handling arbitrary-sized memory requests. The system is initialized at startup and reserves memory for critical kernel components.

## Basic Exercise 1: Buddy System

**Goal:** Implement a page frame allocator for allocating and freeing contiguous blocks of physical memory in powers-of-two multiples of the base page size (4KB).

### Implementation Details:

- **Core Data Structures:** A central `buddy_system_t` structure holds the allocator's state. The physical memory is conceptually divided into pages. The state of these pages is tracked using an array of `buddy_block_list_t` for each possible block size (order). These lists serve a dual purpose: tracking the status of each block (`BLOCK_ALLOCATED`, `BLOCK_SPLIT`, etc.) and forming a doubly-linked list of free blocks for each order.

- **Allocation Logic (`buddy_malloc`):** When a request is made, the required order is calculated. The allocator searches for a free block starting from the smallest fitting order. If an exact match is found, it's allocated. If a larger block is found, the `buddy_split` function is called recursively to break the large block down until a block of the desired order is created. The redundant "buddy" block from each split is added to the corresponding free list.

- **Freeing and Coalescing (`buddy_free`):** When a block is freed, the allocator calculates its address and order. It then checks the status of its "buddy" block. If the buddy is also free, they are coalesced into a single, larger block of the next higher order. This merging process continues iteratively up the orders, ensuring that the largest possible contiguous free blocks are always available.

- **Logging:** All significant actions, such as splitting blocks, merging blocks, and allocating/freeing memory, are printed to the UART console for observability.

## Basic Exercise 2: Dynamic Memory Allocator

**Goal:** Implement a general-purpose dynamic memory allocator that can handle requests of any size by building upon the buddy system.

### Implementation Details:

- **Two-Level Approach:** The implementation uses a two-level strategy. Large allocation requests (greater than 2048 bytes) are passed directly to the buddy system allocator.

- **Fixed-Size Pools:** For small allocations, a set of fixed-size memory pools is created (for sizes like 16, 32, 64, ..., 2048 bytes). When `dynamic_malloc` is called with a small size, it rounds up to the nearest pool size and serves the request from there.

- **On-Demand Page Allocation:** If a requested fixed-size pool is empty, the dynamic allocator requests a new 4KB page from the buddy system. This new page is then partitioned into multiple smaller chunks corresponding to the pool's block size, and these chunks are added to that pool's free list.

- **Freeing Logic (`dynamic_free`):** When a pointer is freed, the allocator determines if it was a large allocation or a small one. It achieves this by checking a global array that maps each physical page to the pool index it serves. If it's a small allocation, the block is returned to its corresponding pool's free list. If it was a large allocation, `buddy_free` is called directly.

## Advanced Exercise 1: Efficient Page Allocation

**Goal:** Ensure that allocation and deallocation operations are efficient, targeting O(log n) complexity.

### Implementation Details:

- **Constant-Time List Access:** By maintaining a separate doubly-linked free list for each order, adding or removing a block from a list is a constant-time operation.

- **Logarithmic Search/Merge:** Finding an available block involves iterating up through the orders, and coalescing involves iterating up as well. Since the number of orders is the logarithm of the total number of pages, both allocation and freeing operations achieve O(log n) complexity.

## Advanced Exercise 2: Reserved Memory

**Goal:** Create an API to prevent the allocator from using memory regions that are already occupied by the kernel or other firmware components at boot time.

### Implementation Details:

- **`memory_reserve` API:** A `memory_reserve` function was implemented. It takes a start and end physical address as arguments.

- **Marking as Allocated:** The function works by iterating through all the 4KB pages within the given range. For each page, it finds the corresponding block in the buddy system's data structures, splits larger blocks if necessary to isolate the single page, and then marks it as `BLOCK_ALLOCATED`, effectively removing it from the pool of available memory.

## Advanced Exercise 3: Startup Allocation

**Goal:** Solve the "chicken-or-the-egg" problem where the main allocator needs dynamic memory for its own metadata before it is ready to allocate anything.

### Implementation Details:

- **Using the Simple Allocator:** The `buddy_init` function, which initializes the entire memory management system, uses the old `simple_alloc` bump allocator from Lab 2. This startup allocator carves out a piece of memory from a pre-defined heap region to store the buddy system's metadata arrays and free lists.

- **Boot-Time Reservation:** Once the buddy system's structures are in memory, `buddy_init` calls `reserve_system_memory`. This function uses the `memory_reserve` API to mark all critical, pre-existing memory regions as allocated. This includes:
  - Spin tables for multi-core boot.
  - The kernel's own image (.text, .data, .bss).
  - The initramfs CPIO archive.
  - The Flattened Devicetree (DTB) blob.

- **Handoff:** After these regions are reserved, the `simple_alloc` heap is abandoned, and all subsequent memory allocations in the kernel are handled by the now fully operational buddy and dynamic allocators.
