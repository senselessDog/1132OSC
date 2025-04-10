#include "buddy_alloc.h"
#include "uart.h"
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
    buddy_system = (buddy_system_t*)BUDDY_METADATA_ADDR;
    
    // Initialize buddy system fields
    buddy_system->memory_start = (void*)BUDDY_MEMORY_START;
    buddy_system->total_pages = total_pages;
    
    // Calculate starting point for buddy lists
    buddy_block_list_t* lists_start = (buddy_block_list_t*)(BUDDY_METADATA_ADDR + sizeof(buddy_system_t));
    buddy_system->buddy_list = (buddy_block_list_t**)((uint32_t)lists_start + sizeof(buddy_block_list_t*) * MAX_ORDER);
    uint32_t offset = 0;
    // Initialize lists for each order
    for (int order = 0; order < MAX_ORDER; order++) {
        buddy_system->list_addr[order] = (void*)((uint32_t)buddy_system->buddy_list + sizeof(buddy_block_list_t*) * MAX_ORDER + offset);
        buddy_system->buddy_list[order] = buddy_system->list_addr[order];
        
        // Initialize blocks for this order
        int num_blocks = total_pages / (1 << order);
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
    }
    
    uart_send_string("\r\nBuddy system initialized with ");
    uart_send_int(total_pages);
    uart_send_string(" pages\r\n");
    
    uart_send_string("\r\nMemory range: 0x");
    uart_send_hex(BUDDY_MEMORY_START);
    uart_send_string(" - 0x");
    uart_send_hex(BUDDY_MEMORY_END);
    uart_send_string("\r\n");
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
    
    // Continue splitting if needed
    if (child_order > requested_order) {
        buddy_split(child_order, left_child_idx, requested_order);
    }
}

// Mark a block as allocated and remove it from the free list
void mark_allocated(int order, int start_idx) {
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
    if (buddy_system == NULL) {
        buddy_init();
    }
    
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
        buddy_split(order, block_idx, requested_order);
        
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

// Find the order of a block given its address
int find_block_order(void* addr) {
    uint32_t offset = (uint32_t)addr - (uint32_t)buddy_system->memory_start;
    uint32_t page_idx = offset / PAGE_SIZE;
    
    // Start from the smallest order
    for (int order = 0; order < MAX_ORDER; order++) {
        int block_idx = page_idx / (1 << order);
        if (buddy_system->buddy_list[order][block_idx].val == BLOCK_ALLOCATED) {
            return order;
        }
    }
    
    return -1;  // Block not found
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

// Print the status of the buddy system
// void print_buddy_status(void) {
//     uart_send_string("\n--- Buddy System Status ---\n");
    
//     for (int order = 0; order < MAX_ORDER; order++) {
//         int free_blocks = 0;
//         int total_blocks = buddy_system->total_pages / (1 << order);
        
//         printf("Order %d (size %d KB):\n", order, (1 << order) * 4);
        
//         if (buddy_system->first_avail[order] != -1) {
//             printf("  Free blocks: ");
//             buddy_block_list_t* block = &buddy_system->buddy_list[order][buddy_system->first_avail[order]];
//             while (block != NULL) {
//                 int idx = (block - buddy_system->buddy_list[order]);
//                 printf("%d ", idx);
//                 free_blocks++;
//                 block = block->next;
//             }
//             printf("\n");
//         }
        
//         printf("  Total blocks: %d, Free: %d, Used/Split: %d\n", 
//                total_blocks, free_blocks, total_blocks - free_blocks);
//     }
    
//     printf("-------------------------\n\n");
// }
