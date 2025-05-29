// vfs.c
#include "vfs.h"
#include "tmpfs.h" // 包含 tmpfs 的定義和函式
#include "uart.h" // 包含 UART 函式 (例如 uart_send_string)
#include "buddy_alloc.h" // 包含動態記憶體分配函式 (例如 dynamic_malloc, dynamic_free)
#include <stdio.h> 	// For printf, remove for kernel
#include <string.h> // For strcmp, strcpy, strchr

// 假設一個簡單的已註冊檔案系統列表 (實際應用中可能需要更動態的結構)
#define MAX_REGISTERED_FS 4
static struct filesystem* registered_fs_list[MAX_REGISTERED_FS];
static int num_registered_fs = 0;

// 全域的根檔案系統掛載點 (在某處定義並初始化)
struct mount* rootfs = NULL;

// 每個行程的檔案描述符表 (File Descriptor Table) 和目前工作目錄 (CWD)
// 這是 Basic Exercise 3 的內容，這裡先簡單示意，Basic 1 的重點是 rootfs
#define MAX_OPEN_FILES_PER_PROCESS 16 // (對應 fd < 16)
#define MAX_PATHNAME_LEN 255 //

// 簡化版：全域只有一個檔案描述符表 (真正的系統中每個 process 有自己的)
static struct file* global_fd_table[MAX_OPEN_FILES_PER_PROCESS];
static int next_fd = 0; // 非常簡化的 fd 分配
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
	int ret = fs_type_to_mount->setup_mount(fs_type_to_mount, rootfs);
	if (ret != E_OK) {
		uart_send_string("CRITICAL:[kernel_init_vfs] Failed to setup_mount for rootfs!\r\n");
		dynamic_free(rootfs); // 清理
		rootfs = NULL;
		// Kernel panic or halt
		return;
	}

	uart_send_string("Rootfs (tmpfs) mounted successfully. Root vnode internal name: '");
	uart_send_string(((tmpfs_inode_t*)rootfs->root->internal)->name);
	uart_send_string("'\r\n"); // 假設 tmpfs internal 結構和名稱
}
// --- 檔案系統註冊 ---
int register_filesystem(struct filesystem* fs) {
	if (num_registered_fs < MAX_REGISTERED_FS) {
		for (int i = 0; i < num_registered_fs; ++i) {
			if (strcmp(registered_fs_list[i]->name, fs->name) == 0) {
				uart_send_string("VFS: Filesystem ");
				uart_send_string(fs->name);
				uart_send_string(" already registered.\r\n");
				return E_EXIST; // Already registered
			}
		}
		registered_fs_list[num_registered_fs++] = fs;
		uart_send_string("VFS: Filesystem '");
		uart_send_string(fs->name);
		uart_send_string("' registered.\r\n");
		return E_OK;
	}
	uart_send_string("VFS: Cannot register filesystem '");
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
// vfs_lookup: 解析完整路徑名稱，找到目標 vnode
// pathname: 絕對路徑，例如 "/foo/bar" 或 "/"
// target: (輸出參數) 指向找到的 vnode
//
// 簡化版 lookup:
// - 只支援絕對路徑
// - 遇到掛載點會切換 (Basic Exercise 2 的內容，但對根目錄查找有用)
// - component_name 長度等限制由底層 fs (如 tmpfs) 處理
int vfs_lookup(const char* pathname, struct vnode** target) {
	if (!pathname || !target) return E_INVAL;
	if (!rootfs || !rootfs->root) {
		uart_send_string("vfs_lookup: Rootfs not mounted!\r\n");
		return E_NOENT; //或者 E_AGAIN / E_BUSY
	}

	uart_send_string("vfs_lookup: Looking for path '\r\n");
	uart_send_string(pathname);
	uart_send_string("'\r\n");

	if (strcmp(pathname, "/") == 0) {
		*target = rootfs->root;
		(*target)->ref_count++;
		return E_OK;
	}

	struct vnode* current_vnode = rootfs->root;
	current_vnode->ref_count++; // Start with a reference to root

	char path_copy[MAX_PATHNAME_LEN + 1];
	strcpy(path_copy, pathname);
	path_copy[MAX_PATHNAME_LEN] = '\0';

	char* next_component = path_copy;
	if (*next_component == '/') { // 跳過開頭的 '/'
		next_component++;
	}

	char* end_of_path = next_component + strlen(next_component);

	while (next_component < end_of_path && *next_component != '\0') {
		char* current_component_name = next_component;
		char* separator = strchr(next_component, '/');
		
		if (separator) {
			*separator = '\0'; // 暫時切斷字串以取得當前 component name
			next_component = separator + 1;
		} else {
			next_component = end_of_path; // 最後一個 component
		}

		if (strlen(current_component_name) == 0) { // 處理像 "//" 這樣的情況
			if (separator) continue; // 如果是路徑中間的 // 就跳過
			else break; // 如果是路徑結尾的 /，則查找結束
		}

		uart_send_string("vfs_lookup: current_vnode is '");
		uart_send_string(((tmpfs_inode_t*)current_vnode->internal)->name);
		uart_send_string("', looking for component '");
		uart_send_string(current_component_name);
		uart_send_string("'\r\n"); // 假設是 tmpfs，僅為 debug

		if (!current_vnode->v_ops || !current_vnode->v_ops->lookup) {
			current_vnode->ref_count--;
			return E_INVAL; // No lookup operation
		}

		struct vnode* found_vnode = NULL;
		int ret = current_vnode->v_ops->lookup(current_vnode, &found_vnode, current_component_name);
		
		current_vnode->ref_count--; // Release previous current_vnode reference

		if (ret != E_OK) {
			uart_send_string("vfs_lookup: component '");
			uart_send_string(current_component_name);
			uart_send_string("' not found or error ");
			uart_send_int(ret);
			uart_send_string("\r\n");
			return ret; // Not found or other error
		}
		current_vnode = found_vnode; // found_vnode 已經被 lookup 增加了引用計數

		// 檢查是否為掛載點 (Basic Exercise 2 的內容，但 lookup 需要這個邏輯)
		// 這裡用一個簡化的檢查：如果 current_vnode->mount 不是 NULL 且和父節點的 mount 不同
		// 或者，更準確地說，檢查 VFS 是否有一個 mount 結構其 host_vnode 是 current_vnode
		// 實際上，我們需要一個全域的掛載列表來查詢。
		// 為了簡化 Basic 1，假設只有 rootfs，不處理非根的掛載點跳轉。
		// 完整的掛載點跳轉邏輯會遍歷一個全域的 _mounts 列表，
		// 檢查是否有 mount->host_vnode == current_vnode，如果是，則 current_vnode = mount->guest_vnode (即掛載檔案系統的根)。
		// current_vnode = get_mounted_vnode_or_self(current_vnode);
	}

	*target = current_vnode; // current_vnode 的引用計數是正確的
	uart_send_string("vfs_lookup: Path '");
	uart_send_string(pathname);
	uart_send_string("' resolved to vnode '");
	uart_send_string(((tmpfs_inode_t*)(*target)->internal)->name);
	uart_send_string("'\r\n");
	return E_OK;
}


// --- VFS API 實作 ---
int vfs_open(const char* pathname, int flags, struct file** target_file) {
	if (!pathname || !target_file) return E_INVAL;
	uart_send_string("vfs_open: path '");
	uart_send_string(pathname);
	uart_send_string("', flags ");
	uart_send_int(flags);
	uart_send_string("\r\n");

	struct vnode* node_to_open = NULL;
	int ret = vfs_lookup(pathname, &node_to_open);

	if (ret != E_OK) {
		if (ret == E_NOENT && (flags & O_CREAT)) { // O_CREAT 旗標在 vfs.h 中未定義，需要自行定義
			// 檔案不存在，但有 O_CREAT 旗標，嘗試建立檔案
			// 1. 找到父目錄和檔名
			char path_copy[MAX_PATHNAME_LEN + 1];
			strcpy(path_copy, pathname);
			path_copy[MAX_PATHNAME_LEN] = '\0';

			char* last_slash = strrchr(path_copy, '/');
			struct vnode* parent_dir_vnode = NULL;
			const char* filename = NULL;

			if (last_slash) {
				if (last_slash == path_copy) { // e.g., "/file"
					filename = last_slash + 1;
					*(last_slash+1) = '\0'; // path_copy will be "/"
					int lookup_ret = vfs_lookup("/", &parent_dir_vnode);
					if(lookup_ret != E_OK) {
						uart_send_string("vfs_open (O_CREAT): failed to lookup parent '/'\r\n");
						return lookup_ret;
					}

				} else { // e.g., "/dir/file"
					filename = last_slash + 1;
					*last_slash = '\0'; // path_copy is now parent dir path
					int lookup_ret = vfs_lookup(path_copy, &parent_dir_vnode);
					if(lookup_ret != E_OK) {
						uart_send_string("vfs_open (O_CREAT): failed to lookup parent '\r\n");
						uart_send_string(path_copy);
						uart_send_string("'\r\n");
						return lookup_ret;
					}
				}
			} else { // e.g., "file" (相對路徑，Basic 1 假設都是絕對路徑)
				uart_send_string("vfs_open (O_CREAT): relative path creation not supported in this example.\r\n");
				return E_INVAL;
			}
			
			if (strlen(filename) == 0) { // e.g. path ends with /
				if(parent_dir_vnode) parent_dir_vnode->ref_count--;
				return E_ISDIR; // Cannot create a file with empty name or named "/"
			}

			if (!parent_dir_vnode->v_ops || !parent_dir_vnode->v_ops->create) {
				parent_dir_vnode->ref_count--;
				return E_INVAL; // Parent doesn't support create
			}

			uart_send_string("vfs_open (O_CREAT): creating '");
			uart_send_string(filename);
			uart_send_string("' in dir '");
			uart_send_string(((tmpfs_inode_t*)parent_dir_vnode->internal)->name);
			uart_send_string("'\r\n");
			ret = parent_dir_vnode->v_ops->create(parent_dir_vnode, &node_to_open, filename);
			parent_dir_vnode->ref_count--; // Release ref from lookup

			if (ret != E_OK) {
				uart_send_string("vfs_open (O_CREAT): create failed with ");
				uart_send_int(ret);
				uart_send_string("\r\n");
				return ret;
			}
			// node_to_open 的 ref_count 已經被 create 設定
		} else {
			// 查找失敗且沒有 O_CREAT，或 O_CREAT 但其他錯誤
			return ret;
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

	// (為 Basic 3 準備) 分配一個 fd
	// 這裡用一個非常簡化的全域 fd 表
	int fd = -1;
	for(int i=0; i < MAX_OPEN_FILES_PER_PROCESS; ++i) {
		if(global_fd_table[i] == NULL) {
			global_fd_table[i] = new_file;
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
	uart_send_string("vfs_open: success for path '");
	uart_send_string(pathname);
	uart_send_string("', assigned fd ");
	uart_send_int(fd);
	uart_send_string(" (conceptually)\r\n");
	return E_OK;
}

int vfs_close(struct file* file_to_close) {
	if (!file_to_close) return E_INVAL;

	uart_send_string("vfs_close: closing file for vnode '");
	uart_send_string(((tmpfs_inode_t*)file_to_close->vnode->internal)->name);
	uart_send_string("'\r\n");

	// (為 Basic 3 準備) 從 fd 表中移除
	for(int i=0; i < MAX_OPEN_FILES_PER_PROCESS; ++i) {
		if(global_fd_table[i] == file_to_close) {
			global_fd_table[i] = NULL;
			break;
		}
	}

	if (file_to_close->f_ops && file_to_close->f_ops->close) {
		file_to_close->f_ops->close(file_to_close);
	}

	file_to_close->vnode->ref_count--; // 釋放對 vnode 的引用
	if (file_to_close->vnode->ref_count == 0) {
		// TODO: 如果 vnode 的引用計數為0，並且沒有被其他 file handle 引用，
		// 且檔案已被 unlink，則可以考慮釋放 vnode 和其 internal node 的資源。
		// 這部分比較複雜，涉及到 unlink 和快取。Basic 1 可先忽略。
		uart_send_string("vfs_close: vnode '");
		uart_send_string(((tmpfs_inode_t*)file_to_close->vnode->internal)->name);
		uart_send_string("' ref_count is now 0.\r\n");
		// dynamic_free(file_to_close->vnode->internal); // 需小心，如果 internal 被共享
		// dynamic_free(file_to_close->vnode);
	}

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

	uart_send_string("vfs_write: for vnode '");
	uart_send_string(((tmpfs_inode_t*)file_to_write->vnode->internal)->name);
	uart_send_string("', len ");
	uart_send_int(len);
	uart_send_string("\r\n");
	return file_to_write->f_ops->write(file_to_write, buf, len);
}

int vfs_mkdir(const char* pathname) {
	if (!pathname) return E_INVAL;
	uart_send_string("vfs_mkdir: path '");
	uart_send_string(pathname);
	uart_send_string("'\r\n");

	// 1. 找到父目錄和要建立的目錄名
	char path_copy[MAX_PATHNAME_LEN + 1];
	strcpy(path_copy, pathname);
	path_copy[MAX_PATHNAME_LEN] = '\0';

	char* last_slash = strrchr(path_copy, '/');
	struct vnode* parent_dir_vnode = NULL;
	const char* dirname_to_create = NULL;

	if (last_slash) {
		if (last_slash == path_copy && *(last_slash+1) != '\0') { // e.g., "/newdir"
			dirname_to_create = last_slash + 1;
			int lookup_ret = vfs_lookup("/", &parent_dir_vnode);
			if(lookup_ret != E_OK) return lookup_ret;
		} else if (last_slash != path_copy) { // e.g., "/existing_dir/newdir"
			dirname_to_create = last_slash + 1;
			*last_slash = '\0'; // path_copy is now parent dir path
			int lookup_ret = vfs_lookup(path_copy, &parent_dir_vnode);
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
	uart_send_string("vfs_mkdir: creating dir '");
	uart_send_string(dirname_to_create);
	uart_send_string("' in parent '");
	uart_send_string(((tmpfs_inode_t*)parent_dir_vnode->internal)->name);
	uart_send_string("'\r\n");
	int ret = parent_dir_vnode->v_ops->mkdir(parent_dir_vnode, &new_dir_vnode, dirname_to_create);
	
	parent_dir_vnode->ref_count--; // 釋放 lookup 得到的 parent ref

	if (ret == E_OK) {
		new_dir_vnode->ref_count--; // mkdir 裡通常會增加 ref_count，這裡操作完成後減掉
											// 因為我們只是建立，不是要持有它。
											// 如果 mkdir 的契約是返回一個帶有 ref_count=1 的 vnode，則不需要減。
											// 這裡假設 mkdir 遵循 create 的模式，返回的 vnode 已被計數。
	}
	return ret;
}

// vfs_mount (Basic 1 的根檔案系統掛載通常直接呼叫 setup_mount，這個是用來掛載後續檔案系統的)
int vfs_mount(const char* target_path, const char* fs_name) {
	// 1. 查找 target_path 的 vnode (掛載點)
	// 2. 查找名為 fs_name 的 filesystem 結構
	// 3. 建立一個新的 mount 結構
	// 4. 呼叫 filesystem->setup_mount(fs, new_mount_struct)
	// 5. 將 new_mount_struct 加入到全域的掛載列表
	// 這部分涉及到 Basic Exercise 2 的內容，Basic 1 的 rootfs 掛載更直接。
	uart_send_string("vfs_mount: STUB for mounting ");
	uart_send_string(fs_name);
	uart_send_string(" at ");
	uart_send_string(target_path);
	uart_send_string("\r\n");
	return E_OK;
}