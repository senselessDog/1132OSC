#include "mailbox.h"
#include "vfs.h"
#include "uart.h" // for debugging
#include "strcmp.h" // for memcpy
#include "tmpfs.h"
#include "fs/specialFile_vfs.h" // for framebuffer_vfs_init
// 全域變數儲存 framebuffer 的資訊
unsigned int fb_width = 0;
unsigned int fb_height = 0;
unsigned int fb_pitch = 0;
unsigned int fb_is_rgb = 0; // 1 for RGB, 0 for BGR
unsigned char *fb_ptr = 0;   // 指向 framebuffer 記憶體的指標 (ARM 物理地址轉換後的虛擬地址)
size_t fb_size = 0;
struct file_operations fb_dev_file_ops = {
    .write = fb_dev_write,
    .read = fb_dev_read,     // 僅寫
    .open =tmpfs_open,
    .close = tmpfs_close,   // framebuffer_dev_close
    .lseek64 = fb_dev_lseek64,
    .ioctl = fb_dev_ioctl,
    // .open, .close 可以是 NULL 或返回 E_OK 的簡單函數
};
// 與講義 P13 匹配的 framebuffer_info 結構
// 你可能需要在一個共用的頭檔案 (如 vfs.h 或一個新的 fb.h) 中定義它

void framebuffer_vfs_init(void){
    uart_send_string("[framebuffer_vfs_init]lookup /dev directory...\r\n");
    struct vnode* target_vnode = NULL;
    int dev_ret = vfs_lookup("/dev",&target_vnode,0); // 這裡的 vfs_mkdir 內部會使用 vfs_resolve_path
    if (dev_ret != E_OK && dev_ret != -E_EXIST) {
        uart_send_string("Failed to lookup /dev directory: "); uart_send_int(dev_ret); uart_send_string("\r\n");
    } else {
        // uart_send_string("/dev directory created or exists.\r\n");
        uart_send_string("[framebuffer_vfs_init]Attempting to mknod /dev/framebuffer...\r\n");
        int mknod_ret = vfs_mknod("/dev/framebuffer", VNODE_FILE, &fb_dev_file_ops, &tmpfs_vnode_ops); // 使用 uart_dev_vnode_ops
        if (mknod_ret != E_OK) {
            uart_send_string("Failed to mknod /dev/framebuffer: "); uart_send_int(mknod_ret); uart_send_string("\r\n");
        } else {
            uart_send_string("[framebuffer_vfs_init]/dev/framebuffer device node created successfully via vfs_mknod.\r\n");
        }
        
    }
}

// 初始化 framebuffer 的函數
int framebuffer_init(unsigned int req_width, unsigned int req_height, unsigned int req_depth) {
    volatile unsigned int __attribute__((aligned(16))) mbox[36];

    mbox[0] = 35 * 4;        // Buffer size in bytes
    mbox[1] = MBOX_REQUEST;  // Request message

    mbox[2] = 0x48003;       // Tag: Set physical width/height
    mbox[3] = 8;             // Value buffer size (bytes)
    mbox[4] = 8;             // Request/response codes
    mbox[5] = req_width;     // Value: width
    mbox[6] = req_height;    // Value: height

    mbox[7] = 0x48004;       // Tag: Set virtual width/height
    mbox[8] = 8;
    mbox[9] = 8;
    mbox[10] = req_width;    // Value: virtual width
    mbox[11] = req_height;   // Value: virtual height

    mbox[12] = 0x48009;      // Tag: Set virtual offset
    mbox[13] = 8;
    mbox[14] = 8;
    mbox[15] = 0;            // X offset
    mbox[16] = 0;            // Y offset

    mbox[17] = 0x48005;      // Tag: Set depth
    mbox[18] = 4;
    mbox[19] = 4;
    mbox[20] = req_depth;    // Bits per pixel

    mbox[21] = 0x48006;      // Tag: Set pixel order
    mbox[22] = 4;
    mbox[23] = 4;
    mbox[24] = 1;            // 1 for RGB, 0 for BGR (講義期望 RGB [cite: 213])

    mbox[25] = 0x40001;      // Tag: Allocate buffer (request display buffer)
    mbox[26] = 8;            // Value buffer size
    mbox[27] = 8;            // Req/resp code
    mbox[28] = 4096;         // Request: alignment for buffer (response: frame buffer address)
    mbox[29] = 0;            // Response: frame buffer size

    mbox[30] = 0x40008;      // Tag: Get pitch
    mbox[31] = 4;
    mbox[32] = 4;
    mbox[33] = 0;            // Response: pitch (bytes per line)

    mbox[34] = MBOX_TAG_LAST;

    if (mailbox_call_lowlevel(MBOX_CH_PROP, (unsigned int*)mbox) && mbox[20] == req_depth && mbox[28] != 0) {
        mbox[28] &= 0x3FFFFFFF; // Convert GPU address to ARM physical address [cite: 215]

        fb_width = mbox[5];    // Actual width [cite: 215]
        fb_height = mbox[6];   // Actual height [cite: 216]
        fb_pitch = mbox[33];   // Pitch [cite: 216]
        fb_is_rgb = mbox[24];  // Pixel order [cite: 217]
        fb_ptr = (unsigned char *)((uintptr_t)mbox[28]); // Framebuffer pointer [cite: 217]
                                                       // 這應該是物理地址，你需要將它轉換成核心可用的虛擬地址
                                                       // 如果你的核心是恆等映射 (identity mapping) 低地址記憶體，則可以直接用
                                                       // 否則你需要透過頁表映射它到核心虛擬地址空間
        fb_size = mbox[29];    // Framebuffer size

        uart_send_string("Framebuffer initialized:\r\n");
        uart_send_string("  Width: "); uart_send_int(fb_width);
        uart_send_string(", Height: "); uart_send_int(fb_height);
        uart_send_string(", Pitch: "); uart_send_int(fb_pitch);
        uart_send_string("\r\n  IsRGB: "); uart_send_int(fb_is_rgb);
        uart_send_string(", Depth: "); uart_send_int(mbox[20]); // depth
        uart_send_string("\r\n  Pointer (phys): 0x"); uart_send_hex(mbox[28]);
        uart_send_string(", Size: "); uart_send_int(fb_size); uart_send_string("\r\n");
        return 0; // Success
    } else {
        uart_send_string("Error:[framebuffer] Unable to set screen resolution (");
        uart_send_int(req_width); uart_send_string("x"); uart_send_int(req_height);
        uart_send_string("x"); uart_send_int(req_depth); uart_send_string(")\r\n");
        return -1; // Failure
    }
}

// Framebuffer file operations
static int fb_dev_write(struct file* f, const void* buf, size_t len) {
    if (!fb_ptr) return -E_NODEV; // Framebuffer not initialized
    if (!f || !buf) return -E_INVAL;

    // 確保寫入不越界
    if (f->f_pos >= fb_size) return 0; // 已經在 framebuffer 範圍之外
    size_t remaining_space = fb_size - f->f_pos;
    size_t bytes_to_write = (len < remaining_space) ? len : remaining_space;

    if (bytes_to_write > 0) {
        memcpy(PHYS_TO_KVA(fb_ptr) + f->f_pos, buf, bytes_to_write);
        f->f_pos += bytes_to_write;
    }
    return bytes_to_write;
}

static int fb_dev_read(struct file* f, void* buf, size_t len) {
    return -E_PERM; // Framebuffer 是僅寫的 [cite: 201]
}
# define SEEK_SET 0
static long fb_dev_lseek64(struct file* file, long offset, int whence) {
    if (!fb_ptr) return -E_NODEV;
    if (!file) return -E_INVAL;

    long new_pos;
    switch (whence) {
        case SEEK_SET:
            new_pos = offset;
            break;
        default:
            return -E_INVAL;
    }

    if (new_pos < 0 || new_pos > (long)fb_size) { // 允許 seek 到檔案末尾 (f_pos == size)
        return -E_INVAL; // 超出範圍
    }
    file->f_pos = (size_t)new_pos;
    return file->f_pos;
}

// ioctl 請求號定義 (如果還沒定義的話)
#define FB_IOCTL_GET_INFO 0 // 根據講義，ioctl 0 用於獲取資訊 [cite: 204]

static int fb_dev_ioctl(struct file* file, unsigned long request, void* argp) {
    if (!fb_ptr) return -E_NODEV;

    switch (request) {
        case FB_IOCTL_GET_INFO: {
            if (!argp) return -E_INVAL;
            struct framebuffer_info user_fb_info_template; // 用來接收使用者傳入的預設值

            // 從使用者空間複製 framebuffer_info 結構 (這裡需要 copy_from_user)
            // 為了簡化，我們假設 argp 是核心可直接訪問的指標，並已包含使用者提供的預設值
            // 實際上，使用者傳遞的是指向其空間中 struct framebuffer_info 的指標
            // memcpy(&user_fb_info_template, argp, sizeof(struct framebuffer_info)); // 假設的 copy_from_user
            // 講義提到 "there will be default value in info" [cite: 204]
            // "if it works with default value, you can ignore this syscall" [cite: 204]
            // 這句話有點模糊，一種理解是使用者會傳一個填有預設值的結構，核心用實際值覆蓋它。
            // 另一種理解是，如果 framebuffer 的配置剛好和使用者期望的預設值一樣，那 ioctl 可以不做事。
            // 我們按照前者來實現：核心用實際值填充使用者提供的結構。

            struct framebuffer_info kern_fb_info;
            kern_fb_info.width = fb_width;
            kern_fb_info.height = fb_height;
            kern_fb_info.pitch = fb_pitch;
            kern_fb_info.isrgb = fb_is_rgb; // 假設你的 framebuffer_info 也有 isrgb
            // kern_fb_info.depth = ... (從 mailbox 呼叫中獲取到的實際深度)

            // 將核心的資訊複製回使用者空間 (這裡需要 copy_to_user)
            // 簡化：直接寫入
            memcpy(argp, &kern_fb_info, sizeof(struct framebuffer_info));

            uart_send_string("[fb_dev_ioctl] FB_IOCTL_GET_INFO: W="); uart_send_int(kern_fb_info.width);
            uart_send_string(" H="); uart_send_int(kern_fb_info.height);
            uart_send_string(" P="); uart_send_int(kern_fb_info.pitch); uart_send_string("\r\n");
            return E_OK;
        }
        default:
            return -E_INVAL; // Unknown request
    }
}


// Framebuffer 的 vnode_operations 通常很簡單，因為它是一個檔案，不是目錄
// 可以共用 uart_dev_vnode_ops 或一個通用的 "device_file_vnode_ops"
// extern struct vnode_operations device_file_vnode_ops; // 假設有一個通用的
// 或者直接用 NULL (如果 VFS 層能處理) 或一個極簡實現：
// static int devfile_not_dir_op(struct vnode* dir, struct vnode** t, const char* n) { return -E_NOTDIR; }
// struct vnode_operations fb_dev_vnode_ops = {
//     .lookup = devfile_not_dir_op,
//     .create = devfile_not_dir_op,
//     .mkdir = devfile_not_dir_op,
//     /* .lookup_parent, .mknod 可能也不需要或返回錯誤 */
// };