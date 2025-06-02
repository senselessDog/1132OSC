// tmpfs.c
#include "tmpfs.h"
#include "vfs.h" // 包含 vfs 結構定義和 API
#include "buddy_alloc.h"
#include "uart.h" // 包含 UART 函式 (例如 uart_send_string, uart_send_int)
#include <string.h> // For strcpy, strcmp, strlen, memcpy, memset
#include <stdio.h> 	// For printf in debugging (remove for kernel)

// -------------------- Helper Functions for tmpfs_inode_t --------------------
// (這些是示意，你需要一個記憶體分配器，例如 kmalloc/kfree)
tmpfs_inode_t* create_tmpfs_internal_node(const char* name, enum TMPFS_TYPE type, struct vnode* v_node_ptr) {
	tmpfs_inode_t* node = (tmpfs_inode_t*)dynamic_malloc(sizeof(tmpfs_inode_t)); // 應用你的記憶體分配器
	if (!node) return NULL;

	memset(node, 0, sizeof(tmpfs_inode_t));
	strcpy(node->name, name);
	node->name[TMPFS_MAX_NAME_LEN] = '\0'; // 確保 null-terminated
	node->type = type;
	node->v_node = v_node_ptr; // 讓 internal node 能找到它的 vnode

	if (type == TMPFS_DIR) {
		node->num_children = 0;
	} else {
		node->size = 0;
	}
	return node;
}

struct vnode* create_tmpfs_vnode(struct mount* mount_info, tmpfs_inode_t* internal_node, enum VNODE_TYPE v_type) {
	struct vnode* vn = (struct vnode*)dynamic_malloc(sizeof(struct vnode)); // 應用你的記憶體分配器
	if (!vn) return NULL;

	vn->mount = mount_info;
	vn->f_ops = &tmpfs_file_ops;
	vn->v_ops = &tmpfs_vnode_ops;
	vn->internal = internal_node;
	vn->ref_count = 1; // 初始引用計數為1
	vn->type = v_type;
	
	if (internal_node) { // 如果 internal_node 已經被創建
		internal_node->v_node = vn; // 互相指
	}
	return vn;
}


// -------------------- tmpfs file_operations --------------------
int tmpfs_write(struct file* file, const void* buf, size_t len) {
	if (!file || !file->vnode || !file->vnode->internal) return E_INVAL;
	tmpfs_inode_t* node = (tmpfs_inode_t*)file->vnode->internal;

	if (node->type != TMPFS_FILE) return E_ISDIR; // 不能對目錄寫

	// 檢查是否超出檔案大小限制
	if (file->f_pos + len > TMPFS_MAX_FILE_SIZE) {
		len = TMPFS_MAX_FILE_SIZE - file->f_pos; // 截斷寫入長度
		if (len == 0 && file->f_pos == TMPFS_MAX_FILE_SIZE) return E_FBIG; // 已經到最大了，寫不進去
	}
	if (len == 0) return 0; // 沒有東西可以寫了

	memcpy(node->content + file->f_pos, buf, len);
	file->f_pos += len;
	if (file->f_pos > node->size) {
		node->size = file->f_pos; // 更新檔案大小
	}
	uart_send_string("[tmpfs_write] wrote ");
	uart_send_int(len);
	uart_send_string(" bytes to '");
	uart_send_string(node->name);
	uart_send_string("', new size ");
	uart_send_int(node->size);
	uart_send_string(", f_pos ");
	uart_send_int(file->f_pos);
	uart_send_string("\r\n");
	return len;
}

int tmpfs_read(struct file* file, void* buf, size_t len) {
	if (!file || !file->vnode || !file->vnode->internal) return E_INVAL;
	tmpfs_inode_t* node = (tmpfs_inode_t*)file->vnode->internal;

	if (node->type != TMPFS_FILE) return E_ISDIR; // 不能從目錄讀

	// 檢查是否讀取超出檔案內容
	if (file->f_pos >= node->size) return 0; // EOF

	size_t readable_len = node->size - file->f_pos;
	if (len > readable_len) {
		len = readable_len;
	}

	memcpy(buf, node->content + file->f_pos, len);
	file->f_pos += len;
	uart_send_string("tmpfs_read: read ");
	uart_send_int(len);
	uart_send_string(" bytes from '");
	uart_send_string(node->name);
	uart_send_string("', f_pos ");
	uart_send_int(file->f_pos);
	uart_send_string("\r\n");
	return len;
}

// tmpfs_open 通常不需要做太多事，因為 VFS 層已經處理了 file handle 的建立
// 這裡可以返回成功，或者如果需要特定的 tmpfs 初始化，可以在這裡做
int tmpfs_open(struct vnode* file_node, struct file** target) {
	// VFS 層會建立 file handle，並將 file_node 和 f_ops (來自 vnode) 填入
	// file handle 的 f_pos 會被設為0，flags 來自 vfs_open 的參數
	// uart_send_string("tmpfs_open: called for vnode of '"); uart_send_string(((tmpfs_inode_t*)file_node->internal)->name); uart_send_string("'\r\n");
	return E_OK; // 成功
}

// tmpfs_close 通常也不需要做太多事，VFS 會釋放 file handle
int tmpfs_close(struct file* file) {
	// 如果 open 時分配了資源，可以在這裡釋放
	// uart_send_string("tmpfs_close: called for file pointing to vnode '"); uart_send_string(((tmpfs_inode_t*)file->vnode->internal)->name); uart_send_string("'\r\n");
	return E_OK; // 成功
}

struct file_operations tmpfs_file_ops = {
	.write = tmpfs_write,
	.read = tmpfs_read,
	.open = tmpfs_open,
	.close = tmpfs_close,
};

// -------------------- tmpfs vnode_operations --------------------
int tmpfs_lookup(struct vnode* dir_node, struct vnode** target, const char* component_name) {
	if (!dir_node || !dir_node->internal || !target || !component_name) return E_INVAL;
	tmpfs_inode_t* parent_internal = (tmpfs_inode_t*)dir_node->internal;

	if (parent_internal->type != TMPFS_DIR) return E_NOTDIR;

	for (int i = 0; i < parent_internal->num_children; ++i) {
		tmpfs_inode_t* child_internal = (tmpfs_inode_t*)parent_internal->children[i]->internal;
		if (strcmp(child_internal->name, component_name) == 0) {
			*target = parent_internal->children[i];
			(*target)->ref_count++; // 增加引用計數
			uart_send_string("[tmpfs_lookup] found '");
			uart_send_string(component_name);
			uart_send_string("' in '");
			uart_send_string(parent_internal->name);
			uart_send_string("'\r\n");
			return E_OK;
		}
	}
	uart_send_string("[tmpfs_lookup] '");
	uart_send_string(component_name);
	uart_send_string("' not found in '");
	uart_send_string(parent_internal->name);
	uart_send_string("'\r\n");
	return E_NOENT; // Not found
}
int tmpfs_lookup_parent(struct vnode* dir_node, struct vnode** target_parent, struct vnode* task_root_node) {
    if (!dir_node || !dir_node->internal || !target_parent || !task_root_node) return E_INVAL;
    tmpfs_inode_t* current_internal = (tmpfs_inode_t*)dir_node->internal;

    // 檢查是否是某個掛載點的根 (guest root)
    // 如果 current_vnode 是某個 mounted_fs_list[i]->root，則 ".." 應該跳出到 mounted_fs_list[i]->mount_point_vnode 的父目錄
    // 這部分的邏輯在 VFS 層處理 ".." 時更合適，底層 FS 的 lookup_parent 應該只返回其 FS 內的父節點
    // 但 "if the root vnode is mounted on a vnode, VFS should go to the mounted vnode" - 這句話針對 lookup，不是 ".."
    // "other wise, “..” behaves like “.”" - 這適用於 FS 根。

    // tmpfs 內部的父節點查找
    if (current_internal->parent_dir_internal != NULL && current_internal->parent_dir_internal->v_node != NULL) {
        *target_parent = current_internal->parent_dir_internal->v_node;
        (*target_parent)->ref_count++;
        uart_send_string("[tmpfs_lookup_parent] Parent of '"); uart_send_string(current_internal->name);
        uart_send_string("' is '"); uart_send_string(current_internal->parent_dir_internal->name); uart_send_string("'\r\n");
        return E_OK;
    } else {
        // 沒有父節點（例如，它是 tmpfs 的根），根據講義，".." 表現得像 "."
        *target_parent = dir_node; // 指回自己
        (*target_parent)->ref_count++;
        uart_send_string("[tmpfs_lookup_parent] '"); uart_send_string(current_internal->name);
        uart_send_string("' has no parent in this tmpfs, '..' is itself.\r\n");
        return E_OK;
    }
    return E_NOENT; // 正常情況下不應該到這裡如果上面邏輯完整
}
static int tmpfs_mknod(struct vnode* dir_node, struct vnode** target, const char* component_name,
	enum VNODE_TYPE type, struct file_operations* dev_fops, struct vnode_operations* dev_vops) {
	uart_send_string("[tmpfs_mknod] Creating device node '"); uart_send_string(component_name);
	uart_send_string("' in dir '"); uart_send_string(((tmpfs_inode_t*)dir_node->internal)->name);
	uart_send_string("'\r\n");

	if (!dir_node || !dir_node->internal || !target || !component_name || !dev_fops) {
	return -E_INVAL; // dev_vops 可以是 NULL，但 dev_fops 必須有
	}
	tmpfs_inode_t* parent_internal = (tmpfs_inode_t*)dir_node->internal;

	if (parent_internal->type != TMPFS_DIR) return -E_NOTDIR;

	// 檢查是否已存在同名檔案/目錄 (與 tmpfs_create_common 類似)
	struct vnode* existing_node = NULL;
	if (tmpfs_lookup(dir_node, &existing_node, component_name) == E_OK) {
		existing_node->ref_count--;
		return -E_EXIST;
	}
	if (parent_internal->num_children >= TMPFS_MAX_DIR_ENTRIES) return -E_NOSPC;
	if (strlen(component_name) > TMPFS_MAX_NAME_LEN) return -E_INVAL; // 或 E_NAMETOOLONG

	// 1. 建立 tmpfs 內部節點
	// 注意：tmpfs_inode_t 本身不儲存 f_ops/v_ops，這些是 vnode 的屬性
	// type 參數來自 mknod 的 type，通常是 VNODE_FILE
	tmpfs_inode_t* new_internal_node = create_tmpfs_internal_node(component_name, (type == VNODE_DIR ? TMPFS_DIR : TMPFS_FILE), NULL);
	if (!new_internal_node) return -E_NOMEM;

	// 如果 tmpfs_inode_t 需要標記它是一個特殊設備，可以在這裡做，但通常 VFS vnode 的 type 和 ops 更重要
	// new_internal_node->is_device = 1; // 例如

	// 2. 建立 vnode
	struct vnode* new_vnode = create_tmpfs_vnode(dir_node->mount, new_internal_node, type);
	if (!new_vnode) {
	dynamic_free(new_internal_node);
	return -E_NOMEM;
	}

	// !!! 關鍵：覆寫 f_ops 和 v_ops !!!
	new_vnode->f_ops = dev_fops;
	if (dev_vops) { // dev_vops 可以是 NULL，表示使用預設的（如果有的話）或不支援
		new_vnode->v_ops = dev_vops;
	} else {
		new_vnode->v_ops = &tmpfs_vnode_ops; // 或一個更合適的 for device files
	}
	// new_vnode->type 已經在 create_tmpfs_vnode 中根據 internal_node->type 設定，
	// 或者我們可以在這裡根據 mknod 傳入的 type 明確設定 vnode->type
	new_vnode->type = type;


	// 3. 將新節點加入父目錄
	parent_internal->children[parent_internal->num_children++] = new_vnode;
	*target = new_vnode; // new_vnode 的 ref_count 在 create_tmpfs_vnode 中已設為 1
	uart_send_string("[tmpfs_mknod] Device node '"); uart_send_string(component_name); uart_send_string("' created.\r\n");
	return E_OK;
}
int tmpfs_create_common(struct vnode* dir_node, struct vnode** target, const char* component_name, enum TMPFS_TYPE type, enum VNODE_TYPE v_type) {
	if (!dir_node || !dir_node->internal || !target || !component_name) return E_INVAL;
	tmpfs_inode_t* parent_internal = (tmpfs_inode_t*)dir_node->internal;

	if (parent_internal->type != TMPFS_DIR) return E_NOTDIR; // 父節點必須是目錄

	// 檢查是否已存在同名檔案/目錄
	struct vnode* existing_node = NULL;
	if (tmpfs_lookup(dir_node, &existing_node, component_name) == E_OK) {
		existing_node->ref_count--; // lookup 增加了引用，這裡減回來
		return E_EXIST;
	}

	// 檢查目錄是否已滿
	if (parent_internal->num_children >= TMPFS_MAX_DIR_ENTRIES) return E_NOSPC; // No space left

	// 檢查名稱長度
	if (strlen(component_name) > TMPFS_MAX_NAME_LEN) return E_INVAL; // Name too long (可以定義 E_NAMETOOLONG)

	// 1. 建立 tmpfs 內部節點
	tmpfs_inode_t* new_internal_node = create_tmpfs_internal_node(component_name, type, NULL); // v_node 稍後填
	if (!new_internal_node) return E_NOMEM;
	//important
	new_internal_node->parent_dir_internal = parent_internal;
	// 2. 建立 vnode
	struct vnode* new_vnode = create_tmpfs_vnode(dir_node->mount, new_internal_node, v_type);
	if (!new_vnode) {
		dynamic_free(new_internal_node); // 清理
		return E_NOMEM;
	}
	// create_tmpfs_vnode 內部已經設定了 new_internal_node->v_node = new_vnode;

	// 3. 將新節點加入父目錄
	parent_internal->children[parent_internal->num_children++] = new_vnode;
	*target = new_vnode;
	uart_send_string("[tmpfs_create_common] created '");
	uart_send_string(component_name);
	uart_send_string("' in '");
	uart_send_string(parent_internal->name);
	uart_send_string("' as type ");
	uart_send_int(type);
	uart_send_string("\r\n");
	return E_OK;
}


int tmpfs_create(struct vnode* dir_node, struct vnode** target, const char* component_name) {
	uart_send_string("tmpfs_create: attempting to create file '");
	uart_send_string(component_name);
	uart_send_string("'\r\n");
	return tmpfs_create_common(dir_node, target, component_name, TMPFS_FILE, VNODE_FILE);
}

int tmpfs_mkdir(struct vnode* dir_node, struct vnode** target, const char* component_name) {
	uart_send_string("[tmpfs_mkdir] attempting to create dir '");
	uart_send_string(component_name);
	uart_send_string("'\r\n");
	return tmpfs_create_common(dir_node, target, component_name, TMPFS_DIR, VNODE_DIR);
}


struct vnode_operations tmpfs_vnode_ops = {
    .lookup = tmpfs_lookup,
    .create = tmpfs_create,
    .mkdir = tmpfs_mkdir,
    .lookup_parent = tmpfs_lookup_parent, // 新增
	.mknod = tmpfs_mknod, // 新增
};

// -------------------- tmpfs filesystem operations --------------------
// 這個函式在掛載 tmpfs 時被 VFS 呼叫
int tmpfs_setup_mount(struct filesystem* fs_info, struct mount* mount_info,struct vnode* logical_parent_of_mount_point) {
	if (!fs_info || !mount_info) return E_INVAL;
	uart_send_string("[tmpfs_setup_mount] Setting up mount for ");
	uart_send_string(fs_info->name);
	uart_send_string("\r\n");

	// 1. 建立 tmpfs 的根目錄的 internal node
	// 	根目錄的名稱通常是 "/"，但在 tmpfs 內部可以自訂，例如就叫 "root_tmpfs_dir"
	// 	VFS 層面看到的 "/" 是透過 mount 結構的 root vnode 來表示的
	tmpfs_inode_t* root_internal_node = create_tmpfs_internal_node("/", TMPFS_DIR, NULL);
	if (!root_internal_node) return E_NOMEM;
	if (first_mount_fs){
		first_mount_fs = 0; // 設定為 0，表示已經有第一個掛載了
		root_internal_node->parent_dir_internal = NULL; // 根目錄沒有父目錄
		uart_send_string("[tmpfs_setup_mount] This is the first mount, setting root internal node parent to NULL.\r\n");
	} else {
		tmpfs_inode_t* logical_parent_internal = (tmpfs_inode_t*)logical_parent_of_mount_point->internal;
		root_internal_node->parent_dir_internal = logical_parent_internal; // 這裡可以指向自己，表示它是根目錄
		uart_send_string("[tmpfs_setup_mount] Not the first mount, setting root internal node parent to\r\n");
		uart_send_string(root_internal_node->parent_dir_internal->name);
		uart_send_string("\r\n");
	}
	// 2. 建立 tmpfs 的根目錄的 vnode
	struct vnode* root_vnode = create_tmpfs_vnode(mount_info, root_internal_node, VNODE_DIR);
	if (!root_vnode) {
		dynamic_free(root_internal_node);
		return E_NOMEM;
	}
	// create_tmpfs_vnode 內部已經設定了 root_internal_node->v_node = root_vnode;

	// 3. 設定 mount 結構
	mount_info->root = root_vnode; // 掛載點的根 vnode 就是剛建立的 tmpfs 根 vnode
	mount_info->fs = fs_info; 	// 指向 tmpfs 的 filesystem 結構

	uart_send_string("[tmpfs_setup_mount] tmpfs root vnode created successfully.\r\n");
	return E_OK;
}

// 用來描述 tmpfs 檔案系統本身的結構實例
struct filesystem tmpfs_filesystem = {
	.name = "tmpfs",
	.setup_mount = tmpfs_setup_mount,
};

// 供外部呼叫以註冊 tmpfs 到 VFS
void tmpfs_init() {
	uart_send_string("[tmpfs_init] Registering tmpfs filesystem.\r\n");
	if (register_filesystem(&tmpfs_filesystem) != E_OK) {
		uart_send_string("[tmpfs_init] Failed to register tmpfs!\r\n");
		// 處理註冊失敗的情況，例如 panic
	} else {
		uart_send_string("[tmpfs_init] tmpfs registered successfully.\r\n");
	}
}