// tmpfs.h
#ifndef TMPFS_H
#define TMPFS_H

#include "vfs.h"

#define TMPFS_MAX_NAME_LEN 15
#define TMPFS_MAX_DIR_ENTRIES 16 // [cite: 20]
#define TMPFS_MAX_FILE_SIZE 4096 // [cite: 21]
enum TMPFS_TYPE {
    TMPFS_DIR,  // 或者你之前用的 TMPFS_DIR
    TMPFS_FILE,      // 或者你之前用的 TMPFS_FILE
};
// tmpfs 內部節點結構 (會被 vnode->internal 指向)
typedef struct tmpfs_inode {
    char name[TMPFS_MAX_NAME_LEN + 1];
    enum TMPFS_TYPE type;
    
    // 如果是檔案
    char content[TMPFS_MAX_FILE_SIZE];
    size_t size;

    // 如果是目錄
    struct vnode* children[TMPFS_MAX_DIR_ENTRIES];
    int num_children;

    struct vnode* v_node; // 指回包含此 internal node 的 vnode (方便操作)
    // 可以加入指向父 tmpfs_inode 的指標，方便實作 ".."
    struct tmpfs_inode* parent_dir_internal; // 指向父目錄的 internal node
} tmpfs_inode_t;

// 初始化 tmpfs 檔案系統 (註冊到 VFS)
void tmpfs_init();

// tmpfs 的 setup_mount 函數 (符合 filesystem->setup_mount 的簽名)
// int tmpfs_setup_mount(struct filesystem* fs, struct mount* mount);
tmpfs_inode_t* create_tmpfs_internal_node(const char* name, enum TMPFS_TYPE type, struct vnode* v_node_ptr);
struct vnode* create_tmpfs_vnode(struct mount* mount_info, tmpfs_inode_t* internal_node, enum VNODE_TYPE v_type);
int tmpfs_write(struct file* file, const void* buf, size_t len);
int tmpfs_read(struct file* file, void* buf, size_t len);
int tmpfs_open(struct vnode* file_node, struct file** target);
int tmpfs_close(struct file* file);
int tmpfs_lookup(struct vnode* dir_node, struct vnode** target, const char* component_name);
int tmpfs_lookup_parent(struct vnode* dir_node, struct vnode** target_parent, struct vnode* task_root_node);
static int tmpfs_mknod(struct vnode* dir_node, struct vnode** target, const char* component_name,
	enum VNODE_TYPE type, struct file_operations* dev_fops, struct vnode_operations* dev_vops) ;
int tmpfs_create_common(struct vnode* dir_node, struct vnode** target, const char* component_name, enum TMPFS_TYPE type, enum VNODE_TYPE v_type);
int tmpfs_create(struct vnode* dir_node, struct vnode** target, const char* component_name);
// tmpfs 的 file_operations
extern struct file_operations tmpfs_file_ops;
// tmpfs 的 vnode_operations
extern struct vnode_operations tmpfs_vnode_ops;


#endif // TMPFS_H