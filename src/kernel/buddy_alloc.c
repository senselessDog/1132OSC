#include "buddy_alloc.h"
#include "uart.h"
#include "devicetree.h"
// Calculate the required order for a given size
int size_to_order(size_t size) {
    int pages = (size + PAGE_SIZE - 1) / PAGE_SIZE;  // Ceiling division
    int order = 0;
    
    // Find the smallest order that can accommodate the requested size
    while ((1 << order) < pages) {
        order++;
    }
    
    return order;
}

void buddy_init(void) {
    if (buddy_system != NULL) {
        uart_send_string("Buddy system already initialized\n");
        return;
    }
    
    // Calculate total memory size and pages
    uint32_t memory_size = BUDDY_MEMORY_END - BUDDY_MEMORY_START;
    uint32_t total_pages = memory_size / PAGE_SIZE;
    
    // Allocate space for buddy system metadata
    buddy_system = (buddy_system_t*)simple_alloc(sizeof(buddy_system_t));
    
    // Initialize buddy system fields
    buddy_system->memory_start = (void*)BUDDY_MEMORY_START;
    buddy_system->total_pages = total_pages;
    
    // Calculate starting point for buddy lists
    buddy_system->buddy_list = (buddy_block_list_t**)simple_alloc(sizeof(buddy_block_list_t*) * MAX_ORDER);
    uint32_t offset = 0;
    // Initialize lists for each order
    for (int order = 0; order < MAX_ORDER; order++) {
        // Initialize blocks for this order
        int num_blocks = total_pages / (1 << order);
        uart_send_string("\r\nBuddy system order ");
        uart_send_int(order);
        uart_send_string(": ");
        uart_send_int(num_blocks);
        uart_send_string(" blocks\r\n");
        buddy_system->list_addr[order] = (void*)simple_alloc(sizeof(buddy_block_list_t) *num_blocks);
        buddy_system->buddy_list[order] = buddy_system->list_addr[order];
        
        // 更新下一個order的偏移量
        offset += sizeof(buddy_block_list_t) * num_blocks;

        for (int i = 0; i < num_blocks; i++) {
            buddy_block_list_t* block = &buddy_system->buddy_list[order][i];
            
            // For the highest order, set blocks as available
            if (order == MAX_ORDER - 1) {
                block->val = order;  // Mark as free with this order
                block->next = (i < num_blocks - 1) ? &buddy_system->buddy_list[order][i + 1] : NULL;
                buddy_system->first_avail[order] = 0;  // First block is available
            } else {
                // For lower orders, mark as belonging to higher orders
                block->val = BLOCK_BELONGS;
                block->next = NULL;
                buddy_system->first_avail[order] = -1;  // No blocks available yet
            }
        }
        uart_send_string("Buddy system simple_allocated to ");
        uart_send_hex((uint32_t)buddy_system->list_addr[order]);
    }
    
    
    uart_send_string("\r\nBuddy system initialized with ");
    uart_send_int(total_pages);
    uart_send_string(" pages\r\n");
    
    uart_send_string("\r\nMemory range: 0x");
    uart_send_hex(BUDDY_MEMORY_START);
    uart_send_string(" - 0x");
    uart_send_hex(BUDDY_MEMORY_END);
    uart_send_string("\r\n");
    //reserve memory
    reserve_system_memory();
}

// Split a block of a given order into two blocks of the next lower order
void buddy_split(int order, int start_idx, int requested_order) {
    if (order <= requested_order) {
        return;  // Already at the requested order
    }
    uart_send_string("\r\nSplitting block: order=");
    uart_send_int(order);
    uart_send_string(", index=");
    uart_send_int(start_idx);
    uart_send_string("\r\n");
    //parent remove from free list
    if (buddy_system->first_avail[order] == start_idx) {
        // If this is the first available block, update to the next one
        buddy_system->first_avail[order] = (buddy_system->buddy_list[order][start_idx].next != NULL) ? 
            // 使用安全的指針運算方式
            (buddy_system->buddy_list[order][start_idx].next -buddy_system->buddy_list[order]) : -1;
    } else {
        // Otherwise, find the block in the list and remove it
        buddy_block_list_t* prev = &buddy_system->buddy_list[order][buddy_system->first_avail[order]];
        while (prev->next != &buddy_system->buddy_list[order][start_idx]) {
            prev = prev->next;
        }
        prev->next = buddy_system->buddy_list[order][start_idx].next;
    }

    // Mark the current block as split
    buddy_system->buddy_list[order][start_idx].val = BLOCK_SPLIT;
    
    // Calculate indices for the two child blocks at the lower order
    int child_order = order - 1;
    int left_child_idx = start_idx * 2;
    int right_child_idx = left_child_idx + 1;
    
    // Mark left child as free
    buddy_system->buddy_list[child_order][left_child_idx].val = child_order;
    buddy_system->buddy_list[child_order][left_child_idx].next = NULL;
    
    // Update the free list for the child order
    if (buddy_system->first_avail[child_order] == -1) { //no free blocks in this order
        buddy_system->first_avail[child_order] = left_child_idx;
    } else {
        // Find the last element in the list
        buddy_block_list_t* current = &buddy_system->buddy_list[child_order][buddy_system->first_avail[child_order]];
        while (current->next != NULL) {
            current = current->next;
        }
        current->next = &buddy_system->buddy_list[child_order][left_child_idx];
    }
    
    // Mark right child as free
    buddy_system->buddy_list[child_order][right_child_idx].val = child_order;
    buddy_system->buddy_list[child_order][right_child_idx].next = NULL;
    
    // Add right child to the free list
    buddy_block_list_t* last = &buddy_system->buddy_list[child_order][left_child_idx];
    last->next = &buddy_system->buddy_list[child_order][right_child_idx];
    return;
}

// Mark a block as allocated and remove it from the free list
void mark_allocated(int order, int start_idx) {
    if (buddy_system->buddy_list[order][start_idx].val== BLOCK_ALLOCATED){
        // Block is already allocated, no need to mark again
        uart_send_string("\r\nBlock already split, skipping\r\n");
        return;
    }
    //printf("Marking block as allocated: order=%d, index=%d\n", order, start_idx);
    uart_send_string("\r\nMarking block as allocated: order=");
    uart_send_int(order);
    uart_send_string(", index=");
    uart_send_int(start_idx);
    uart_send_string("\r\n");
    
    // Mark the block as allocated
    buddy_system->buddy_list[order][start_idx].val = BLOCK_ALLOCATED;
    
    // Update the free list
    if (buddy_system->first_avail[order] == start_idx) {
        // If this was the first available block, update to the next one
        buddy_system->first_avail[order] = (buddy_system->buddy_list[order][start_idx].next != NULL) ? 
            ((int)(buddy_system->buddy_list[order][start_idx].next - buddy_system->buddy_list[order])) : -1;
        // uart_send_string("In allocated Now next and add= \r\n");
        // uart_send_hex((buddy_system->buddy_list[order][start_idx].next));
        // uart_send_string("\r\n");
        // uart_send_hex(buddy_system->buddy_list[order]);
        // uart_send_string("\r\n");
        // uart_send_int((int)(buddy_system->buddy_list[order][start_idx].next - buddy_system->buddy_list[order]));
        // uart_send_string("\r\n");
        // uart_send_int((int)(buddy_system->buddy_list[order][start_idx].next - buddy_system->buddy_list[order]) / sizeof(buddy_block_list_t));
        // uart_send_string("\r\n");
    } else {
        // Otherwise, find the block in the list and remove it
        buddy_block_list_t* prev = &buddy_system->buddy_list[order][buddy_system->first_avail[order]];
        while (prev->next != &buddy_system->buddy_list[order][start_idx]) {
            prev = prev->next;
        }
        prev->next = buddy_system->buddy_list[order][start_idx].next;
    }
    
    // Clear the next pointer
    buddy_system->buddy_list[order][start_idx].next = NULL;
}

// Allocate memory from the buddy system
void* buddy_malloc(size_t size) {
    // if (buddy_system == NULL) {
    //     buddy_init();
    // }
    
    if (size == 0) {
        return NULL;
    }
    
    // Calculate the required order
    int requested_order = size_to_order(size);
    
    // Check if the requested size is too large
    if (requested_order >= MAX_ORDER) {
        uart_send_string("Requested size too large!");
        return NULL;
    }
    //"Allocating %zu bytes (order %d)\n", size, requested_order
    uart_send_string("Allocating ");
    uart_send_int(size);
    uart_send_string(" bytes (order ");
    uart_send_int(requested_order);
    uart_send_string(")\r\n");
    
    // Find the smallest available order that can satisfy the request
    int order;
    for (order = requested_order; order < MAX_ORDER; order++) {
        if (buddy_system->first_avail[order] != -1) {
            break;
        }
    }
    
    // If no suitable block was found
    if (order >= MAX_ORDER) {
        //printf("No suitable block available for size %zu\n", size);
        uart_send_string("No suitable block available for size ");
        uart_send_int(size);
        uart_send_string("\r\n");
        return NULL;
    }
    
    // Get the index of the first available block
    int block_idx = buddy_system->first_avail[order];
    
    // If the block is larger than needed, split it
    if (order > requested_order) {
        for (int i = order; i > requested_order; i--) {
            // Split the block into two smaller blocks
            buddy_split(i, block_idx, requested_order);
            
            // Update the block index to point to the left child
            block_idx = block_idx * 2;
        }
        // After splitting, use the smallest block that fits the request
        block_idx = buddy_system->first_avail[requested_order];
        order = requested_order;
    }
    // uart_send_string("After split: [0][0] address=");
    // uart_send_hex(&buddy_system->buddy_list[0]);
    // uart_send_string(", next=");
    // uart_send_hex(buddy_system->buddy_list[0][0].next);
    // uart_send_string("\r\n");
    // Mark the block as allocated
    mark_allocated(order, block_idx);
    // uart_send_string("After mark_allocated: [0][0] address=");
    // uart_send_hex(&buddy_system->buddy_list[0][0]);
    // uart_send_string(", next=");
    // uart_send_hex(buddy_system->buddy_list[0][0].next);
    // uart_send_string("\r\n");
    // uart_send_string("After mark_allocated: First_alloc[order]=");
    // uart_send_int(buddy_system->first_avail[order]);
    // uart_send_string("\r\n");
    // Calculate the physical address
    void* addr = (void*)((uint32_t)buddy_system->memory_start + block_idx * (1 << order) * PAGE_SIZE);
    //printf("Allocated memory at 0x%x (order %d, index %d)\n", (uint32_t)addr, order, block_idx);
    uart_send_string("Allocated memory at 0x");
    uart_send_hex((uint32_t)addr);
    uart_send_string(" (order ");
    uart_send_int(order);
    uart_send_string(", index ");
    uart_send_int(block_idx);
    uart_send_string(")\r\n");
    
    
    return addr;
}

// Free a previously allocated memory block
void buddy_free(void* addr) {
    if (buddy_system == NULL || addr == NULL) {
        return;
    }
    
    // Check if the address is within our memory range
    if ((uint32_t)addr < (uint32_t)buddy_system->memory_start || 
        (uint32_t)addr >= (uint32_t)buddy_system->memory_start + buddy_system->total_pages * PAGE_SIZE) {
        //printf("Invalid address to free: 0x%x\n", (uint32_t)addr);
        uart_send_string("Invalid address to free: 0x");
        uart_send_hex((uint32_t)addr);
        uart_send_string("\r\n");
        return;
    }
    
    // Calculate the page index
    uint32_t offset = (uint32_t)addr - (uint32_t)buddy_system->memory_start;
    uint32_t page_idx = offset / PAGE_SIZE;
    
    //printf("Freeing memory at 0x%x (page index %u)\n", (uint32_t)addr, page_idx);
    uart_send_string("Freeing memory at 0x");
    uart_send_hex((uint32_t)addr);
    uart_send_string(" (page index ");
    uart_send_int(page_idx);
    uart_send_string(")\r\n");
    
    // Find the order of the block
    int order = 0;
    int block_idx = page_idx;
    
    // Traverse up until we find the allocated block
    while (order < MAX_ORDER) {
        block_idx = page_idx / (1 << order);
        if (buddy_system->buddy_list[order][block_idx].val == BLOCK_ALLOCATED) {
            break;
        }
        order++;
    }
    
    if (order >= MAX_ORDER) {
        //printf("Block not found for address 0x%x\n", (uint32_t)addr);
        uart_send_string("Block not found for address 0x");
        uart_send_hex((uint32_t)addr);
        uart_send_string("\r\n");
        return;
    }
    //printf("Found allocated block: order=%d, index=%d\n", order, block_idx);
    uart_send_string("Found allocated block: order=");
    uart_send_int(order);
    uart_send_string(", index=");
    uart_send_int(block_idx);
    uart_send_string("\r\n");
    
    // Mark the block as free
    buddy_system->buddy_list[order][block_idx].val = order;
    
    // Add to the free list
    if (buddy_system->first_avail[order] == -1) {
        buddy_system->first_avail[order] = block_idx;
        buddy_system->buddy_list[order][block_idx].next = NULL;
    } else {
        buddy_system->buddy_list[order][block_idx].next = &buddy_system->buddy_list[order][buddy_system->first_avail[order]];
        buddy_system->first_avail[order] = block_idx;
    }
    
    // Try to merge with the buddy
    while (order < MAX_ORDER - 1) {
        // Calculate buddy index
        int buddy_idx = block_idx ^ 1;
        
        // Check if the buddy is free
        if (buddy_system->buddy_list[order][buddy_idx].val != order) {
            // Buddy is not free, can't merge
            break;
        }
        //printf("Merging with buddy: order=%d, index=%d with buddy index=%d\n", order, block_idx, buddy_idx);
        uart_send_string("Merging with buddy: order=");
        uart_send_int(order);
        uart_send_string(", index=");
        uart_send_int(block_idx);
        uart_send_string(" with buddy index=");
        uart_send_int(buddy_idx);
        uart_send_string("\r\n");
         
        // Remove both blocks from the free list
        if (buddy_system->first_avail[order] == block_idx) {
            buddy_system->first_avail[order] = (buddy_system->buddy_list[order][block_idx].next != NULL) ? 
                (buddy_system->buddy_list[order][block_idx].next - buddy_system->buddy_list[order]) : -1;
                
                // uart_send_hex((buddy_system->buddy_list[order][block_idx].next));
                // uart_send_string("\r\n");
                // uart_send_hex(buddy_system->buddy_list[order]);
                // uart_send_string("\r\n");
        } else {
            // Find block in the list
            buddy_block_list_t* prev = &buddy_system->buddy_list[order][buddy_system->first_avail[order]];
            while (prev->next != &buddy_system->buddy_list[order][block_idx]) {
                prev = prev->next;
            }
            prev->next = buddy_system->buddy_list[order][block_idx].next;
        }

        // uart_send_string("Now first_avail[order]= ");
        // uart_send_int(buddy_system->first_avail[order]);
        // uart_send_string("\r\n");
        if (buddy_system->first_avail[order] == buddy_idx) {
            buddy_system->first_avail[order] = (buddy_system->buddy_list[order][buddy_idx].next != NULL) ? 
                (buddy_system->buddy_list[order][buddy_idx].next - buddy_system->buddy_list[order]) : -1;
        } else {
            // Find buddy in the list
            buddy_block_list_t* prev = &buddy_system->buddy_list[order][buddy_system->first_avail[order]];
            while (prev->next != &buddy_system->buddy_list[order][buddy_idx]) {
                prev = prev->next;
            }
            prev->next = buddy_system->buddy_list[order][buddy_idx].next;
        }
        //uart_send_string("Removed child buddy from free list\r\n");
        // Mark both blocks as belonging to a larger block
        buddy_system->buddy_list[order][block_idx].val = BLOCK_BELONGS;
        buddy_system->buddy_list[order][block_idx].next = NULL;
        buddy_system->buddy_list[order][buddy_idx].val = BLOCK_BELONGS;
        buddy_system->buddy_list[order][buddy_idx].next = NULL;
        
        // Move to the parent block
        order++;
        block_idx /= 2;
        
        // Mark the parent as free
        buddy_system->buddy_list[order][block_idx].val = order;
        
        // Add parent to the free list
        if (buddy_system->first_avail[order] == -1) {
            buddy_system->first_avail[order] = block_idx;
            buddy_system->buddy_list[order][block_idx].next = NULL;
        } else {
            buddy_system->buddy_list[order][block_idx].next = &buddy_system->buddy_list[order][buddy_system->first_avail[order]];
            buddy_system->first_avail[order] = block_idx;
        }
    }
    
    uart_send_string("Free operation completed\r\n");
}

void init_dynamic_allocator() {
    // Initialize all free lists to NULL
    for (int i = 0; i < NUM_POOLS; i++) {
        free_lists[i] = NULL;
        free_list_counts[i] = 0;
    }
    
    // Initialize the pool page tracking array
    for (int i = 0; i < (1 << (MAX_INDEX_EXPONENT - 1)); i++) {
        pool_page_addr[i] = -1;  // -1 indicates not part of any pool
    }
}

// dynamic_malloc function modified version
void* dynamic_malloc(size_t size) {
    // For large allocations, use buddy system directly
    if (size > POOL_SIZES[NUM_POOLS - 1]) {
        void* mem = buddy_malloc(size);
        uart_send_string("Large allocation: requested ");
        uart_send_int(size);
        uart_send_string(" bytes, use buddy_malloc\r\n");
        return mem;
    }
    
    // Find appropriate pool
    int pool_index = -1;
    for (int i = 0; i < NUM_POOLS; i++) {
        if (POOL_SIZES[i] >= size) {
            pool_index = i;
            break;
        }
    }
    
    // Check if there are available blocks
    if (free_list_counts[pool_index] == 0) {
        // No free blocks, allocate a new page
        void* new_page = buddy_malloc(PAGE_SIZE);
        
        // Calculate how many blocks can fit in one page
        int block_size = POOL_SIZES[pool_index];
        int blocks_per_page = PAGE_SIZE / block_size;
        
        // Update page tracking
        int page_index = ((int)(new_page - BUDDY_MEMORY_START)) / PAGE_SIZE;
        pool_page_addr[page_index] = pool_index;
        
        // Split the page into blocks and add to free list
        for (int i = 0; i < blocks_per_page; i++) {
            // Allocate space for block_header using simple_alloc
            block_header_t* block_header = (block_header_t*)simple_alloc(sizeof(block_header_t));
            
            // Set the address of the actual memory block
            block_header->address = (char*)new_page + i * block_size;
            
            // Add to free list
            block_header->next = free_lists[pool_index];
            free_lists[pool_index] = block_header;
            free_list_counts[pool_index]++;
        }
        
        uart_send_string("Created new pool: size ");
        uart_send_int(block_size);
        uart_send_string(", ");
        uart_send_int(blocks_per_page);
        uart_send_string(" blocks at page 0x");
        uart_send_hex((uint32_t)new_page);
        uart_send_string("\r\n");
    }
    
    // Now take a block from the free list
    block_header_t* allocated_header = free_lists[pool_index];
    free_lists[pool_index] = allocated_header->next;
    free_list_counts[pool_index]--;
    
    // Get the actual memory address to return
    void* allocated_memory = allocated_header->address;
    
    // The block_header could be saved somewhere for later use in free
    // Or simply create a new one during free operation
    
    uart_send_string("Small allocation: requested ");
    uart_send_int(size);
    uart_send_string(" bytes, allocated ");
    uart_send_int(POOL_SIZES[pool_index]);
    uart_send_string(" bytes at address 0x");
    uart_send_hex((uint32_t)allocated_memory);
    uart_send_string("\r\n");
    
    return allocated_memory;
}


void dynamic_free(void* ptr) {
    if (ptr == NULL) return;
    
    // Calculate which page this address belongs to
    uint32_t addr = (uint32_t)ptr;
    int page_index = (addr - BUDDY_MEMORY_START) / PAGE_SIZE;
    
    // Check if this address is from a pool or directly from buddy
    if (page_index >= 0 && page_index < (1 << (POOL_SIZES[NUM_POOLS-1] - 1)) && pool_page_addr[page_index] != -1) {
        // This is a small allocation from a pool
        int pool_index = pool_page_addr[page_index];
        
        // Create a new block_header for this address
        block_header_t* block = (block_header_t*)simple_alloc(sizeof(block_header_t));
        block->address = ptr;
        
        // Add it back to the free list
        block->next = free_lists[pool_index];
        free_lists[pool_index] = block;
        free_list_counts[pool_index]++;
        
        uart_send_string("Freed small block at 0x");
        uart_send_hex((uint32_t)ptr);
        uart_send_string(" to pool size ");
        uart_send_int(POOL_SIZES[pool_index]);
        uart_send_string("\r\n");
    } else {
        // This is a direct allocation from buddy system
        buddy_free(ptr);
        uart_send_string("Freed large block at 0x");
        uart_send_hex((uint32_t)ptr);
        uart_send_string(" back to buddy system\r\n");
    }
}

void memory_reserve(uint32_t start, uint32_t end) {
    // 計算起始和結束的區塊索引
    uint32_t start_idx = (start - (uint32_t)buddy_system->memory_start) / PAGE_SIZE;
    uint32_t end_idx = (end - (uint32_t)buddy_system->memory_start-1) / PAGE_SIZE;
    // 檢查是否為無效請求
    if (end_idx < start_idx) {
        uart_send_string("Invalid memory reserve request\r\n");
        return;
    }
    uart_send_string("Reserving memory from idx= ");
    uart_send_hex(start_idx);
    uart_send_string(" to idx= ");
    uart_send_hex(end_idx);
    uart_send_string("\r\n");

    // 從最底層頁面開始處理
    for (uint32_t i = start_idx; i <= end_idx; i++) {
        int target_order = 0;
        int target_idx = i;
        
        // 檢查是否已經被分配
        if (buddy_system->buddy_list[target_order][target_idx].val == BLOCK_ALLOCATED) {
            // 已經被分配，跳過
            continue;
        }
        
        // 從最底層開始，向上找到可以開始分割的最高層
        int split_order = MAX_ORDER - 1;
        int split_idx = target_idx / (1 << split_order);
        
        // 從最高層開始，往下檢查每一層
        while (split_order > 0) {
            // 檢查當前層級的區塊狀態
            int block_val = buddy_system->buddy_list[split_order][split_idx].val;
            
            if (block_val == BLOCK_SPLIT) {
                // 已經被分割，找下一層
                split_order--;
                split_idx = target_idx / (1 << split_order);
            } else {
                break;
            }
        }
        
        // 如果找到了可以分割的區塊
        if (split_order > 0) {
            // 從該層開始分割
            for (int j = split_order; j > target_order; j--) {
                // 計算當前區塊的索引
                int current_idx = target_idx / (1 << j);
                
                // 分割該區塊
                buddy_split(j, current_idx, target_order);
                
                // 更新索引
            }

        }
        uart_send_string("Memory reservation split completed\r\n");
        // 無論是否需要分割，最後都標記目標頁面為已分配
        if (buddy_system->buddy_list[target_order][target_idx].val == target_order) {
            mark_allocated(target_order, target_idx);
        }
    }
    uart_send_string("Memory reservation completed\r\n");
}

// Function to reserve all required memory regions
void reserve_system_memory(void) {
    
    // Now reserve all regions
    // Reserve spin tables
    memory_reserve((uint32_t)0x0000, (uint32_t)0x1000);
    // Reserve kernel image
    uint32_t kernel_end_addr = (uint32_t)&_kernel_end;

    uart_send_string("[reserve_memory] Kernel start: 0x");
    uart_send_hex(_kernel_start);
    uart_send_string(" - 0x");
    uart_send_hex(kernel_end_addr); 
    uart_send_string("\r\n");
    memory_reserve((uint32_t)_kernel_start, (uint32_t)kernel_end_addr);
    
    // Reserve initramfs
    uart_send_string("[reserve_memory] Initramfs start: 0x");
    uart_send_hex((uint32_t)g_initramfs_addr);
    uart_send_string(" - 0x");
    uart_send_hex((uint32_t)(g_initramfs_end_addr)); 
    uart_send_string("\r\n");
    memory_reserve((uint32_t)g_initramfs_addr, (uint32_t)g_initramfs_end_addr);
    if (g_fdt_addr) {
        struct fdt_header *header = (struct fdt_header *)g_fdt_addr;
        uint32_t dtb_size = (uint32_t)fdt32_to_cpu(header->totalsize);
        // Reserve devicetree
        memory_reserve((uint32_t)g_fdt_addr, (uint32_t)(g_fdt_addr + dtb_size));
        uart_send_string("[reserve_memory] Reserved devicetree: 0x");
        uart_send_hex((uint32_t)g_fdt_addr);
        uart_send_string(" - 0x");
        uart_send_hex((uint32_t)(g_fdt_addr + dtb_size));
        uart_send_string("\r\n");
    }
    
    
    // Reserve simple allocator - you'll need to define where it is located
    // This depends on your implementation
    // uint64_t startup_allocator_start = /* define this */;
    // uint64_t startup_allocator_size = /* define this */;
    // memory_reserve(startup_allocator_start, startup_allocator_start + startup_allocator_size);
    
    uart_send_string("[reserve_system_memory] Memory regions reserved successfully\r\n");
}