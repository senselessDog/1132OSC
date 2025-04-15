#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

// Constants
#define PAGE_SIZE 4096  // 4KB page size
#define MAX_ORDER 13    //range from order=0~12
#define BUDDY_METADATA_ADDR 0x00 // Start address for metadata
#define BUDDY_MEMORY_START 0x00   // Start address for allocatable memory
#define BUDDY_MEMORY_END 0x3C000000     // End address for allocatable memory
// Status values for blocks
#define BLOCK_SPLIT -3      // Block is split into smaller blocks
#define BLOCK_BELONGS -2    // Block belongs to a larger block
#define BLOCK_ALLOCATED -1  // Block is allocated

uint32_t _kernel_start=0x80000;
// void * kernel_end=0x100000;
extern char _kernel_end;

// Data structures
typedef struct buddy_block_list {
    int val;  // Status or order value
    struct buddy_block_list* prev;  // Previous block in the free list
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
//dynamic memory allocation
void init_dynamic_allocator(void);
void* dynamic_malloc(size_t size);
void dynamic_free(void* ptr);
// Define the number of pools and their sizes
#define NUM_POOLS 8
const size_t POOL_SIZES[NUM_POOLS] = {16, 32, 64, 128,256, 512, 1024, 2048};  // Note: You had 196 but I think you meant 96

// For tracking free blocks in each pool
typedef struct block_header {
    void* address; // Pointer to the start of the block
    struct block_header* next;  // Pointer to next free block
} block_header_t;

// Array of free lists, one for each pool size
block_header_t* free_lists[NUM_POOLS];
#define MAX_INDEX_EXPONENT 19
// To track which pool a page belongs to (-1 if page is directly from buddy allocator)
int pool_page_addr[(1 << (MAX_INDEX_EXPONENT - 1))];

// Count of free blocks in each pool
int free_list_counts[NUM_POOLS];
//memory_reserve
void memory_reserve(uint32_t start, uint32_t end);
void reserve_system_memory(void);