// === 在新的 initramfs.c 中 ===
#include "initramfs.h"
#include "uart.h"
#include "buddy_alloc.h" // dynamic_malloc, dynamic_free
#include "string.h"    // strcmp, strncpy (或你的 manual_strncpy)
#include "vfs.h"       // E_ROFS 等錯誤碼

struct filesystem initramfs_filesystem = {
    .name = "initramfs_cpio", // 你選擇的檔案系統名稱
    .setup_mount = initramfs_setup_mount,
};
void initramfs_init (void){
    uart_send_string("[initramfs_init] Registering initramfs_cpio filesystem...\r\n");
    if (register_filesystem(&initramfs_filesystem) != E_OK) {
        uart_send_string("Error: [initramfs_init] Failed to register initramfs_cpio!\r\n");
    }
    uart_send_string("Attempting to mount initramfs...\r\n");
    int mkdir_ret = vfs_mkdir("/initramfs"); // 在根 tmpfs 中創建掛載點目錄
    if (mkdir_ret != E_OK && mkdir_ret != -E_EXIST) { // 如果不是已存在，則報錯
        uart_send_string("Failed to mkdir /initramfs, error: "); uart_send_int(mkdir_ret); uart_send_string("\r\n");
    } else {
        uart_send_string("/initramfs directory created or already exists.\r\n");
        int mount_ret = vfs_mount("/initramfs", "initramfs_cpio"); // 掛載
        if (mount_ret != E_OK) {
            uart_send_string("Failed to mount initramfs_cpio on /initramfs, error: "); uart_send_int(mount_ret); uart_send_string("\r\n");
        } else {
            uart_send_string("initramfs_cpio mounted successfully on /initramfs.\r\n");

            // // 測試一下 (可選)
            // struct vnode* test_node = NULL;
            // int lookup_ret = vfs_lookup("/initramfs/your_file_in_cpio.txt", &test_node);
            // if (lookup_ret == E_OK) {
            //     uart_send_string("Lookup test successful for /initramfs/your_file_in_cpio.txt. Name: ");
            //     uart_send_string( ((initramfs_inode_t*)test_node->internal)->name );
            //     uart_send_string("\r\n");
            //     test_node->ref_count--; // 釋放 lookup 得到的引用
            // } else {
            //     uart_send_string("Lookup test failed for /initramfs/your_file_in_cpio.txt, error: ");
            //     uart_send_int(lookup_ret); uart_send_string("\r\n");
            // }
        }
    }
}
// 將8位十六進制ASCII字串轉換為無符號整數
static unsigned int hex_to_uint(char *s, int n) {
    unsigned int r = 0;
    for (int i = 0; i < n; i++) {
        r *= 16;
        if (s[i] >= '0' && s[i] <= '9') {
            r += s[i] - '0';
        } else if (s[i] >= 'a' && s[i] <= 'f') {
            r += s[i] - 'a' + 10;
        } else if (s[i] >= 'A' && s[i] <= 'F') {
            r += s[i] - 'A' + 10;
        }
    }
    return r;
}

// 地址對齊輔助函數 (向上對齊到4位元組)
static uintptr_t align_up_4(uintptr_t addr) {
    return (addr + 3) & ~3;
}

// 創建 initramfs 內部節點 (initramfs_inode_t)
static initramfs_inode_t* create_initramfs_internal(const char* name, enum VNODE_TYPE type) {
    initramfs_inode_t* internal = (initramfs_inode_t*)dynamic_malloc(sizeof(initramfs_inode_t));
    if (!internal) return NULL;
    memset(internal, 0, sizeof(initramfs_inode_t));
    strncpy(internal->name, name, INITRAMFS_MAX_NAME_LEN); // 使用你的 strncpy
    internal->name[INITRAMFS_MAX_NAME_LEN] = '\0';
    internal->type = type;
    internal->num_children = 0;
    internal->dotdot_vnode = NULL; // 預設
    // internal->parent_internal = NULL; // 如果有這個欄位
    return internal;
}

// 創建 VFS vnode
static struct vnode* create_initramfs_vnode(struct mount* mount_info, initramfs_inode_t* internal_node) {
    struct vnode* vn = (struct vnode*)dynamic_malloc(sizeof(struct vnode));
    if (!vn) return NULL;
    vn->mount = mount_info;
    vn->f_ops = &initramfs_file_ops;
    vn->v_ops = &initramfs_vnode_ops;
    vn->internal = internal_node;
    vn->ref_count = 1; // 初始引用
    vn->type = internal_node->type;

    if (internal_node) {
        internal_node->v_node = vn; // 互相指
    }
    return vn;
}


// === initramfs vnode_operations ===
static int initramfs_lookup(struct vnode* dir_node, struct vnode** target, const char* component_name) {
    if (!dir_node || !dir_node->internal || !target || !component_name) return -E_INVAL;
    initramfs_inode_t* parent_internal = (initramfs_inode_t*)dir_node->internal;

    if (parent_internal->type != VNODE_DIR) return -E_NOTDIR;

    // uart_send_string("[initramfs_lookup] In dir '"); uart_send_string(parent_internal->name);
    // uart_send_string("', looking for '"); uart_send_string(component_name); uart_send_string("'\r\n");

    for (int i = 0; i < parent_internal->num_children; ++i) {
        struct vnode* child_vnode = parent_internal->children[i];
        initramfs_inode_t* child_internal = (initramfs_inode_t*)child_vnode->internal;
        if (strcmp(child_internal->name, component_name) == 0) {
            *target = child_vnode;
            (*target)->ref_count++;
            // uart_send_string("[initramfs_lookup] Found.\r\n");
            return E_OK;
        }
    }
    // uart_send_string("[initramfs_lookup] Not found.\r\n");
    return -E_NOENT;
}

// initramfs 是唯讀的
static int initramfs_create(struct vnode* dir_node, struct vnode** target, const char* component_name) {
    return -E_ROFS; // Read-only filesystem
}

static int initramfs_mkdir(struct vnode* dir_node, struct vnode** target, const char* component_name) {
    return -E_ROFS; // Read-only filesystem
}

// 處理 ".."
static int initramfs_lookup_parent(struct vnode* dir_node, struct vnode** target_parent, struct vnode* task_root_node_ignored) {
    if (!dir_node || !dir_node->internal || !target_parent) return -E_INVAL;
    initramfs_inode_t* current_internal = (initramfs_inode_t*)dir_node->internal;

    // 如果是 initramfs 的根，且已設定 dotdot_vnode (由 setup_mount 設定，指向掛載點的父節點)
    if (current_internal->dotdot_vnode != NULL) {
        *target_parent = current_internal->dotdot_vnode;
        (*target_parent)->ref_count++;
        uart_send_string("[initramfs_lookup_parent] Using stored dotdot_vnode for initramfs root\r\n");
        return E_OK;
    }
    // // 如果有內部父節點指標 (用於 initramfs 內非根目錄的 "..")
    // if (current_internal->parent_internal != NULL && current_internal->parent_internal->v_node != NULL) {
    //     *target_parent = current_internal->parent_internal->v_node;
    //     (*target_parent)->ref_count++;
    //     return E_OK;
    // }
    uart_send_string("Warning: [initramfs_lookup_parent] Reached fallback for '");
    uart_send_string(current_internal->name); uart_send_string("', '..' is itself.\r\n");
    *target_parent = dir_node;
    (*target_parent)->ref_count++;
    return E_OK;
}


struct vnode_operations initramfs_vnode_ops = {
    .lookup = initramfs_lookup,
    .create = initramfs_create,
    .mkdir = initramfs_mkdir,
    .lookup_parent = initramfs_lookup_parent, // 需要仔細考慮實現
};

// === initramfs file_operations ===
static int initramfs_read(struct file* file, void* buf, size_t len) {
    if (!file || !file->vnode || !file->vnode->internal || !buf) return -E_INVAL;
    initramfs_inode_t* internal = (initramfs_inode_t*)file->vnode->internal;

    if (internal->type != VNODE_FILE) return -E_ISDIR;
    if (file->f_pos >= internal->size) return 0; // EOF

    size_t remaining_size = internal->size - file->f_pos;
    size_t bytes_to_read = (len < remaining_size) ? len : remaining_size;

    if (bytes_to_read > 0) {
        memcpy(buf, internal->data + file->f_pos, bytes_to_read);
        file->f_pos += bytes_to_read;
    }
    return bytes_to_read;
}

static int initramfs_write(struct file* file, const void* buf, size_t len) {
    return -E_ROFS; // Read-only
}

static int initramfs_open(struct vnode* file_node, struct file** target) {
    // 通常不需要做特別的事，VFS層會處理file結構的創建
    return E_OK;
}

static int initramfs_close(struct file* file) {
    // 通常不需要做特別的事
    return E_OK;
}

struct file_operations initramfs_file_ops = {
    .read = initramfs_read,
    .write = initramfs_write,
    .open = initramfs_open,
    .close = initramfs_close,
};

// === 檔案系統掛載與註冊 ===

// 輔助函數：在 initramfs 內部根據路徑創建/查找 vnode
// root_internal_of_initramfs: 指向 initramfs 檔案系統根的 internal node
// fullpath: 來自 CPIO 的完整路徑，例如 "dir1/file.txt"
// type: VNODE_FILE 或 VNODE_DIR
// data, size: 僅對檔案有效
static struct vnode* initramfs_add_node_recursive(struct mount* mount_info, initramfs_inode_t* root_internal_of_initramfs, const char* fullpath, enum VNODE_TYPE type, const char* data_ptr, size_t file_size) {
    char path_copy[INITRAMFS_MAX_NAME_LEN + 1];
    strncpy(path_copy, fullpath, INITRAMFS_MAX_NAME_LEN); // 使用你的 strncpy
    path_copy[INITRAMFS_MAX_NAME_LEN] = '\0';

    initramfs_inode_t* current_parent_internal = root_internal_of_initramfs;
    char* component = path_copy;
    char* next_slash;
    //if component is directory, it should end with '/'
    while ((next_slash = strchr(component, '/')) != NULL) { // 是路徑中的目錄部分
        *next_slash = '\0';
        if (strlen(component) == 0) { // 處理開頭的 //
            component = next_slash + 1;
            continue;
        }

        struct vnode* found_dir_vnode = NULL;
        // 在 current_parent_internal 中查找名為 component 的目錄
        for (int i = 0; i < current_parent_internal->num_children; ++i) {
            initramfs_inode_t* child_i = (initramfs_inode_t*)current_parent_internal->children[i]->internal;
            if (child_i->type == VNODE_DIR && strcmp(child_i->name, component) == 0) {
                found_dir_vnode = current_parent_internal->children[i];
                break;
            }
        }

        if (found_dir_vnode) {
            current_parent_internal = (initramfs_inode_t*)found_dir_vnode->internal;
        } else { // 目錄不存在，創建它
            if (current_parent_internal->num_children >= MAX_CHILDREN_PER_DIR) {
                uart_send_string("Error: [initramfs] Max children exceeded for dir '");
                uart_send_string(current_parent_internal->name); uart_send_string("'\r\n");
                return NULL; // 或其他錯誤處理
            }
            initramfs_inode_t* new_dir_internal = create_initramfs_internal(component, VNODE_DIR);
            if (!new_dir_internal) return NULL;
            // new_dir_internal->parent_internal = current_parent_internal; // 如果需要
            struct vnode* new_dir_vnode = create_initramfs_vnode(mount_info, new_dir_internal);
            if (!new_dir_vnode) { dynamic_free(new_dir_internal); return NULL; }

            current_parent_internal->children[current_parent_internal->num_children++] = new_dir_vnode;
            current_parent_internal = new_dir_internal;
        }
        component = next_slash + 1;
    }

    // 最後一個組件 (檔名或空目錄名)
    if (strlen(component) == 0 && type == VNODE_DIR) { // 例如 CPIO 條目 "somedir/"
        // 這表示 component 是父目錄名，已經處理完畢
        return current_parent_internal->v_node;
    }
    if (strlen(component) > 0) {
         if (current_parent_internal->num_children >= MAX_CHILDREN_PER_DIR) {
            uart_send_string("Error: [initramfs] Max children exceeded for dir '");
            uart_send_string(current_parent_internal->name); uart_send_string("'\r\n");
            return NULL;
        }
        initramfs_inode_t* new_node_internal = create_initramfs_internal(component, type);
        if (!new_node_internal) return NULL;
        new_node_internal->parent_internal = current_parent_internal; // 如果需要

        if (type == VNODE_FILE) {
            new_node_internal->data = data_ptr;
            new_node_internal->size = file_size;
        }
        struct vnode* new_node_vnode = create_initramfs_vnode(mount_info, new_node_internal);
        if (!new_node_vnode) { dynamic_free(new_node_internal); return NULL; }

        current_parent_internal->children[current_parent_internal->num_children++] = new_node_vnode;
        return new_node_vnode;
    }
    return NULL; // 不應該到這裡
}


int initramfs_setup_mount(struct filesystem* fs_info, struct mount* mount_info, struct vnode* logical_parent_of_mount_point) {
    uart_send_string("[initramfs_setup_mount] Setting up initramfs mount.\r\n");

    // 1. 創建 initramfs 的根 internal node 和 vnode
    initramfs_inode_t* initramfs_root_internal = create_initramfs_internal("/", VNODE_DIR); // 內部名稱設為 "/"
    if (!initramfs_root_internal) return -E_NOMEM;

    // 根據助教的指導，設定 ".." 指標
    initramfs_root_internal->dotdot_vnode = logical_parent_of_mount_point;
    if (initramfs_root_internal->dotdot_vnode) {
        // initramfs_root_internal->dotdot_vnode->ref_count++; // mount 結構會持有這個引用，setup_mount 只是使用
                                                            // vfs_mount 傳遞時，logical_parent_of_mount_point 已有引用
                                                            // 如果 initramfs_inode 要永久持有它，才需要增加。
                                                            // 暫時不增加，假設 dotdot_vnode 只是個臨時參考，或其生命週期由 VFS 層保證
    }

    struct vnode* initramfs_root_vnode = create_initramfs_vnode(mount_info, initramfs_root_internal);
    if (!initramfs_root_vnode) {
        // if (initramfs_root_internal->dotdot_vnode) initramfs_root_internal->dotdot_vnode->ref_count--;
        dynamic_free(initramfs_root_internal);
        return -E_NOMEM;
    }
    mount_info->root = initramfs_root_vnode; // 設定 guest root
    mount_info->fs = fs_info; // 設定檔案系統資訊
    // mount_info->fs 和 mount_info->mount_point_vnode 已由 vfs_mount 設定

    // 2. 解析 CPIO 存檔
    char *current_ptr = (char*)(g_initramfs_addr); // 使用 linker script 提供的符號
    if (!current_ptr) {
        uart_send_string("Error: [initramfs] g_initramfs_addr is NULL!\r\n");
        // 清理已分配的 root_vnode 等
        dynamic_free(initramfs_root_internal);
        dynamic_free(initramfs_root_vnode);
        mount_info->root = NULL;
        return -E_NOENT;
    }
    uart_send_string("[initramfs_setup_mount] CPIO archive at: 0x"); uart_send_hex((uintptr_t)current_ptr); uart_send_string("\r\n");


    while (1) {
        struct cpio_newc_header *header = (struct cpio_newc_header *)current_ptr;
        if (strncmp(header->c_magic, "070701", 6) != 0) {
            uart_send_string("Error: [initramfs] Invalid CPIO magic.\r\n");
            break; // 或者 return 錯誤
        }

        unsigned int namesize = hex_to_uint(header->c_namesize, 8);
        unsigned int filesize = hex_to_uint(header->c_filesize, 8);
        unsigned int mode = hex_to_uint(header->c_mode, 8);

        char *pathname_ptr = current_ptr + sizeof(struct cpio_newc_header);
        if (strcmp(pathname_ptr, "TRAILER!!!") == 0) {
            uart_send_string("[initramfs_setup_mount] CPIO trailer found. Parsing complete.\r\n");
            break; // 結束
        }

        // uart_send_string("[initramfs] File: "); uart_send_string(pathname_ptr);
        // uart_send_string(", Mode: "); uart_send_hex(mode);
        // uart_send_string(", Size: "); uart_send_int(filesize); uart_send_string("\r\n");

        uintptr_t data_ptr_aligned = align_up_4((uintptr_t)pathname_ptr + namesize);
        const char *filedata_ptr = (const char*)data_ptr_aligned;

        enum VNODE_TYPE type = VNODE_FILE; // 預設
        // 檢查模式是否為目錄 (S_IFMT & S_IFDIR)
        // S_IFMT is 0xF000, S_IFDIR is 0x4000.
        if ((mode & 0xF000) == 0x4000) { // 簡化判斷，通常是 S_ISDIR(mode)
            type = VNODE_DIR;
        }

        if (strcmp(pathname_ptr, ".") == 0) { // "." 通常代表根目錄本身，已處理
            // do nothing, root is already created.
        } else {
            initramfs_add_node_recursive(mount_info, initramfs_root_internal, pathname_ptr, type, filedata_ptr, filesize);
        }

        current_ptr = (char*)align_up_4(data_ptr_aligned + filesize);
    }

    uart_send_string("[initramfs_setup_mount] initramfs mounted successfully.\r\n");
    return E_OK;
}


