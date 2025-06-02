// vfs.c
#include "vfs.h"
#include "tmpfs.h" // 包含 tmpfs 的定義和函式
#include "uart.h" // 包含 UART 函式 (例如 uart_send_string)
#include "buddy_alloc.h" // 包含動態記憶體分配函式 (例如 dynamic_malloc, dynamic_free)
#include "strcmp.h"
#include "thread.h" 
#include <stdio.h> 	// For printf, remove for kernel

// 假設一個簡單的已註冊檔案系統列表 (實際應用中可能需要更動態的結構)
static struct filesystem* registered_fs_list[MAX_REGISTERED_FS];
static int num_registered_fs = 0;
// 全域的掛載列表 (除了 rootfs 外，管理其他掛載點)
struct mount* mounted_fs_list[MAX_MOUNTED_FS];
int num_mounted_fs = 0;
// 全域的根檔案系統掛載點 (在某處定義並初始化)
struct mount* rootfs = NULL;
int first_mount_fs=1;

// static struct file* global_fd_table[MAX_OPEN_FILES_PER_PROCESS];
// static int next_fd = 0; // 非常簡化的 fd 分配
//init vfs
void kernel_init_vfs() {
	uart_send_string("[kernel_init_vfs] Initializing VFS...\r\n");

	// 1. 初始化 tmpfs 模組 (這會向 VFS 註冊 tmpfs)
	tmpfs_init();

	// 2. 準備掛載根檔案系統 ("tmpfs")
	struct filesystem* fs_type_to_mount = find_filesystem("tmpfs");
	if (!fs_type_to_mount) {
		uart_send_string("CRITICAL:[kernel_init_vfs] tmpfs filesystem type not found!\r\n");
		// Kernel panic or halt
		return;
	}

	// 3. 建立根檔案系統的 mount 結構
	// 	這個 rootfs mount 結構是 VFS 層的，不是 tmpfs 內部自己用的。
	// 	它代表 VFS 知道 "/" 是由一個 tmpfs 提供的。
	rootfs = (struct mount*)dynamic_malloc(sizeof(struct mount)); // 用你的記憶體分配器
	if (!rootfs) {
		uart_send_string("CRITICAL:[kernel_init_vfs] Failed to allocate memory for rootfs mount structure!\r\n");
		// Kernel panic or halt
		return;
	}
	// rootfs->root 和 rootfs->fs 會由 setup_mount 填寫

	// 4. 呼叫 tmpfs 的 setup_mount 來初始化 tmpfs 並設定 rootfs 的內容
	// 	實驗手冊提到: "lookup might not be available, you can call setup_mount directly to mount it."
	int ret = fs_type_to_mount->setup_mount(fs_type_to_mount, rootfs,NULL);
	if (ret != E_OK) {
		uart_send_string("CRITICAL:[kernel_init_vfs] Failed to setup_mount for rootfs!\r\n");
		dynamic_free(rootfs); // 清理
		rootfs = NULL;
		// Kernel panic or halt
		return;
	}
	uart_send_string("[kernel_init_vfs] Rootfs (tmpfs) mounted successfully. Root vnode internal name: '");
	uart_send_string(((tmpfs_inode_t*)rootfs->root->internal)->name);
	uart_send_string("'\r\n"); // 假設 tmpfs internal 結構和名稱
}
// --- 檔案系統註冊 ---
int register_filesystem(struct filesystem* fs) {
	if (num_registered_fs < MAX_REGISTERED_FS) {
		for (int i = 0; i < num_registered_fs; ++i) {
			if (strcmp(registered_fs_list[i]->name, fs->name) == 0) {
				uart_send_string("[register_filesystem] Filesystem ");
				uart_send_string(fs->name);
				uart_send_string(" already registered.\r\n");
				return E_EXIST; // Already registered
			}
		}
		registered_fs_list[num_registered_fs++] = fs;
		uart_send_string("[register_filesystem] Filesystem '");
		uart_send_string(fs->name);
		uart_send_string("' registered.\r\n");
		return E_OK;
	}
	uart_send_string("Error: [register_filesystem] Cannot register filesystem '");
	uart_send_string(fs->name);
	uart_send_string("', list full.\r\n");
	return E_NOMEM; // Or some other error for list full
}

struct filesystem* find_filesystem(const char* name) {
	for (int i = 0; i < num_registered_fs; ++i) {
		if (strcmp(registered_fs_list[i]->name, name) == 0) {
			return registered_fs_list[i];
		}
	}
	return NULL;
}


// --- 核心 VFS Pathname Lookup ---
// 新增一個內部路徑解析輔助函數
// base_node: 相對路徑的起點 (通常是 cwd)
// root_node: 絕對路徑的起點 (任務的 root_dir)
int vfs_resolve_path(const char* pathname, struct vnode* base_node, struct vnode* root_node, struct vnode** target,int resolve_flags) {
    if (!pathname || !target || !root_node) return E_INVAL;
    if (!base_node && pathname[0] != '/') return E_INVAL; // 相對路徑但沒有 base_node

    uart_send_string("[vfs_resolve_path] Path: '"); uart_send_string(pathname);
    uart_send_string("', Base: '");
    if (base_node && base_node->internal) uart_send_string(((tmpfs_inode_t*)base_node->internal)->name); else uart_send_string("N/A");
    uart_send_string("', Root: '");
    if (root_node && root_node->internal) uart_send_string(((tmpfs_inode_t*)root_node->internal)->name); else uart_send_string("N/A");
    uart_send_string("'\r\n");


    struct vnode* current_vnode;
    char path_copy[MAX_PATHNAME_LEN + 1];
    strcpy(path_copy, pathname);
    path_copy[MAX_PATHNAME_LEN] = '\0';

    char* p = path_copy;

    if (p[0] == '/') {
        current_vnode = root_node; // 絕對路徑從任務的根目錄開始
        p++;
        if (*p == '\0') { // 路徑是 "/"
            *target = root_node;
            (*target)->ref_count++;
            return E_OK;
        }
    } else {
        current_vnode = base_node; // 相對路徑從 CWD 開始
    }
    current_vnode->ref_count++; // 持有起始節點的引用
	int is_last_component = 0; // 用來標記是否是最後一個組件
    char* next_component;
    while (*p != '\0') {
        // 跳過多餘的 '/'
        while (*p == '/') p++;
        if (*p == '\0') break;

        next_component = p;
        char* separator = strchr(p, '/');
        if (separator) {
            *separator = '\0';
            p = separator + 1;
        } else {
			is_last_component= 1; // 標記為最後一個組件
            p += strlen(p); // 移動到字串末尾
        }

        if (strlen(next_component) == 0) continue; // 應該在上面 while(*p=='/') 處理掉了

        uart_send_string("[vfs_resolve_path] Current vnode: '");
        if (current_vnode->internal) uart_send_string(((tmpfs_inode_t*)current_vnode->internal)->name);
        uart_send_string("', looking for: '"); uart_send_string(next_component); uart_send_string("'\r\n");

        struct vnode* found_vnode = NULL;

        if (strcmp(next_component, ".") == 0) {
            // '.' 指向當前目錄，不需要改變 current_vnode，但需要增加引用計數（因為我們期望返回一個新的引用）
            // 由於 current_vnode 已經持有引用，這裡什麼都不做，或者說 found_vnode = current_vnode; current_vnode->ref_count++;
            // 但因為下面會 current_vnode->ref_count--; 然後 current_vnode = found_vnode; 所以實際上是平衡的
            found_vnode = current_vnode;
            found_vnode->ref_count++; // 為 "found_vnode" 增加一次引用
        } else if (strcmp(next_component, "..") == 0) {
            if (!current_vnode->v_ops || !current_vnode->v_ops->lookup_parent) { // 需要新的 vnode_operation
                current_vnode->ref_count--;
                return E_INVAL; // 不支援查找父目錄
            }
            // 查找父目錄
            int ret_parent = current_vnode->v_ops->lookup_parent(current_vnode, &found_vnode, root_node);
            if (ret_parent != E_OK) {
                current_vnode->ref_count--;
                return ret_parent;
            }
            // lookup_parent 應該已經處理了 found_vnode 的 ref_count
        } else {
            // 一般組件查找
            if (!current_vnode->v_ops || !current_vnode->v_ops->lookup) {
                current_vnode->ref_count--;
                return E_INVAL;
            }
            int ret_lookup = current_vnode->v_ops->lookup(current_vnode, &found_vnode, next_component);
            if (ret_lookup != E_OK) {
                current_vnode->ref_count--;
                return ret_lookup;
            }
            // lookup 應該已經處理了 found_vnode 的 ref_count
        }

        current_vnode->ref_count--; // 釋放對上一級 current_vnode 的引用
        current_vnode = found_vnode; // found_vnode 已經被其查找函數增加了引用計數

        // 檢查掛載點 (這部分邏輯來自你之前的代碼，並已修正)
        struct mount* jumped_mount = NULL;
        for (int i = 0; i < num_mounted_fs; ++i) {
            if (mounted_fs_list[i]->mount_point_vnode == current_vnode) {
                jumped_mount = mounted_fs_list[i];
                uart_send_string("[vfs_resolve_path] Matched mount_point_vnode: '");
                if (current_vnode->internal) uart_send_string(((tmpfs_inode_t*)current_vnode->internal)->name);
                uart_send_string("'\r\n");
                break;
            }
        }
        if (jumped_mount && (!is_last_component || !(resolve_flags & RESOLVE_NO_CROSS_MOUNT))){
			// 如果是掛載點，並且不是最後一個組件，則跳到 guest root
            uart_send_string("[vfs_resolve_path] Crossing mount point from '");
            if (current_vnode->internal) uart_send_string(((tmpfs_inode_t*)current_vnode->internal)->name);
            uart_send_string("' to FS '"); uart_send_string(jumped_mount->fs->name);
            uart_send_string("'s root '");
            if (jumped_mount->root && jumped_mount->root->internal) uart_send_string(((tmpfs_inode_t*)jumped_mount->root->internal)->name);
            uart_send_string("'\r\n");

            current_vnode->ref_count--;      // 釋放對 mount_point_vnode 的引用
            current_vnode = jumped_mount->root; // 跳到 guest root
            current_vnode->ref_count++;      // 增加對 guest root 的引用
        }
    }

    *target = current_vnode; // current_vnode 應已持有正確的引用計數
    uart_send_string("[vfs_resolve_path] Resolved '"); uart_send_string(pathname);
    uart_send_string("' to vnode '");
    if ((*target)->internal) uart_send_string(((tmpfs_inode_t*)(*target)->internal)->name);
    uart_send_string("'\r\n");
    return E_OK;
}
int vfs_lookup(const char* pathname, struct vnode** target, int resolve_flags) {
    // 這個舊的 vfs_lookup 假設是從全域 rootfs 開始的絕對路徑查找
    // Basic 3 中，大部分查找應該基於任務的 cwd 和 root_dir
    thread_t* current_task = get_current();
    if (!current_task) { // 內核早期或特殊情況
        if (pathname[0] != '/') return E_INVAL; // 沒有 CWD 就只能是絕對路徑
		uart_send_string("[vfs_lookup] Using global rootfs for path: '");
		uart_send_string(pathname); uart_send_string("'\r\n");
        return vfs_resolve_path(pathname, NULL, rootfs->root, target,resolve_flags);
    }
    return vfs_resolve_path(pathname, current_task->cwd, current_task->root_dir, target,resolve_flags);
}

int vfs_mknod(const char* pathname, enum VNODE_TYPE type, struct file_operations* dev_fops, struct vnode_operations* dev_vops) {
    uart_send_string("[vfs_mknod] Path: '"); uart_send_string(pathname);
    uart_send_string("', Type: "); uart_send_int(type); uart_send_string("\r\n");

    if (!pathname || !dev_fops) return -E_INVAL;
    if (!rootfs || !rootfs->root) { // 確保全域根已掛載
        uart_send_string("Error: [vfs_mknod] Rootfs not available.\r\n");
        return -E_NOENT; // 或者一個更合適的早期錯誤
    }

    thread_t* current_task = get_current();
    struct vnode* effective_cwd;
    struct vnode* effective_root_dir;

    if (current_task && current_task->cwd && current_task->root_dir) {
        effective_cwd = current_task->cwd;
        effective_root_dir = current_task->root_dir;
        uart_send_string("[vfs_mknod] Using task's CWD & root_dir.\r\n");
    } else {
        // 無任務上下文，或任務 VFS 上下文未完全初始化 (例如核心初始化階段)
        // 此時，路徑必須是絕對的，CWD 的概念是 rootfs->root
        if (pathname[0] != '/') {
            uart_send_string("Error: [vfs_mknod] No task context, pathname must be absolute: ");
            uart_send_string(pathname); uart_send_string("\r\n");
            return -E_INVAL;
        }
        effective_cwd = rootfs->root;      // 對於絕對路徑，CWD 在此不直接使用，但 resolve_path 會用到 base
        effective_root_dir = rootfs->root; // 絕對路徑的起點
        uart_send_string("[vfs_mknod] No/incomplete task context, using global rootfs for path resolution.\r\n");
    }

    // 1. 分離父目錄路徑和新節點名稱
    char path_copy[MAX_PATHNAME_LEN + 1];
    // manual_strncpy(path_copy, pathname, MAX_PATHNAME_LEN); // 改用你的 strncpy
    // path_copy[MAX_PATHNAME_LEN] = '\0';
    // 為簡化，假設 strncpy 來自 string.h
    strncpy(path_copy, pathname, MAX_PATHNAME_LEN);
    path_copy[MAX_PATHNAME_LEN] = '\0';


    char* last_slash = strrchr(path_copy, '/');
    char parent_dir_path_str[MAX_PATHNAME_LEN + 1]; // 改名以區分
    const char* component_name;

    if (last_slash) {
        component_name = last_slash + 1;
        if (strlen(component_name) == 0) { // 路徑以 '/' 結尾，例如 "/dev/"
             uart_send_string("Error: [vfs_mknod] Component name cannot be empty (path ends with '/').\r\n");
             return -E_INVAL;
        }
        if (last_slash == path_copy) { // e.g., "/devnode" (父目錄是 "/")
            strncpy(parent_dir_path_str, "/", MAX_PATHNAME_LEN + 1);
        } else { // e.g., "/some/dir/devnode"
            *last_slash = '\0'; // path_copy 現在是父目錄字串
            strncpy(parent_dir_path_str, path_copy, MAX_PATHNAME_LEN + 1);
        }
    } else { // e.g., "devnode" (相對於 effective_cwd)
        component_name = path_copy;
        if (strlen(component_name) == 0) {
             uart_send_string("Error: [vfs_mknod] Component name is empty.\r\n");
             return -E_INVAL;
        }
        // 父目錄是 "effective_cwd"，用 "." 表示
        strncpy(parent_dir_path_str, ".", MAX_PATHNAME_LEN + 1);
    }
    // parent_dir_path_str[MAX_PATHNAME_LEN] = '\0'; // strncpy 應該處理，或手動確保

    uart_send_string("[vfs_mknod] Parent path to resolve: '"); uart_send_string(parent_dir_path_str);
    uart_send_string("', component to create: '"); uart_send_string(component_name); uart_send_string("'\r\n");


    // 2. 解析父目錄
    struct vnode* parent_dir_vnode = NULL;
    int ret = vfs_resolve_path(parent_dir_path_str, effective_cwd, effective_root_dir, &parent_dir_vnode,0);
    if (ret != E_OK) {
        uart_send_string("Error: [vfs_mknod] Failed to resolve parent directory '");
        uart_send_string(parent_dir_path_str); uart_send_string("'. Error: "); uart_send_int(ret); uart_send_string("\r\n");
        return ret;
    }

    // 3. 檢查父目錄是否支援 mknod
    if (parent_dir_vnode->type != VNODE_DIR) {
        uart_send_string("Error: [vfs_mknod] Parent path '"); uart_send_string(parent_dir_path_str);
        uart_send_string("' is not a directory.\r\n");
        parent_dir_vnode->ref_count--;
        return -E_NOTDIR;
    }
    if (!parent_dir_vnode->v_ops || !parent_dir_vnode->v_ops->mknod) {
        uart_send_string("Error: [vfs_mknod] Filesystem for '"); uart_send_string(parent_dir_path_str);
        uart_send_string("' does not support mknod.\r\n");
        parent_dir_vnode->ref_count--;
        return -E_PERM;
    }

    // 4. 呼叫底層檔案系統的 mknod
    struct vnode* new_device_vnode = NULL;
    ret = parent_dir_vnode->v_ops->mknod(parent_dir_vnode, &new_device_vnode, component_name, type, dev_fops, dev_vops);

    parent_dir_vnode->ref_count--; // 釋放對父目錄 vnode 的引用 (來自 vfs_resolve_path)

    if (ret == E_OK) {
        uart_send_string("[vfs_mknod] Successfully created device node '");
        uart_send_string(pathname); uart_send_string("'.\r\n");
        // new_device_vnode 的 ref_count 是 1 (由底層 mknod 設定)
        // 核心內部 vfs_mknod 函數通常不直接 "使用" 這個新節點，
        // 而是為更高層（如 sys_mknod 或初始化程式碼）創建它。
        // 如果沒有其他核心內部使用者，則減少引用。
        new_device_vnode->ref_count--;
    } else {
        uart_send_string("Error: [vfs_mknod] Underlying FS mknod for '");
        uart_send_string(component_name); uart_send_string("' failed. Error: ");
        uart_send_int(ret); uart_send_string("\r\n");
    }
    return ret;
}
// --- VFS API 實作 ---
int vfs_open(const char* pathname, int flags, struct file** target_file) {
	if (!pathname || !target_file) return E_INVAL;
	uart_send_string("[vfs_open] path '");
	uart_send_string(pathname);
	uart_send_string("', flags ");
	uart_send_int(flags);
	uart_send_string("\r\n");

	thread_t* current_task = get_current();
    if (!current_task) return E_INVAL; // 或者 E_INVAL，沒有當前任務上下文

    struct vnode* node_to_open = NULL;
    // 使用 vfs_resolve_path 進行路徑解析
    int ret = vfs_resolve_path(pathname, current_task->cwd, current_task->root_dir, &node_to_open,0);

	if (ret != E_OK) {
        if (ret == E_NOENT && (flags & O_CREAT)) {
            // 處理 O_CREAT: 需要找到父目錄和檔名
            // 這部分邏輯也需要使用 vfs_resolve_path 來找到父目錄
            char path_copy_for_create[MAX_PATHNAME_LEN + 1];
            strcpy(path_copy_for_create, pathname);
            path_copy_for_create[MAX_PATHNAME_LEN] = '\0';

            char* last_slash = strrchr(path_copy_for_create, '/');
            struct vnode* parent_dir_vnode = NULL;
            const char* filename_to_create = NULL;
            char parent_path_buffer[MAX_PATHNAME_LEN +1];

            if (last_slash) { // e.g., "/path/to/file" or "path/to/file"
                if (last_slash == path_copy_for_create && path_copy_for_create[0] == '/') { // e.g. "/file"
                    filename_to_create = last_slash + 1;
                    strcpy(parent_path_buffer, "/");
                } else {
                    filename_to_create = last_slash + 1;
                    *last_slash = '\0'; // path_copy_for_create 現在是父目錄路徑
                    strcpy(parent_path_buffer, path_copy_for_create);
                }
            } else { // e.g. "file" (在 CWD 下)
                filename_to_create = path_copy_for_create;
                // 父目錄就是 CWD，但我們需要一個路徑字串來 resolve CWD
                // 這裡有一個小問題：如果 CWD 本身沒有一個規範的路徑字串，如何傳給 vfs_resolve_path？
                // 解決方法1: 直接使用 current_task->cwd 作為 parent_dir_vnode
                // 解決方法2: vfs_resolve_path 可以接受 "." 來代表 base_node
                strcpy(parent_path_buffer, "."); // 或者傳遞一個特殊標記
            }
            parent_path_buffer[MAX_PATHNAME_LEN] = '\0';

            if (strlen(filename_to_create) == 0) return E_INVAL; // 不能創建名為空的檔案

            // 解析父目錄
            int parent_ret;
            if (last_slash == NULL) { // 創建在 CWD
                parent_dir_vnode = current_task->cwd;
                parent_dir_vnode->ref_count++; // 手動增加引用，因為我們直接用了它
                parent_ret = E_OK;
            } else {
                parent_ret = vfs_resolve_path(parent_path_buffer, current_task->cwd, current_task->root_dir, &parent_dir_vnode,0);
            }

            if (parent_ret != E_OK) {
                uart_send_string("[vfs_open] (O_CREAT) Failed to resolve parent dir '");
                uart_send_string(parent_path_buffer); uart_send_string("'\r\n");
                return parent_ret;
            }

            // ... (原有的 create 邏輯，使用 parent_dir_vnode 和 filename_to_create) ...
            // ... 記得在 create 之後 parent_dir_vnode->ref_count--; ...
            // (這部分 create 的邏輯你需要重構以適應新的 resolve 方式)
            // 這裡省略了 O_CREAT 的詳細重構，它比較複雜
             uart_send_string("[vfs_open] O_CREAT for '"); uart_send_string(pathname);
             uart_send_string("' (filename '"); uart_send_string(filename_to_create);
             uart_send_string("') in dir '"); uart_send_string(((tmpfs_inode_t*)parent_dir_vnode->internal)->name);
             uart_send_string("'\r\n");

             if (!parent_dir_vnode->v_ops || !parent_dir_vnode->v_ops->create) {
                 parent_dir_vnode->ref_count--;
                 return E_INVAL;
             }
             ret = parent_dir_vnode->v_ops->create(parent_dir_vnode, &node_to_open, filename_to_create);
             parent_dir_vnode->ref_count--; // 釋放 lookup (或直接使用 CWD) 得到的 parent ref

             if (ret != E_OK) {
                 return ret;
             }
             // node_to_open 的 ref_count 已被 create 設定
        } else {
            return ret; // 查找失敗且沒有 O_CREAT，或 O_CREAT 但其他錯誤
        }
    }

	// 檢查節點類型 (例如，不能 open 一個目錄來讀寫內容，除非是 readdir)
	if (node_to_open->type == VNODE_DIR && !(flags & O_DIRECTORY)) { // O_DIRECTORY 也是需要自己定義的旗標
		// 暫時允許打開目錄以簡化，但實際系統中，打開目錄通常是為了列舉
		// uart_send_string("vfs_open: trying to open a directory as a file.\r\n");
		// node_to_open->ref_count--;
		// return E_ISDIR;
	}


	// 建立 file handle
	struct file* new_file = (struct file*)dynamic_malloc(sizeof(struct file)); // 用你的記憶體分配器
	if (!new_file) {
		node_to_open->ref_count--; // 釋放 lookup 或 create 得到的引用
		return E_NOMEM;
	}

	new_file->vnode = node_to_open; // node_to_open 的 ref_count 由 lookup/create 保證
	new_file->f_pos = 0;
	new_file->flags = flags;
	new_file->f_ops = node_to_open->f_ops; // 從 vnode 繼承 file_operations

	// 呼叫底層檔案系統的 open (如果有的話)
	if (new_file->f_ops && new_file->f_ops->open) {
		ret = new_file->f_ops->open(node_to_open, &new_file); // 這裡 target 其實不太需要，因為 file 已經建了
		if (ret != E_OK) {
			dynamic_free(new_file);
			node_to_open->ref_count--; // 釋放 vnode 引用
			return ret;
		}
	}

	// 分配 fd 到 current_task->fd_table
	int fd = -1;
    for(int i = 0; i < MAX_PROCESS_OPEN_FILES; ++i) { // 使用 MAX_PROCESS_OPEN_FILES
        if(current_task->fd_table[i] == NULL) {
            current_task->fd_table[i] = new_file;
            fd = i;
            break;
        }
    }
	if (fd == -1) {
		// fd 表滿了
		if (new_file->f_ops && new_file->f_ops->close) { // 嘗試回滾 FS 層的 open
			new_file->f_ops->close(new_file);
		}
		dynamic_free(new_file);
		node_to_open->ref_count--;
		return E_MAX_FILES;
	}


	*target_file = new_file;
	// caller_fd = fd; // 如果要返回 fd 而不是 file*
	uart_send_string("[vfs_open] success for path '");
	uart_send_string(pathname);
	uart_send_string("', assigned fd ");
	uart_send_int(fd);
	uart_send_string(" (conceptually)\r\n");
	return E_OK;
}

int vfs_close(struct file* file_to_close) {
	if (!file_to_close) return E_INVAL;

	uart_send_string("[vfs_close] closing file for vnode '");
	uart_send_string(((tmpfs_inode_t*)file_to_close->vnode->internal)->name);
	uart_send_string("'\r\n");
	thread_t* current_task=get_current();
	// (為 Basic 3 準備) 從 fd 表中移除
	for(int i=0; i < MAX_OPEN_FILES_PER_PROCESS; ++i) {
		if(current_task->fd_table[i] == file_to_close) {
			current_task->fd_table[i] = NULL;
			break;
		}
	}

	if (file_to_close->f_ops && file_to_close->f_ops->close) {
		file_to_close->f_ops->close(file_to_close);
	}

	file_to_close->vnode->ref_count--; // 釋放對 vnode 的引用
	uart_send_string("[vfs_close] vnode '");
	uart_send_string(((tmpfs_inode_t*)file_to_close->vnode->internal)->name);
	uart_send_string("' ref_count is now ");
	uart_send_int(file_to_close->vnode->ref_count);
	uart_send_string(", should be freed if 0.\r\n");

	dynamic_free(file_to_close); // 釋放 file handle 結構本身
	return E_OK;
}

int vfs_read(struct file* file_to_read, void* buf, size_t len) {
	if (!file_to_read || !buf) return E_INVAL;
	if (!file_to_read->f_ops || !file_to_read->f_ops->read) return E_INVAL; // No read op
	// 權限檢查 (例如，是否以可讀模式打開)
	// if (!(file_to_read->flags & O_RDONLY) && !(file_to_read->flags & O_RDWR)) return E_ACCESS_DENIED;

	uart_send_string("vfs_read: for vnode '");
	uart_send_string(((tmpfs_inode_t*)file_to_read->vnode->internal)->name);
	uart_send_string("', len ");
	uart_send_int(len);
	uart_send_string("\r\n");
	return file_to_read->f_ops->read(file_to_read, buf, len);
}

int vfs_write(struct file* file_to_write, const void* buf, size_t len) {
	if (!file_to_write || !buf) return E_INVAL;
	if (!file_to_write->f_ops || !file_to_write->f_ops->write) return E_INVAL; // No write op
	// 權限檢查
	// if (!(file_to_write->flags & O_WRONLY) && !(file_to_write->flags & O_RDWR)) return E_ACCESS_DENIED;

	// uart_send_string("[vfs_write] for vnode '");
	// uart_send_string(((tmpfs_inode_t*)file_to_write->vnode->internal)->name);
	// uart_send_string("', len ");
	// uart_send_int(len);
	// uart_send_string("\r\n");
	return file_to_write->f_ops->write(file_to_write, buf, len);
}

int vfs_mkdir(const char* pathname) {
	if (!pathname) return E_INVAL;
	uart_send_string("[vfs_mkdir] path '");
	uart_send_string(pathname);
	uart_send_string("'\r\n");

	// 1. 找到父目錄和要建立的目錄名
	char path_copy[MAX_PATHNAME_LEN + 1];
	strcpy(path_copy, pathname);
	path_copy[MAX_PATHNAME_LEN] = '\0';

	char* last_slash = strrchr(path_copy, '/');
	struct vnode* parent_dir_vnode = NULL;
	const char* dirname_to_create = NULL;
	// uart_send_string("[vfs_mkdir] last_slash ='");
	// uart_send_string(last_slash);
	// uart_send_string("'\r\n");
	if (last_slash) {
		if (last_slash == path_copy && *(last_slash+1) != '\0') { // e.g., "/newdir"
			dirname_to_create = last_slash + 1;
			int lookup_ret = vfs_lookup("/", &parent_dir_vnode,0);
			if(lookup_ret != E_OK) return lookup_ret;
		} else if (last_slash != path_copy) { // e.g., "/existing_dir/newdir"
			dirname_to_create = last_slash + 1;
			*last_slash = '\0'; // path_copy is now parent dir path
			int lookup_ret = vfs_lookup(path_copy, &parent_dir_vnode,0);
			if(lookup_ret != E_OK) return lookup_ret;
		} else { // e.g. "/"
			return E_EXIST; // Cannot mkdir "/"
		}
	} else { // e.g., "newdir" (相對路徑，Basic 1 假設都是絕對路徑)
		return E_INVAL;
	}

	if (strlen(dirname_to_create) == 0) {
		if(parent_dir_vnode) parent_dir_vnode->ref_count--;
		return E_INVAL; // Cannot create dir with empty name
	}

	if (!parent_dir_vnode->v_ops || !parent_dir_vnode->v_ops->mkdir) {
		parent_dir_vnode->ref_count--;
		return E_INVAL; // Parent doesn't support mkdir
	}

	struct vnode* new_dir_vnode = NULL;
	uart_send_string("[vfs_mkdir] creating dir '");
	uart_send_string(dirname_to_create);
	uart_send_string("' in parent '");
	uart_send_string(((tmpfs_inode_t*)parent_dir_vnode->internal)->name);
	uart_send_string("'\r\n");
	int ret = parent_dir_vnode->v_ops->mkdir(parent_dir_vnode, &new_dir_vnode, dirname_to_create);
	uart_send_string("[vfs_mkdir] Mount point vnode: '");
	uart_send_hex((uint64_t)new_dir_vnode);
	uart_send_string("'\r\n");
	parent_dir_vnode->ref_count--; // 釋放 lookup 得到的 parent ref

	if (ret == E_OK) {
		new_dir_vnode->ref_count--; // mkdir 裡通常會增加 ref_count，這裡操作完成後減掉
											// 因為我們只是建立，不是要持有它。
											// 如果 mkdir 的契約是返回一個帶有 ref_count=1 的 vnode，則不需要減。
											// 這裡假設 mkdir 遵循 create 的模式，返回的 vnode 已被計數。
	}
	return ret;
}
//init_rootfs will not deal with this function, it is called in kernel_init_vfs
int vfs_mount(const char* target_path, const char* fs_name) {
    uart_send_string("[vfs_mount] Attempting to mount ");
    uart_send_string(fs_name);
    uart_send_string(" at ");
    uart_send_string(target_path);
    uart_send_string("\r\n");

    if (!target_path || !fs_name) return E_INVAL;

    thread_t* current_task = get_current(); // 假設的，vfs_mount 可能不總是有任務上下文
                                         // 如果 vfs_mount 只在內核初始化等特定階段被調用，
                                         // 且 target_path 總是絕對路徑，則不需要 current_task->cwd
                                         // 但查找 host_vnode 的父節點時，還是需要一個 "root_node" 參考點
    if (!current_task && strcmp(target_path, "/") != 0) { // 掛載根目錄時可能沒有 current_task
        uart_send_string("Error: [vfs_mount] No current task for non-root mount path resolution.\r\n");
        // return -E_PERM; // 如果 mount 可以由 user space 觸發，則需要 task
    }


    // 1. 查找掛載點 (host_vnode)
    struct vnode* host_vnode = NULL;
    // vfs_lookup 應該使用 vfs_resolve_path，它需要 cwd 和 root_dir
    // 如果是在內核初始化時掛載 rootfs，vfs_lookup 可能有特殊處理或直接調用 setup_mount
    // 這裡我們假設 vfs_lookup 能正確工作
    int ret = vfs_lookup(target_path, &host_vnode,1); // vfs_lookup 內部會調用 vfs_resolve_path
    if (ret != E_OK) {
        uart_send_string("[vfs_mount] Target path lookup failed for '");
        uart_send_string(target_path); uart_send_string("': ");
        uart_send_int(ret);
        uart_send_string("\r\n");
        return ret;
    }

    if (host_vnode->type != VNODE_DIR) {
        uart_send_string("[vfs_mount] Target path '");
        uart_send_string(target_path); uart_send_string("' is not a directory.\r\n");
        host_vnode->ref_count--;
        return E_NOTDIR;
    }

    for (int i = 0; i < num_mounted_fs; ++i) {
        if (mounted_fs_list[i]->mount_point_vnode == host_vnode) { // 比較 mount_point_vnode
            uart_send_string("[vfs_mount] A filesystem is already mounted at '");
            uart_send_string(target_path); uart_send_string("'.\r\n");
            host_vnode->ref_count--;
            return E_BUSY;
        }
    }
	// 2. find filesystem type
    struct filesystem* fs_type = find_filesystem(fs_name);
    if (!fs_type) {
        uart_send_string("[vfs_mount] Filesystem type '");
        uart_send_string(fs_name);
        uart_send_string("' not found.\r\n");
        host_vnode->ref_count--;
        return E_NOENT;
    }
	//3. allocate new mount structure
    struct mount* new_mount = (struct mount*)dynamic_malloc(sizeof(struct mount));
    if (!new_mount) {
        uart_send_string("[vfs_mount] Failed to allocate memory for new_mount.\r\n");
        host_vnode->ref_count--;
        return E_NOMEM;
    }
    memset(new_mount, 0, sizeof(struct mount));

    new_mount->mount_point_vnode = host_vnode; // host_vnode 的引用被 new_mount 持有
    new_mount->fs = fs_type;


    // 4. 查找 host_vnode (掛載點) 的父節點
    struct vnode* mount_point_parent_vnode = NULL;
    if (host_vnode == rootfs->root) { // 如果掛載點是 VFS 的根
        // VFS 根的 ".." 通常是它自己，所以其父節點可以認為是它自己或 NULL
        // 傳遞 NULL 或 host_vnode 本身給 setup_mount，由它決定如何處理
        mount_point_parent_vnode = host_vnode; // 或者 NULL，取決於 setup_mount 的期望
        mount_point_parent_vnode->ref_count++; // 如果不是 NULL，則增加引用
        uart_send_string("[vfs_mount] Mount point is VFS root. Parent considered itself (or NULL).\r\n");
    } else {
        // 檢查 host_vnode 是否有 lookup_parent 操作
        if (!host_vnode->v_ops || !host_vnode->v_ops->lookup_parent) {
            uart_send_string("Error: [vfs_mount] host_vnode does not support lookup_parent.\r\n");
            dynamic_free(new_mount);
            host_vnode->ref_count--; // 釋放 host_vnode 的引用
            return E_INVAL;
        }
        // 呼叫 host_vnode 所屬檔案系統的 lookup_parent
        // task_root_node 參數：對於 lookup_parent，它通常用於 chroot 邊界檢查。
        // 在 mount 的上下文中，我們關心的是 host_vnode 在其 *自身檔案系統* 中的父節點。
        // rootfs->root 可以作為一個安全的 "邊界" 參考。
        ret = host_vnode->v_ops->lookup_parent(host_vnode, &mount_point_parent_vnode, rootfs->root);
        if (ret != E_OK) {
            uart_send_string("[vfs_mount] Failed to find parent of mount point '");
            uart_send_string(target_path); uart_send_string("': ");
            uart_send_int(ret); uart_send_string("\r\n");
            dynamic_free(new_mount);
            host_vnode->ref_count--; // 釋放 host_vnode 的引用
            return ret;
        }
        // lookup_parent 成功，mount_point_parent_vnode 的 ref_count 已被增加
        uart_send_string("[vfs_mount] Parent of mount point '");
        if (host_vnode->internal) uart_send_string(((tmpfs_inode_t*)host_vnode->internal)->name);
        uart_send_string("' is '");
        if (mount_point_parent_vnode && mount_point_parent_vnode->internal) uart_send_string(((tmpfs_inode_t*)mount_point_parent_vnode->internal)->name);
        else uart_send_string("Error:[vfs_mount] UNKNOWN_PARENT_NAME");
        uart_send_string("'.\r\n");
    }

    // 5. 呼叫檔案系統的 setup_mount，傳入 mount_point_parent_vnode
    // 確保 fs_type->setup_mount 的簽名已更新
    ret = fs_type->setup_mount(fs_type, new_mount, mount_point_parent_vnode);

    // setup_mount 使用完 mount_point_parent_vnode 後，我們需要釋放由 lookup_parent 增加的引用
    if (mount_point_parent_vnode) {
        mount_point_parent_vnode->ref_count--;
    }

    if (ret != E_OK) {
        uart_send_string("[vfs_mount] Filesystem setup_mount failed: ");
        uart_send_int(ret); uart_send_string(".\r\n");
        dynamic_free(new_mount);
        host_vnode->ref_count--; // 釋放 host_vnode 的引用
        return ret;
    }

    // 6. 將新掛載加入 VFS 的全局掛載列表
    if (num_mounted_fs >= MAX_MOUNTED_FS) {
        uart_send_string("[vfs_mount] Mounted filesystem list full.\r\n");
        // 清理: guest root (new_mount->root), new_mount 結構
        if (new_mount->root) {
            if(new_mount->root->internal) dynamic_free(new_mount->root->internal);
            dynamic_free(new_mount->root);
        }
        dynamic_free(new_mount);
        host_vnode->ref_count--; // 釋放 host_vnode 的引用
        return E_NOMEM;
    }
    mounted_fs_list[num_mounted_fs++] = new_mount;

    uart_send_string("[vfs_mount] Successfully mounted '");
    uart_send_string(fs_name); uart_send_string("' at '"); uart_send_string(target_path);
    uart_send_string("'. \r\nmount_point_vnode: '");
    if(new_mount->mount_point_vnode->internal) uart_send_string(((tmpfs_inode_t*)new_mount->mount_point_vnode->internal)->name);
    uart_send_string("', guest_root: '");
    if(new_mount->root->internal) uart_send_string(((tmpfs_inode_t*)new_mount->root->internal)->name);
    uart_send_string("'.\r\n");

    // host_vnode 的引用已被 new_mount->mount_point_vnode 持有，所以這裡不減少。
    // (除非 unmount 時才減少)

    return E_OK;
}

void test_vfs_operations() {
    uart_send_string("\n--- Testing VFS Operations ---\r\n");
    struct file* myfile = NULL;
    char buffer[100];
    int bytes_written, bytes_read;

    // --- Basic Exercise 1 Tests (原有的) ---
    // 1. 建立目錄 /mydir
    int mkdir_ret = vfs_mkdir("/mydir");
    uart_send_string("[Test B1] vfs_mkdir(\"/mydir\") returned ");
    uart_send_int(mkdir_ret);
    uart_send_string("\r\n");
    if (mkdir_ret != E_OK) {
        uart_send_string("Failed to create /mydir. Exiting test.\r\n");
        return;
    }

    // 2. 在 /mydir 下建立檔案 /mydir/test.txt
    int open_ret = vfs_open("/mydir/test.txt", O_CREAT | O_RDWR, &myfile);
    uart_send_string("[Test B1] vfs_open(\"/mydir/test.txt\", O_CREAT | O_RDWR) returned ");
    uart_send_int(open_ret);
    uart_send_string("\r\n");
    if (open_ret != E_OK || !myfile) {
        uart_send_string("Failed to open/create /mydir/test.txt. Exiting test.\r\n");
        return;
    }

    // 3. 寫入檔案
    const char* content_to_write = "Hello VFS from tmpfs!";
    size_t content_len = strlen(content_to_write);
    bytes_written = vfs_write(myfile, content_to_write, content_len);
    uart_send_string("[Test B1] vfs_write returned ");
    uart_send_int(bytes_written);
    uart_send_string(" (expected ");
    uart_send_int((int)content_len);
    uart_send_string(")\r\n");
    if (bytes_written <= 0) {
        uart_send_string("Failed to write to file. Closing file and exiting test.\r\n");
        vfs_close(myfile);
        return;
    }

    // 4. 關閉並重新開啟檔案 (測試 seek 到開頭)
    vfs_close(myfile);
    myfile = NULL;
    open_ret = vfs_open("/mydir/test.txt", O_RDWR, &myfile);
    uart_send_string("[Test B1] vfs_open(\"/mydir/test.txt\", O_RDWR) (re-open) returned ");
    uart_send_int(open_ret);
    uart_send_string("\r\n");
    if (open_ret != E_OK || !myfile) {
        uart_send_string("Failed to re-open /mydir/test.txt. Exiting test.\r\n");
        return;
    }
    
    // 5. 讀取檔案
    memset(buffer, 0, sizeof(buffer));
    bytes_read = vfs_read(myfile, buffer, sizeof(buffer) - 1);
    uart_send_string("[Test B1] vfs_read returned ");
    uart_send_int(bytes_read);
    uart_send_string("\r\n");
    if (bytes_read > 0) {
        uart_send_string("[Test B1] Content read: \"");
        uart_send_string(buffer);
        uart_send_string("\"\r\n");
    }

    // 6. 關閉檔案
    vfs_close(myfile);

    // 測試 lookup 一個不存在的檔案
    struct vnode* non_existent_vnode = NULL;
    int lookup_ret = vfs_lookup("/mydir/nosuchfile.txt", &non_existent_vnode,0);
    uart_send_string("[Test B1] vfs_lookup for non-existent file returned ");
    uart_send_int(lookup_ret);
    uart_send_string(" (expected E_NOENT = ");
    uart_send_int(E_NOENT);
    uart_send_string(")\r\n");
    if (non_existent_vnode) {
        uart_send_string("Warning: non_existent_vnode was not NULL after lookup failure.\r\n");
        non_existent_vnode->ref_count--; 
    }

    // 測試 lookup 已存在的檔案
    struct vnode* existent_vnode = NULL;
    lookup_ret = vfs_lookup("/mydir/test.txt", &existent_vnode,0);
    uart_send_string("[Test B1] vfs_lookup for existent file returned ");
    uart_send_int(lookup_ret);
    uart_send_string(" (expected E_OK = ");
    uart_send_int(E_OK);
    uart_send_string(")\r\n");
    if (existent_vnode) {
        uart_send_string("[Test B1] Found vnode name: ");
        uart_send_string(((tmpfs_inode_t*)existent_vnode->internal)->name);
        uart_send_string("\r\n");
        existent_vnode->ref_count--; // 釋放 lookup 增加的引用
    }
    uart_send_string("\n--- Basic Exercise 1 Tests Finished ---\r\n");


    // --- Basic Exercise 2 Tests ---
    uart_send_string("\n--- Starting Basic Exercise 2 Tests ---\r\n");

    // 1. 建立多層子目錄
    uart_send_string("\n--- Test B2: Create nested directories ---\r\n");
    mkdir_ret = vfs_mkdir("/foo");
    uart_send_string("[Test B2] vfs_mkdir(\"/foo\") returned ");
    uart_send_int(mkdir_ret);
    uart_send_string("\r\n");
    
    mkdir_ret = vfs_mkdir("/foo/bar");
    uart_send_string("[Test B2] vfs_mkdir(\"/foo/bar\") returned ");
    uart_send_int(mkdir_ret);
    uart_send_string("\r\n");

    mkdir_ret = vfs_mkdir("/foo/bar/baz");
    uart_send_string("[Test B2] vfs_mkdir(\"/foo/bar/baz\") returned ");
    uart_send_int(mkdir_ret);
    uart_send_string("\r\n");

    // 2. 測試在多層目錄下創建檔案
    uart_send_string("\n--- Test B2: Create file in nested directory ---\r\n");
    myfile = NULL;
    open_ret = vfs_open("/foo/bar/file_in_baz.txt", O_CREAT | O_RDWR, &myfile);
    uart_send_string("[Test B2] vfs_open(\"/foo/bar/file_in_baz.txt\") returned ");
    uart_send_int(open_ret);
    uart_send_string("\r\n");
    if (open_ret == E_OK && myfile) {
        const char* nested_content = "Content in nested file!";
        bytes_written = vfs_write(myfile, nested_content, strlen(nested_content));
        uart_send_string("[Test B2] Wrote ");
        uart_send_int(bytes_written);
        uart_send_string(" bytes to /foo/bar/file_in_baz.txt\r\n");
        vfs_close(myfile);
    } else {
        uart_send_string("[Test B2] Failed to create file in nested directory.\r\n");
    }

    // 3. 測試掛載檔案系統 (需要 initramfs_filesystem)
    uart_send_string("\n--- Test B2: Mounting a new filesystem ---\r\n");
    // 假設 initramfs_init() 已經在 kernel_init_vfs() 裡被呼叫並註冊了 initramfs
    // 如果沒有，你需要在 kernel_init_vfs 或這裡呼叫 initramfs_init()
    // initramfs_init(); // 如果 initramfs 需要單獨初始化和註冊

    // 創建一個掛載點目錄 (在tmpfs中)
    mkdir_ret = vfs_mkdir("/initramfs");
    uart_send_string("[Test B2] vfs_mkdir(\"/initramfs\") returned ");
    uart_send_int(mkdir_ret);
    uart_send_string("\r\n");

    // 掛載 initramfs 到 /initramfs
    // 請確認你的 initramfs_filesystem 存在並已註冊
    int mount_ret = vfs_mount("/initramfs", "tmpfs"); // 使用 tmpfs 模擬 initramfs
    // mount_ret = vfs_mount("/initramfs", "initramfs"); // 實際使用 initramfs
    uart_send_string("[Test B2] vfs_mount(\"/initramfs\", \"tmpfs\") returned ");
    uart_send_int(mount_ret);
    uart_send_string("\r\n");
    if (mount_ret != E_OK) {
        uart_send_string("[Test B2] Failed to mount /initramfs. Skipping cross-mount tests.\r\n");
    } else {
        // 4. 測試跨掛載點查找 (現在 /initramfs 下的內容應該來自被掛載的 tmpfs/initramfs)
        uart_send_string("\n--- Test B2: Cross-mount lookup & operations ---\r\n");
        // 在被掛載的 initramfs 裡創建一個檔案
        // 這裡會使用 /initramfs，然後 VFS 會跳轉到 initramfs 的根目錄
        myfile = NULL;
        open_ret = vfs_open("/initramfs/file.txt", O_CREAT | O_RDWR, &myfile);
        uart_send_string("[Test B2] vfs_open(\"/initramfs/file.txt\") returned ");
        uart_send_int(open_ret);
        uart_send_string("\r\n");
        if (open_ret == E_OK && myfile) {
            const char* mount_content = "Content from mounted filesystem!";
            bytes_written = vfs_write(myfile, mount_content, strlen(mount_content));
            uart_send_string("[Test B2] Wrote ");
            uart_send_int(bytes_written);
            uart_send_string(" bytes to /initramfs/file.txt\r\n");
            vfs_close(myfile);

            // 讀取跨掛載點的檔案
            myfile = NULL;
            open_ret = vfs_open("/initramfs/file.txt", O_RDWR, &myfile);
            if (open_ret == E_OK && myfile) {
                memset(buffer, 0, sizeof(buffer));
                bytes_read = vfs_read(myfile, buffer, sizeof(buffer) - 1);
                uart_send_string("[Test B2] Read ");
                uart_send_int(bytes_read);
                uart_send_string(" bytes from /initramfs/file.txt: \"");
                uart_send_string(buffer);
                uart_send_string("\"\r\n");
                vfs_close(myfile);
            } else {
                uart_send_string("[Test B2] Failed to re-open mounted file for reading.\r\n");
            }

        } else {
            uart_send_string("[Test B2] Failed to create file in mounted filesystem.\r\n");
        }
    }

    uart_send_string("\n--- Basic Exercise 2 Tests Finished ---\r\n");
    uart_send_string("\n--- End VFS Test ---\r\n");
}
