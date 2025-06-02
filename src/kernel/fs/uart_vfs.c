// === 在 uart.c 或一個新的 uart_vfs.c 中 ===
#include "vfs.h"
#include "uart.h" // 假設這裡有 uart_send_char, uart_recv_char 或類似函式
#include "buddy_alloc.h"
#include "thread.h"
#include "tmpfs.h"
struct file_operations uart_dev_file_ops;
void mkdir_dev(void){
    int mkdir_dev_ret = vfs_mkdir("/dev"); // 這裡的 vfs_mkdir 內部會使用 vfs_resolve_path
    if (mkdir_dev_ret != E_OK && mkdir_dev_ret != -E_EXIST) {
        uart_send_string("Failed to create /dev directory: "); uart_send_int(mkdir_dev_ret); uart_send_string("\r\n");
    }
}
void uart_vfs_init(void){
    uart_send_string("[uart_vfs_init]lookup /dev directory...\r\n");
    struct vnode* target_vnode = NULL;
    int dev_ret = vfs_lookup("/dev",&target_vnode,0);
    if (dev_ret != E_OK && dev_ret != -E_EXIST) {
        uart_send_string("Failed to lookup /dev directory: "); uart_send_int(dev_ret); uart_send_string("\r\n");
    } else {
        // uart_send_string("/dev directory created or exists.\r\n");
        uart_send_string("[uart_vfs_init]Attempting to mknod /dev/uart...\r\n");
        int mknod_ret = vfs_mknod("/dev/uart", VNODE_FILE, &uart_dev_file_ops, &tmpfs_vnode_ops); // 使用 uart_dev_vnode_ops
        if (mknod_ret != E_OK) {
            uart_send_string("Failed to mknod /dev/uart: "); uart_send_int(mknod_ret); uart_send_string("\r\n");
        } else {
            uart_send_string("[uart_vfs_init]/dev/uart device node created successfully via vfs_mknod.\r\n");
            register_uart_device(); // 假設這個函數會註冊 UART 設備到 VFS
        }
        
    }
}
void register_uart_device(void) {
    // 這個函數可以用來註冊 UART 設備到 VFS
    // 例如，將 /dev/uart 設備節點與實際的 UART 硬體或驅動程式連接
    uart_send_string("UART device registered in VFS.\r\n");
    char* uart_dev_path = "/dev/uart";
    struct file* stdin_file, *stdout_file, *stderr_file;
    thread_t* init_task = get_current();
    uart_send_string("Setting up stdin/stdout/stderr for init task...\r\n");
    // Open for stdin (fd 0)
    if (vfs_open(uart_dev_path, O_RDONLY, &stdin_file) == E_OK) { // O_RDONLY 或 O_RDWR
        if (init_task->fd_table[0] != NULL) vfs_close(init_task->fd_table[0]); // 以防萬一
        init_task->fd_table[0] = stdin_file;
        uart_send_string("stdin (fd 0) set to /dev/uart\r\n");
    } else {
        uart_send_string("Error opening /dev/uart for stdin!\r\n");
    }

    // Open for stdout (fd 1)
    if (vfs_open(uart_dev_path, O_WRONLY, &stdout_file) == E_OK) { // O_WRONLY 或 O_RDWR
        if (init_task->fd_table[1] != NULL) vfs_close(init_task->fd_table[1]);
        init_task->fd_table[1] = stdout_file;
        uart_send_string("stdout (fd 1) set to /dev/uart\r\n");
    } else {
        uart_send_string("Error opening /dev/uart for stdout!\r\n");
    }

    // Open for stderr (fd 2)
    if (vfs_open(uart_dev_path, O_WRONLY, &stderr_file) == E_OK) { // O_WRONLY 或 O_RDWR
        if (init_task->fd_table[2] != NULL) vfs_close(init_task->fd_table[2]);
        init_task->fd_table[2] = stderr_file;
        uart_send_string("stderr (fd 2) set to /dev/uart\r\n");
    } else {
        uart_send_string("Error opening /dev/uart for stderr!\r\n");
    }
}
// void uart_vfs_init(void){
//     uart_send_string("[uart_vfs_init]Creating /dev directory...\r\n");
//     int mkdir_dev_ret = vfs_mkdir("/dev");
//     if (mkdir_dev_ret != E_OK && mkdir_dev_ret != -E_EXIST) {
//         uart_send_string("Failed to create /dev directory: "); uart_send_int(mkdir_dev_ret); uart_send_string("\r\n");
//         // 嚴重錯誤，可能需要 panic
//     } else {
//         uart_send_string("[uart_vfs_init]/dev directory created or exists.\r\n");

//         // 現在創建 /dev/uart 特殊 vnode
//         // 我們需要手動創建一個 vnode 並將其 f_ops 指向 uart_dev_file_ops
//         // 這需要一個方法將這個 vnode "插入" 到 /dev 目錄下
//         // 最簡單的方式是擴展 tmpfs 的功能，允許它創建一個 "預先設定好 f_ops" 的特殊檔案

//         // 方案 A: 修改 tmpfs，增加一個 create_special_file 操作
//         // tmpfs_vnode_ops.create_special_file(dev_dir_vnode, &uart_vnode, "uart", &uart_dev_file_ops, &uart_dev_vnode_ops);

//         // 方案 B: 如果不想修改 tmpfs，可以手動查找 /dev，然後創建一個 tmpfs 內部節點，
//         //         再創建 vnode，然後手動覆寫其 f_ops 和 v_ops。
//         //         這種方式比較 hacky，但對於實驗是可行的。

//         struct vnode* dev_dir_vnode = NULL;
//         int lookup_dev_ret = vfs_lookup("/dev", &dev_dir_vnode); // 使用你已有的 vfs_lookup

//         if (lookup_dev_ret == E_OK) {
//             uart_send_string("Found /dev vnode. Attempting to create uart device file...\r\n");

//             // 手動創建一個代表 /dev/uart 的 vnode 和 internal node
//             // 這部分需要調用 tmpfs 的內部創建邏輯，或者有一個通用的 vfs_mknod_internal 輔助函數

//             // 假設我們直接在 dev_dir_vnode (tmpfs) 上調用 create，然後修改它
//             struct vnode* uart_file_vnode = NULL;
//             // 先嘗試用普通的 create 創建一個空檔案節點
//             // 確保 dev_dir_vnode->v_ops->create 是存在的 (它應該是 tmpfs_create)
//             if (dev_dir_vnode->v_ops && dev_dir_vnode->v_ops->create) {
//                 int create_uart_ret = dev_dir_vnode->v_ops->create(dev_dir_vnode, &uart_file_vnode, "uart");
//                 if (create_uart_ret == E_OK) {
//                     uart_send_string("[uart_vfs_init]Placeholder for /dev/uart created via tmpfs->create.\r\n");
//                     // 現在修改這個 vnode，使其成為 UART 設備
//                     uart_file_vnode->f_ops = &uart_dev_file_ops;
//                     uart_file_vnode->v_ops = &uart_dev_vnode_ops; // UART 設備通常不需要 vnode 操作
//                     // uart_file_vnode->type 應該是 VNODE_FILE 或一個新的 VNODE_CHAR_DEVICE
//                     // tmpfs 創建的 internal node 可能不適用於設備，但對於簡化實驗可能夠用
//                     // ((tmpfs_inode_t*)uart_file_vnode->internal)->type = TMPFS_DEVICE; // 如果 tmpfs_inode 有此類型

//                     uart_send_string("[uart_vfs_init]/dev/uart special file node configured.\r\n");

//                     // 因為 create 會增加 uart_file_vnode 的引用計數，而我們不再直接持有它 (它在 /dev 中)
//                     // 所以這裡應該減少一次引用，除非你有其他地方會用到這個 uart_file_vnode 指標
//                     uart_file_vnode->ref_count--;
//                     char* uart_dev_path = "/dev/uart";
//                     struct file* stdin_file, *stdout_file, *stderr_file;

//                     uart_send_string("Setting up stdin/stdout/stderr for init task...\r\n");
//                     thread_t* init_task = get_current(); // 假設有一個函數可以獲取 init 任務
//                     // Open for stdin (fd 0)
//                     if (vfs_open(uart_dev_path, O_RDONLY, &stdin_file) == E_OK) { // O_RDONLY 或 O_RDWR
//                         if (init_task->fd_table[0] != NULL) vfs_close(init_task->fd_table[0]); // 以防萬一
//                         init_task->fd_table[0] = stdin_file;
//                         uart_send_string("stdin (fd 0) set to /dev/uart\r\n");
//                     } else {
//                         uart_send_string("Error opening /dev/uart for stdin!\r\n");
//                     }

//                     // Open for stdout (fd 1)
//                     if (vfs_open(uart_dev_path, O_WRONLY, &stdout_file) == E_OK) { // O_WRONLY 或 O_RDWR
//                         if (init_task->fd_table[1] != NULL) vfs_close(init_task->fd_table[1]);
//                         init_task->fd_table[1] = stdout_file;
//                         uart_send_string("stdout (fd 1) set to /dev/uart\r\n");
//                     } else {
//                         uart_send_string("Error opening /dev/uart for stdout!\r\n");
//                     }

//                     // Open for stderr (fd 2)
//                     if (vfs_open(uart_dev_path, O_WRONLY, &stderr_file) == E_OK) { // O_WRONLY 或 O_RDWR
//                         if (init_task->fd_table[2] != NULL) vfs_close(init_task->fd_table[2]);
//                         init_task->fd_table[2] = stderr_file;
//                         uart_send_string("stderr (fd 2) set to /dev/uart\r\n");
//                     } else {
//                         uart_send_string("Error opening /dev/uart for stderr!\r\n");
//                     }
//                 } else if (create_uart_ret == -E_EXIST) {
//                     uart_send_string("[uart_vfs_init]/dev/uart already exists. Attempting to reconfigure...\r\n");
//                 }
//                 else {
//                     uart_send_string("[uart_vfs_init]Failed to create placeholder for /dev/uart: "); uart_send_int(create_uart_ret); uart_send_string("\r\n");
//                 }
//             } else {
//                 uart_send_string("Error:[uart_vfs_init] /dev vnode does not support create operation.\r\n");
//             }
//             dev_dir_vnode->ref_count--; // 釋放 lookup /dev 得到的引用
//         } else {
//             uart_send_string("[uart_vfs_init] Failed to lookup /dev directory after creation: "); uart_send_int(lookup_dev_ret); uart_send_string("\r\n");
//         }
//     }
// }
// UART 設備檔案的 file_operations
static int uart_dev_read(struct file* f, void* buf, size_t len) {
    if (!buf || len == 0) return 0;
    char *cbuf = (char*)buf;
    size_t bytes_read = 0;
    for (size_t i = 0; i < len; ++i) {
        // 假設 uart_getc() 是阻塞讀取一個字元
        // 如果你的 uart_recv() 是一次讀取多個，需要調整
        cbuf[i] = uart_recv(); // 或者 uart_recv_char()
        bytes_read++;
        if (cbuf[i] == '\n' || cbuf[i] == '\r') { // 讀到換行可以提前結束 (可選)
            // 如果希望讀滿 len，則移除此判斷
            break;
        }
    }
    return bytes_read;
}

static int uart_dev_write(struct file* f, const void* buf, size_t len) {
    if (!buf) return 0;
    const char *cbuf = (const char*)buf;
    for (size_t i = 0; i < len; ++i) {
        uart_send(cbuf[i]); // 或者 uart_send_char(cbuf[i])
    }
    return len; // 返回實際寫入的長度
}

static int uart_dev_open(struct vnode* file_node, struct file** target) {
    // UART 設備通常不需要特別的 open 狀態
    return E_OK;
}

static int uart_dev_close(struct file* file) {
    // UART 設備通常不需要特別的 close 狀態
    return E_OK;
}

struct file_operations uart_dev_file_ops = {
    .read = uart_dev_read,
    .write = uart_dev_write,
    .open = uart_dev_open,
    .close = uart_dev_close,
    // .lseek64 = ... (UART 通常不支持 lseek)
};

// // UART 設備的 vnode_operations (大部分可能不需要或返回錯誤)
// // 因為它是一個檔案而不是目錄，所以 lookup, create, mkdir 通常不適用
// static int uart_dev_lookup(struct vnode* dir_node, struct vnode** target, const char* component_name) {
//     return -E_NOTDIR; // UART 設備檔案不是目錄
// }
// static int uart_dev_create(struct vnode* dir_node, struct vnode** target, const char* component_name) {
//     return -E_NOTDIR;
// }
//  static int uart_dev_mkdir(struct vnode* dir_node, struct vnode** target, const char* component_name) {
//     return -E_NOTDIR;
// }


// struct vnode_operations uart_dev_vnode_ops = {
//     .lookup = uart_dev_lookup, // 或設為 NULL 如果不應該被呼叫
//     .create = uart_dev_create, // 或設為 NULL
//     .mkdir = uart_dev_mkdir,   // 或設為 NULL
//     // .lookup_parent 可能也不需要，或返回錯誤
// };