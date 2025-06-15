
# Operating System Capstone - Lab 2: Booting

This project implements the core components of a boot process for the Raspberry Pi 3, including a self-relocating bootloader, kernel loading via UART, an initial ramdisk parser, a simple memory allocator, and devicetree parsing.

## Basic Exercise 1: Reboot
This section implements a command to restart the Raspberry Pi 3.

* A `reboot` command has been added to the shell in `kernel.c`.
* The command calls the `reset` function defined in `power.c`.
* This function interacts directly with the Raspberry Pi's Power Manager registers (`PM_RSTC` and `PM_WDOG`) to trigger a hardware reset, as detailed in the BCM2837 documentation.

## Basic Exercise 2: UART Bootloader
To streamline development, a two-stage boot process was created. A small bootloader is first loaded from the SD card, which then loads the main kernel image via UART. This avoids the need to move the SD card between the host and the Pi for every change.

* The `Makefile` is configured to build two separate binaries: `bootloader.img` and `kernel8.img`.
* The bootloader (`bootloader.c`) initializes UART and waits for the kernel image.
* A Python script (`send_kernel.py`) is used on the host machine to send the kernel. It first sends a custom header containing a magic number (`0x544F4F42`), the kernel size, and a simple checksum.
* The bootloader verifies the header and checksum after receiving the kernel data.
* Upon successful verification, the bootloader writes the kernel to address `0x80000` and jumps to it, passing along the devicetree address.

## Basic Exercise 3: Initial Ramdisk
The kernel needs a root filesystem to load initial programs. Since no storage drivers are implemented yet, an initial ramdisk (initramfs) is used. The ramdisk is a `cpio` archive loaded into memory.

* The `cpio.c` file contains functions to parse a "New ASCII Format" CPIO archive.
* The `list_cpio_files` function implements the `ls` command. It iterates through the CPIO headers and prints the pathname of each file.
* The `parse_cpio_archive` function implements the `cat` command. It prompts the user for a filename, finds the corresponding file in the archive, and prints its content to the console.
* These functions calculate offsets based on header information and data alignment rules (4-byte boundaries) to correctly locate file pathnames and content.

## Basic Exercise 4: Simple Allocator
Early kernel initialization requires a way to allocate memory before the main dynamic memory manager is ready. A simple, non-freeing allocator was implemented for this purpose.

* `alloc.c` contains the `simple_alloc` function, which implements a bump allocator.
* A dedicated heap region is defined in the kernel's linker script (`linker_kernel.ld`) using `__heap_start` and `__heap_end` symbols.
* `simple_alloc` allocates memory by simply advancing a pointer (`heap_index`) through this pre-defined heap region.
* It checks for heap overflow and ensures that all allocated blocks are 8-byte aligned.
* A `memAlloc <size>` command was added to the shell to test this functionality from the console.

## Advanced Exercise 1: Bootloader Self-Relocation
To make the bootloader more robust, it was designed to relocate itself in memory. This allows it to load the kernel at address `0x80000` even if the bootloader itself was loaded there initially, removing the need for `kernel_address` in `config.txt`.

* The bootloader's entry code is in `boot_bootloader.S`.
* It is linked to load at `0x60000`, but the firmware loads it to `0x80000`.
* The assembly code copies the entire bootloader from its initial location (`0x80000`) to its target address (`0x60000`).
* After the relocation is complete, it sets up the stack and jumps to the C function `bootloader_main`, adjusted for the new memory location.

## Advanced Exercise 2: Devicetree Parsing
Instead of hardcoding memory addresses for hardware like the initial ramdisk, the kernel now parses a Flattened Devicetree (DTB) file passed by the bootloader.

* A complete devicetree parser is implemented in `devicetree.c` and `devicetree.h`.
* The `fdt_traverse` function walks the devicetree structure by interpreting tokens like `FDT_BEGIN_NODE`, `FDT_PROP`, and `FDT_END_NODE`.
* It uses a callback mechanism, allowing different parts of the kernel to search the tree for relevant information.
* The `get_initramfs_info` function uses this traversal to find the `initramfs` location. It provides a specific callback (`initramfs_callback`) that searches for the `/chosen` node and reads the `linux,initrd-start` property.
* The `kernel_main` function now calls `fdt_init` at startup to get this address dynamically, falling back to a default address only if the DTB parse fails.
* The bootloader is responsible for receiving the DTB address from the firmware and passing it to the kernel in the `x0` register upon jumping to it.
