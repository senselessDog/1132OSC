#include <stdint.h>
#include <string.h>
void uart_send_string(const char *str);
extern char __heap_start;
extern char __heap_end;

static char *heap = &__heap_start;
static size_t heap_size;
static size_t heap_index = 0;

void *simple_alloc(size_t size)
{
    if (size % 16 != 0)
    {
        size += 16 - (size % 16);
    }
    heap_size = &__heap_end - &__heap_start;
    if (heap_index + size > heap_size)
    {
        uart_send_string("Heap overflow\r\n");
        return NULL;
    }
    heap_index += size;
    void *ptr = heap + heap_index - size;
    return ptr;
}