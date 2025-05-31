// vfs.h
#ifndef VFS_H
#define VFS_H

#include <stddef.h> // For size_t
#include <stdint.h> // For uint32_t etc. (optional, if needed for specific fs)

// Forward declarations to avoid circular dependencies
struct vnode;
struct file;
struct mount;
struct filesystem;

// 檔案操作集合 (對應 file_operations)
struct file_operations {
    // 寫入 file 指定的檔案
    // file: 檔案控制代碼 (file handle)
    // buf:  要寫入的資料緩衝區
    // len:  要寫入的長度
    // 返回: 實際寫入的位元組數，或錯誤碼 (負值)
    int (*write)(struct file* file, const void* buf, size_t len);

    // 從 file 指定的檔案讀取
    // file: 檔案控制代碼
    // buf:  存放讀取資料的緩衝區
    // len:  想要讀取的長度
    // 返回: 實際讀取的位元組數，或錯誤碼 (負值)
    int (*read)(struct file* file, void* buf, size_t len);

    // 開啟 vnode 代表的檔案，並將建立的 file handle 存入 target
    // file_node: 要開啟的 vnode
    // target:   (輸出參數) 指向新建立的 file handle 的指標
    // 返回: 0 表示成功，錯誤碼 (負值) 表示失敗
    int (*open)(struct vnode* file_node, struct file** target); //

    // 關閉 file handle
    // file: 要關閉的檔案控制代碼
    // 返回: 0 表示成功，錯誤碼 (負值) 表示失敗
    int (*close)(struct file* file); //

    // (選做，Basic 1 不強制，但 Advanced Exercise 2Framebuffer 會用到)
    // long (*lseek64)(struct file* file, long offset, int whence);
};

// Vnode 操作集合 (對應 vnode_operations)
struct vnode_operations {
    // 在 dir_node 目錄下尋找名為 component_name 的節點
    // dir_node:  當前目錄的 vnode
    // target:    (輸出參數) 指向找到的目標 vnode 的指標
    // component_name: 要尋找的下一級名稱 (例如 "my_file" 或 "my_dir")
    // 返回: 0 表示成功找到，錯誤碼 (負值) 表示失敗
    int (*lookup)(struct vnode* dir_node, struct vnode** target, const char* component_name); //

    // 在 dir_node 目錄下建立名為 component_name 的新檔案節點
    // dir_node:  父目錄的 vnode
    // target:    (輸出參數) 指向新建立的 vnode 的指標
    // component_name: 要建立的檔案名稱
    // 返回: 0 表示成功，錯誤碼 (負值) 表示失敗
    int (*create)(struct vnode* dir_node, struct vnode** target, const char* component_name); //

    // 在 dir_node 目錄下建立名為 component_name 的新目錄節點
    // dir_node:  父目錄的 vnode
    // target:    (輸出參數) 指向新建立的 vnode 的指標
    // component_name: 要建立的目錄名稱
    // 返回: 0 表示成功，錯誤碼 (負值) 表示失敗
    int (*mkdir)(struct vnode* dir_node, struct vnode** target, const char* component_name); //
    int (*lookup_parent)(struct vnode* dir_node, struct vnode** target_parent, struct vnode* task_root_node);
};
enum VNODE_TYPE {
    VNODE_DIR,  // 或者你之前用的 VNODE_DIR
    VNODE_FILE // 或者你之前用的 VNODE_FILE
};
// VFS 中的節點 (vnode)
struct vnode {
    struct mount* mount;                 // 指向這個 vnode 所屬的掛載點資訊 (如果它是掛載點的根)
    struct vnode_operations* v_ops;      // 指向 vnode 操作的函式集合
    struct file_operations* f_ops;       // 指向 file 操作的函式集合
    void* internal;                      // 指向底層檔案系統特定的內部資料結構
    // 你可以添加其他 VFS 通用的屬性，例如引用計數 (reference count)、節點類型 (v_type: V_FILE, V_DIR) 等
    int ref_count;
    enum VNODE_TYPE type; // 簡單的類型標示
};

// 檔案控制代碼 (file handle)
struct file {
    struct vnode* vnode;      // 指向此 file handle 對應的 vnode
    size_t f_pos;             // 目前在此檔案中的讀寫位置 (offset)
    struct file_operations* f_ops; // 指向此檔案的操作函式 (通常和 vnode->f_ops 一樣)
    int flags;                // 開檔時的旗標 (例如 O_CREAT, O_RDONLY)
};

// 掛載點 (mount point) 資訊
struct mount {
    struct vnode* mount_point_vnode; // 指向「掛載點目錄」的 vnode (例如父檔案系統中的 /initramfs)
    struct vnode* root;              // 指向「被掛載檔案系統」自己的根 vnode (例如新的 tmpfs 實例的 /)
    struct filesystem* fs;           // 指向檔案系統類型資訊
};

// 檔案系統類型資訊
struct filesystem {
    const char* name; // 檔案系統的名稱，例如 "tmpfs"
    // 設定掛載函式：當掛載此類檔案系統時被呼叫
    // fs:     指向這個 filesystem 結構本身
    // mount:  (輸出參數) 指向要被初始化的 mount 結構
    // 返回: 0 表示成功，錯誤碼 (負值) 表示失敗
    int (*setup_mount)(struct filesystem* fs, struct mount* mount, struct vnode* logical_parent_of_mount_point); //
    // 你可以在這裡加入檔案系統註冊到 VFS 時需要的其他函式指標，例如 unmount
};

// 全域的根檔案系統掛載點
extern struct mount* rootfs; // (概念對應，但這裡用 extern)
void kernel_init_vfs(void);
struct filesystem* find_filesystem(const char* name);
// VFS API 函數原型宣告
int register_filesystem(struct filesystem* fs); //
int vfs_open(const char* pathname, int flags, struct file** target); //
int vfs_close(struct file* file);
int vfs_write(struct file* file, const void* buf, size_t len); //
int vfs_read(struct file* file, void* buf, size_t len); //
int vfs_mkdir(const char* pathname); //
int vfs_mount(const char* target_path, const char* fs_name); // 類似於 vfs_mount(const char* target, const char* filesystem)
int vfs_lookup(const char* pathname, struct vnode** target); //
int vfs_resolve_path(const char* pathname, struct vnode* base_node, struct vnode* root_node, struct vnode** target);
// 輔助：錯誤碼 (可以定義更多)
#define E_OK      0  // 成功
#define E_PERM   -1  // Operation not permitted
#define E_NOENT  -2  // No such file or directory
#define E_BADF   -9  // Bad file descriptor
#define E_EXIST  -17 // File exists
#define E_INVAL  -22 // Invalid argument
#define E_NOMEM  -12 // Out of memory
#define E_NOTDIR -20 // Not a directory
#define E_ISDIR  -21 // Is a directory
#define E_MAX_FILES -24 // Too many open files in system (或 per-process)
#define E_FBIG   -27 // File too large
#define E_NOSPC  -28 // No space left on device
#define E_ROFS   -30 // Read-only file system
//basic 2
#define E_BUSY   -16 // Device or resource busy

#define O_CREAT     00000100 // 八進制，用於建立檔案
#define O_DIRECTORY 00200000 // 八進制，確保開啟的是目錄 (Linux 特有)
#define O_RDWR  02   // Open for reading and writing

#define MAX_REGISTERED_FS 8
#define MAX_MOUNTED_FS 8 // 假設最多可以掛載8個檔案系統

//Lab7 basic:3
#define MAX_PROCESS_OPEN_FILES 16
extern struct mount* mounted_fs_list[MAX_MOUNTED_FS];
extern int num_mounted_fs;
extern int first_mount_fs;
// 簡化版：全域只有一個檔案描述符表 (真正的系統中每個 process 有自己的)
#define MAX_OPEN_FILES_PER_PROCESS 16 // (對應 fd < 16)
#define MAX_PATHNAME_LEN 255
#endif // VFS_H