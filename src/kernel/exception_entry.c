
#include <stdint.h>
#include "uart.h"
#include "gpu_interrupt.h"
#include "task_queue.h"
#include "syscall.h"
#include "thread.h"
#include "alloc.h"
// #define MMIO_BASE 0x3F000000
#define IRQ_PENDING1 ((volatile uint32_t *)(MMIO_BASE + 0x0000B204))
void display_timer_info(void *arg);
#define CORE0_INTERRUPT_SOURCE ((volatile uint32_t *)(0xFFFF000040000060))
// 定時器顯示數據結構
typedef struct
{
    uint64_t count;
    uint64_t freq;
} timer_display_data_t;

void empty()
{
}
uint32_t is_core_timer_irq()
{
    return *CORE0_INTERRUPT_SOURCE == (1 << 1);
}
uint32_t is_uart_interrupt()
{
    return *IRQ_PENDING1 & (1 << 29);
}
void sync_lower_el_64_entry(uint64_t parent_sp)
{
    
    uint64_t esr, far;
    asm volatile("mrs %0, esr_el1" : "=r"(esr));     // 讀取 ESR_EL1
    asm volatile("mrs %0, far_el1": "=r"(far));
    unsigned int ec = (esr >> 26) & 0x3f;           // 取得 Exception Class
    // uart_send_string("\r\nESR_EL1: 0x");
    // uart_send_hex(esr);
    // uart_send_string("\r\n");
    // uart_send_string("Fault Address Register (FAR_EL1): 0x");
    // uart_send_hex(far);
    // uart_send_string("\r\n------------------------\r\n");
    if (ec == 0b010101) { // 判斷是不是 SVC
        handle_syscall(parent_sp);
        // uint64_t tmp;
        // asm volatile("mrs %0,cntkctl_el1" :  "=r"(tmp));
        // uart_send_string("[sync_lower_el_64_entry] cntkctl_el1: ");
        // uart_send_hex(tmp);
        // uart_send_string("\r\n");
        // asm volatile("mrs %0, cntp_ctl_el0" :  "=r"(tmp));
        // uart_send_string("[sync_lower_el_64_entry] cntp_ctl_el0: ");
        // uart_send_hex(tmp);
        // uart_send_string("\r\n");
        // uart_send_string("[sync_lower_el_64_entry] cntp_tval_el0: ");
        // asm volatile("mrs %0, cntp_tval_el0" :  "=r"(tmp));
        // uart_send_hex(tmp);
        // uart_send_string("\r\n");
        // uart_send_string("[sync_lower_el_64_entry] frame address: ");
        // trap_frame_t *frame = (trap_frame_t *)parent_sp;
        // uart_send_hex((uint64_t)frame);
        // uart_send_string(",result: ");
        // uart_send_int(frame->x0);
        // uart_send_string("\r\n");
    }else if(ec == 0b100100 || ec==0b100000){ //Data Abort
        uart_send_string("Error: [sync_lower_el_64_entry] Data or Instruction Abort from a lower Exception level\r\n");
        uint64_t dfcs = esr & 0x3c; //bit[5~2]
        if (dfcs ==0b000100 ){
            trap_frame_t* frame=(trap_frame_t*)parent_sp;
            handle_page_fault(frame);
            // unsigned int err_level = esr & 0x3; //bit[1~0]
            // uart_send_string("Error: [sync_lower_el_64_entry] Translation fault happened at level ");
            // uart_send_int(err_level);
            // uart_send_string("\r\n");
            // uart_send_string("Error: [sync_lower_el_64_entry] Fault Address Register (FAR_EL1): 0x");
            // uart_send_hex(far);
            // uart_send_string("\r\n------------\r\n");
        }else if (dfcs ==0b001100){
            unsigned int err_level = esr & 0x3; //bit[1~0]
            uart_send_string("Error: [sync_lower_el_64_entry] Permission fault happened at level ");
            uart_send_int(err_level);
            uart_send_string("\r\n");
            uart_send_string("Error: [sync_lower_el_64_entry] Fault Address Register (FAR_EL1): 0x");
            uart_send_hex(far);
            uart_send_string("\r\n------------\r\n");
        }else{
            uart_send_string("Error: [sync_lower_el_64_entry] UnDevelop Error\r\n");
        }
            
    }
    // // Read exception-related registers
    // unsigned long spsr, elr;

    // asm volatile("mrs %0, spsr_el1" : "=r"(spsr));
    // asm volatile("mrs %0, elr_el1" : "=r"(elr));
    //asm volatile("mrs %0, esr_el1" : "=r"(esr));

    // Print the register values
    // uart_send_string("Exception taken!\r\n");
    // uart_send_string("SPSR_EL1: 0x");
    // uart_send_hex(spsr);
    // uart_send_string("\r\nELR_EL1: 0x");
    // uart_send_hex(elr);
    // uart_send_string("\r\nESR_EL1: 0x");
    // uart_send_hex(esr);
    // uart_send_string("\r\n");
    // int result;
    // asm volatile("mov %0, x0" :"=r"(result));
    

    return;
}

void el1_irq_entry(uint64_t frame_sp)
{
    // disable_interrupts();
    // uart_send_string("EL1 IRQ taken!\r\n");
    //empty();
    if (is_core_timer_irq())
    {
        *CORE0_INTERRUPT_SOURCE &= ~(1 << 1);
        trap_frame_t *frame = (trap_frame_t *)frame_sp;
        user_timeout_handler(frame);
        // // timer_interrupt_handler(); //先暫時關掉
        // uint32_t irq_id;
        // // asm volatile("mrc p15, 0, %0, c12, c12, 0" : "=r"(irq_id)); // 讀 GICC_IAR
        // // 處理計時器中斷
        
        // *CORE0_INTERRUPT_SOURCE &= ~(1 << 1);
        // // *CORE0_TIMER_IRQ_CTRL |= (1 << 1);  // 寫1清除中斷
        // // uart_send_string("[el1_irq_entry] \r\n");
        // user_thread_schedule();
        // // 創建定時器顯示任務數據
        // timer_display_data_t *data = (timer_display_data_t *)simple_alloc(sizeof(timer_display_data_t));


        // // 獲取當前計數和頻率
        // asm volatile("mrs %0, cntpct_el0" : "=r"(data->count));
        // asm volatile("mrs %0, cntfrq_el0" : "=r"(data->freq));
        // // 將任務加入佇列（優先級1）
        // // enqueue_task(&global_task_queue, display_timer_info, data, 1);

        // // 設置下一個計時器
        // unsigned long next_timeout = data->freq>>5;
        // asm volatile("msr cntp_tval_el0, %0" ::"r"(next_timeout));
        // asm volatile("mov x0, #1");
        // asm volatile("msr cntp_ctl_el0, x0");
        // // asm volatile("mcr p15, 0, %0, c12, c12, 1" :: "r"(irq_id)); // 寫 GICC_EOIR
    }
    if (is_gpu_interrupt())
    {
        // 檢查是否為 UART 中斷
        if (is_uart_interrupt())
        {
            uart_irq_handler();
        }
        // 處理其他可能的 GPU 中斷...
    }
    // enable_interrupts();
}
// 顯示定時器信息的任務處理函數 (for EL0)
void display_timer_info(void *arg)
{
    timer_display_data_t *data = (timer_display_data_t *)arg;

    // 計算並顯示啟動後的秒數
    int seconds = data->count / data->freq;
    uart_send_int(seconds);
    uart_send_string(" seconds\r\n");
}
void lower_el_irq_entry(uint64_t frame_sp)
{
    
    // uart_send_string("[lower_el_irq_entry] \r\n");
    if (is_core_timer_irq()) // 檢查計時器中斷位
    {
        *CORE0_INTERRUPT_SOURCE &= ~(1 << 1);
        trap_frame_t *frame = (trap_frame_t *)frame_sp;
        user_timeout_handler(frame);
        // user_thread_schedule(frame);
        // // 創建定時器顯示任務數據
        // timer_display_data_t *data = (timer_display_data_t *)simple_alloc(sizeof(timer_display_data_t));


        // // 獲取當前計數和頻率
        // asm volatile("mrs %0, cntpct_el0" : "=r"(data->count));
        // asm volatile("mrs %0, cntfrq_el0" : "=r"(data->freq));
        // // 將任務加入佇列（優先級1）
        // enqueue_task(&global_task_queue, display_timer_info, data, 1);

        // // 設置下一個計時器
        // unsigned long next_timeout = data->freq>>5;
        // asm volatile("msr cntp_tval_el0, %0" ::"r"(next_timeout));
        // asm volatile("mov x0, #1");
        // asm volatile("msr cntp_ctl_el0, x0");
    }
}

void print_esr_el1_details(uint64_t esr_el1) {
    uint32_t ec = (esr_el1 >> 26) & 0x3F; // Bits 31:26
    uint32_t il = (esr_el1 >> 25) & 0x1;  // Bit 25
    uint32_t iss = esr_el1 & 0x1FFFFFF;  // Bits 24:0

    uart_send_string("ESR_EL1 Details:\r\n");
    uart_send_string("  Raw ESR_EL1: 0x"); uart_send_hex(esr_el1); uart_send_string("\r\n");
    uart_send_string("  Exception Class (EC): 0x"); uart_send_hex(ec); uart_send_string(" (");

    switch (ec) {
        case 0b000000: uart_send_string("Unknown reason"); break;
        case 0b000001: uart_send_string("Trapped WFI or WFE"); break;
        // ... many other EC values for MCR/MRC, SIMD, etc.
        case 0b010101: uart_send_string("SVC instruction execution in AArch64 state"); break;
        case 0b100000: uart_send_string("Instruction Abort from lower EL (AArch32)"); break;
        case 0b100001: uart_send_string("Instruction Abort from lower EL (AArch64)"); break;
        case 0b100010: uart_send_string("PC alignment fault"); break;
        case 0b100100: uart_send_string("Data Abort from current EL (EL1)"); break;
        case 0b100101: uart_send_string("Data Abort from lower EL (EL0)"); break;
        case 0b100110: uart_send_string("SP alignment fault"); break;
        // ... other EC values
        case 0b110000: uart_send_string("Breakpoint exception from lower EL (AArch32)"); break;
        case 0b110001: uart_send_string("Breakpoint exception from lower EL (AArch64)"); break;
        case 0b110010: uart_send_string("Software Step from lower EL (AArch32)"); break;
        case 0b110011: uart_send_string("Software Step from lower EL (AArch64)"); break;
        case 0b110100: uart_send_string("Watchpoint from lower EL (AArch32)"); break;
        case 0b110101: uart_send_string("Watchpoint from lower EL (AArch64)"); break;
        case 0b111000: uart_send_string("BKPT instruction execution (AArch32)"); break;
        case 0b111100: uart_send_string("BRK instruction execution (AArch64)"); break;
        default: uart_send_string("Unlisted EC"); break;
    }
    uart_send_string(")\r\n");
    // uart_send_string("  Instruction Length (IL): "); uart_send_hex(il); uart_send_string(il ? " (32-bit)\r\n" : " (16-bit or N/A)\r\n");
    // uart_send_string("  Instruction Specific Syndrome (ISS): 0x"); uart_send_hex(iss); uart_send_string("\r\n");

    // // If it's a Data Abort (from current or lower EL)
    // if (ec == 0b100100 || ec == 0b100101) {
    //     uint32_t dfsc = iss & 0x3F;      // Bits 5:0 - Data Fault Status Code
    //     uint32_t wnr  = (iss >> 6) & 0x1; // Bit 6 - Write not Read

    //     uart_send_string("    Data Abort Details:\r\n");
    //     uart_send_string("      Write (1) / Read (0) operation: "); uart_send_hex(wnr); uart_send_string("\r\n");
    //     uart_send_string("      Data Fault Status Code (DFSC): 0x"); uart_send_hex(dfsc); uart_send_string(" (");
    //     switch (dfsc) {
    //         case 0b000000: uart_send_string("Address size fault, level 0"); break;
    //         case 0b000001: uart_send_string("Address size fault, level 1"); break;
    //         case 0b000010: uart_send_string("Address size fault, level 2"); break;
    //         case 0b000011: uart_send_string("Address size fault, level 3"); break;
    //         case 0b000100: uart_send_string("Translation fault, level 0"); break;
    //         case 0b000101: uart_send_string("Translation fault, level 1"); break;
    //         case 0b000110: uart_send_string("Translation fault, level 2"); break;
    //         case 0b000111: uart_send_string("Translation fault, level 3"); break;
    //         case 0b001001: uart_send_string("Access flag fault, level 1"); break;
    //         case 0b001010: uart_send_string("Access flag fault, level 2"); break;
    //         case 0b001011: uart_send_string("Access flag fault, level 3"); break;
    //         case 0b001101: uart_send_string("Permission fault, level 1"); break;
    //         case 0b001110: uart_send_string("Permission fault, level 2"); break;
    //         case 0b001111: uart_send_string("Permission fault, level 3"); break;
    //         // Add more DFSC codes as needed from ARM ARM
    //         case 0b100001: uart_send_string("Alignment fault"); break;
    //         case 0b110001: uart_send_string("TLB conflict abort"); break;
    //         default: uart_send_string("Unlisted DFSC or other fault type"); break;
    //     }
    //     uart_send_string(")\r\n");
    //     // You can decode other ISS bits here if needed (e.g., S1PTW, CM, EA, FnV)
    // }
}

// 在你的 default_handler_entry 中呼叫:
void default_handler_entry(void){
    uint64_t esr, far;
    asm volatile("mrs %0, esr_el1": "=r"(esr));
    asm volatile("mrs %0, far_el1": "=r"(far));

    uart_send_string("\r\n--- EXCEPTION CAUGHT ---\r\n");
    print_esr_el1_details(esr); // 呼叫新的解析函數
    uart_send_string("\r\nFault Address Register (FAR_EL1): 0x");
    uart_send_hex(far);
    uart_send_string("\r\n------------------------\r\n");

    // Loop indefinitely to halt
    // while(1); // Or your preferred halt mechanism
}
