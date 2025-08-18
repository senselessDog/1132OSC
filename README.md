# Lab 7: Virtual File System (VFS)

This lab implements a Virtual File System (VFS) layer for the operating system. This provides a unified interface for various underlying file systems and devices. The implementation includes a memory-based `tmpfs` as the root filesystem, a read-only `initramfs` populated from a CPIO archive, and special device files for UART and the framebuffer. 💾

---

## 📖 Introduction & Background

The VFS abstracts the specific details of different filesystems, presenting a single, hierarchical tree structure to user processes. Key concepts include:

* **Vnode**: A generic node in the VFS tree representing a file, directory, or device. It contains pointers to file and vnode operations (`f_ops`, `v_ops`) which are implemented by the underlying filesystem.
* **Filesystem**: A registered filesystem type, like `tmpfs`, which knows how to manage its own internal structures.
* **Mount**: An instance of a filesystem attached to a specific vnode in the VFS tree. 
* **File Handle**: Represents an opened file, tracking the current read/write position (`f_pos`) for a specific process.

This project introduces a significant number of new files and data structures to support these concepts, primarily within the `include/` and `src/kernel/fs/` directories.

---

## 🔧 Basic Exercises

### Basic Exercise 1 - Root File System

This exercise establishes the core VFS framework and implements `tmpfs` as the root filesystem (`/`).

#### VFS Core Implementation (`vfs.h`, `vfs.c`)

* Defined the primary VFS data structures: `struct vnode`, `struct file`, `struct mount`, `struct filesystem`, `struct file_operations`, and `struct vnode_operations`.
* Implemented the main VFS API functions:
    * `register_filesystem()`: Allows a filesystem driver to make itself known to the kernel.
    * `vfs_open()`, `vfs_close()`, `vfs_read()`, `vfs_write()`: These functions act as the main entry points for file operations. They resolve the path to a vnode, create a file handle, and dispatch the call to the appropriate `f_ops` of the underlying filesystem.
* The global `rootfs` mount point was created and initialized in `kernel_main()`.

#### Tmpfs Implementation (`tmpfs.h`, `tmpfs.c`)

* `tmpfs` is a simple, memory-based filesystem. An internal inode structure (`tmpfs_inode_t`) was created to store file content, directory entries, and metadata.
* The required `file_operations` (`tmpfs_write`, `tmpfs_read`, etc.) and `vnode_operations` (`tmpfs_lookup`, `tmpfs_create`, `tmpfs_mkdir`) were implemented.
* A `tmpfs_filesystem` struct was created and registered with the VFS using `tmpfs_init()`.
* The `tmpfs_setup_mount()` function handles the creation of the root vnode for a new `tmpfs` instance.

---

### Basic Exercise 2 - Multi-level VFS

This part extends the VFS to handle subdirectories and mounting other filesystems.

* **Pathname Lookup (`vfs_resolve_path`)**: A robust path resolution function was implemented. It parses a pathname component by component, starting from a base directory (like the CWD or root). Crucially, it handles crossing mount points: when it encounters a vnode that is a mount point, it seamlessly transitions to the root vnode of the mounted filesystem.
* **`vfs_mkdir()`**: Implemented to allow the creation of new directories.
* **`vfs_mount()`**: A system call wrapper that allows mounting a new filesystem onto an existing directory vnode.

---

### Basic Exercise 3 - Multitask VFS

This exercise integrates the VFS with the multitasking system, giving each process its own filesystem context.

#### Task Struct Extension (`thread.h`)

* The `thread_t` struct was extended to include:
    * `struct vnode* cwd`: A pointer to the Current Working Directory vnode.
    * `struct file* fd_table[MAX_PROCESS_OPEN_FILES]`: A file descriptor table for each process.

#### System Call Implementation (`syscall.c`)

* System calls were created to expose VFS functionality to user processes: `sys_open`, `sys_close`, `sys_read`, `sys_write`, `sys_mkdir`, `sys_mount`, `sys_chdir`.
* These syscalls act as wrappers. They retrieve the current task's context (like its `fd_table` and `cwd`), copy arguments from user space, call the corresponding VFS function, and return the result.

#### Fork Integration (`sys_fork`)

* The `sys_fork` logic was updated to correctly duplicate the parent's filesystem context for the child. This includes copying the file descriptor table and incrementing the reference counts on the corresponding vnodes to ensure they are not prematurely deallocated.

---

### Basic Exercise 4 - /initramfs

This exercise implements a read-only filesystem based on an initial RAM disk (`initramfs`) provided as a CPIO archive.

#### Initramfs Driver (`initramfs.h`, `initramfs.c`)

* A CPIO header parser was implemented to read the `initramfs.cpio` archive passed by the bootloader.
* During `initramfs_setup_mount`, the driver traverses the CPIO archive and builds a tree of `initramfs_inode_t` structures in memory, representing the files and directories within the archive.
* The `file_operations` for `initramfs` are implemented as read-only. Any calls to `write`, `create`, or `mkdir` return a read-only error (`-E_ROFS`).

#### Mounting

* In `kernel_main`, after the root `tmpfs` is mounted, `vfs_mkdir("/initramfs")` is called, followed by `vfs_mount("/initramfs", "initramfs")` to mount the CPIO-backed filesystem.

---

## 🚀 Advanced Exercises

### Advanced Exercise 1 - /dev/uart

A special device file for the UART was created at `/dev/uart`, allowing it to be accessed like a standard file.

#### Device Node Creation (`uart_vfs.c`)

* A dedicated set of file operations, `uart_dev_file_ops`, was created. The `read` and `write` functions in this struct directly call the low-level `uart_recv()` and `uart_send()` functions.
* The `vfs_mknod()` function was used to create a special file node at `/dev/uart`. `mknod` associates a path with a specific set of file operations, effectively creating a device file.

#### Standard I/O

* After creation, `/dev/uart` is opened three times, and the resulting file handles are assigned to file descriptors **0 (stdin)**, **1 (stdout)**, and **2 (stderr)** for the initial user process. Child processes created via `fork` will inherit these file descriptors.

---

### Advanced Exercise 2 - /dev/framebuffer

A write-only special device file was created for the framebuffer, allowing user processes to draw to the screen via standard file I/O. 🎨

#### Framebuffer Driver (`framebuffer_vfs.c`)

* A `framebuffer_init()` function was created, encapsulating the mailbox calls required to set up the screen resolution and get the framebuffer's physical address.
* A dedicated `fb_dev_file_ops` struct was implemented:
    * **`write`**: Copies data from the user buffer directly into the framebuffer memory, respecting the current file position (`f_pos`).
    * **`lseek64`**: Allows the user process to change the write position (`f_pos`), enabling random access to the screen buffer.
    * **`ioctl`**: Implemented to handle requests for framebuffer information (width, height, pitch), copying a `framebuffer_info` struct back to the user.

#### Device Node Creation

* Similar to the UART, `vfs_mknod()` was used to create the `/dev/framebuffer` node, linking it to `fb_dev_file_ops`.

#### Memory Mapping

* The physical address of the framebuffer obtained from the mailbox is mapped into the kernel's virtual address space to allow safe access.
