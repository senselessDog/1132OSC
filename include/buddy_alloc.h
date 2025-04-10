#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Constants
#define PAGE_SIZE 4096  // 4KB page size
#define MAX_ORDER 10    // Maximum order for the buddy system (2^10 * 4KB = 4MB max allocation)
#define BUDDY_METADATA_ADDR 0x10000000  // Start address for metadata
#define BUDDY_MEMORY_START 0x11000000   // Start address for allocatable memory
#define BUDDY_MEMORY_END 0x20000000     // End address for allocatable memory

// Status values for blocks
#define BLOCK_SPLIT -3      // Block is split into smaller blocks
#define BLOCK_BELONGS -2    // Block belongs to a larger block
#define BLOCK_ALLOCATED -1  // Block is allocated

// Data structures
typedef struct buddy_block_list {
    int val;  // Status or order value
    struct buddy_block_list* next;  // Next block in the free list
} buddy_block_list_t;

typedef struct {
    buddy_block_list_t** buddy_list;  // Array of free lists for each order
    int first_avail[MAX_ORDER];       // First available block index for each order
    void* list_addr[MAX_ORDER];       // Address of each order's list
    void* memory_start;               // Start of allocatable memory
    uint32_t total_pages;             // Total number of pages managed
} buddy_system_t;

// Global buddy system instance
buddy_system_t* buddy_system = NULL;

// Function prototypes
int size_to_order(size_t size);
void buddy_init(void);
void* buddy_malloc(size_t size);
void buddy_free(void* addr);
void buddy_split(int order, int start_idx, int requested_order);
void mark_allocated(int order, int start_idx);
//int find_block_order(void* addr);
//void print_buddy_status(void);