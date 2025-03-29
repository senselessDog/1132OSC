
#include <stdint.h>
void uart_send_string(char *str);
void uart_send_hex(uint32_t value);

void sync_lower_el_64_entry(void) {
    // Read exception-related registers
    unsigned long spsr, elr, esr;
    
    asm volatile("mrs %0, spsr_el1" : "=r" (spsr));
    asm volatile("mrs %0, elr_el1" : "=r" (elr));
    asm volatile("mrs %0, esr_el1" : "=r" (esr));
    
    // Print the register values
    uart_send_string("Exception taken!\r\n");
    uart_send_string("SPSR_EL1: 0x");
    uart_send_hex(spsr);
    uart_send_string("\r\nELR_EL1: 0x");
    uart_send_hex(elr);
    uart_send_string("\r\nESR_EL1: 0x");
    uart_send_hex(esr);
    uart_send_string("\r\n");

    return;
}