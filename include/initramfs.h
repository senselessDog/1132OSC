// === 在新的 initramfs.h (或類似檔案) 中 ===
#ifndef INITRAMFS_H
#define INITRAMFS_H

#include "vfs.h"

#define INITRAMFS_MAX_NAME_LEN 255 // 路徑組件名稱最大長度
#define MAX_CHILDREN_PER_DIR 64   // 假設一個目錄下最多子節點 (可調整)

// CPIO newc 格式的頭部 (110 bytes)
struct cpio_newc_header {
    char c_magic[6];
    char c_ino[8];
    char c_mode[8];
    char c_uid[8];
    char c_gid[8];
    char c_nlink[8];
    char c_mtime[8];
    char c_filesize[8];
    char c_devmajor[8];
    char c_devminor[8];
    char c_rdevmajor[8];
    char c_rdevminor[8];
    char c_namesize[8];
    char c_check[8];
};

// initramfs 內部節點結構
typedef struct initramfs_inode {
    char name[INITRAMFS_MAX_NAME_LEN + 1];
    enum VNODE_TYPE type; // VNODE_FILE 或 VNODE_DIR
    struct vnode *v_node; // 指回 VFS vnode

    // 文件特有
    const char *data;   // 指向 CPIO 檔案數據的指標
    size_t size;        // 檔案大小

    // 目錄特有
    struct vnode* children[MAX_CHILDREN_PER_DIR]; // 指向子 vnode 的陣列
    int num_children;

    // 如果此 inode 是 initramfs 的根，".." 應該指向的 VFS vnode (由 setup_mount 設定)
    struct vnode* dotdot_vnode;
    // initramfs 內部父節點 (可選，如果需要更複雜的 .. 解析)
    struct initramfs_inode* parent_internal;

} initramfs_inode_t;

// 函數原型宣告
void initramfs_init(void); // 註冊檔案系統
int initramfs_setup_mount(struct filesystem* fs, struct mount* mount, struct vnode* logical_parent_of_mount_point);
static struct vnode* initramfs_add_node_recursive(struct mount* mount_info, initramfs_inode_t* root_internal_of_initramfs, const char* fullpath, enum VNODE_TYPE type, const char* data_ptr, size_t file_size);
// VFS 操作函數集 (將在 .c 檔案中定義)
extern struct file_operations initramfs_file_ops;
extern struct vnode_operations initramfs_vnode_ops;
extern uint64_t g_initramfs_addr;
extern uint64_t g_initramfs_size;
#endif // INITRAMFS_H