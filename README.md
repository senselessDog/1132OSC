## Backgorund
This project was developed for the "Operating System Capstone" course at NYCU.
This is the complete context about how to build raspberryPi in bare models.
This project have 7 lab. Hope you enjoying.
## Prerequisites: Environment Setup (Lab 0)

This project requires a specific development environment to build and run the code for the Raspberry Pi 3. The following steps outline the necessary setup for cross-platform development.

#### 1. Cross-Platform Toolchain

* **Compiler**: A cross-compiler is required to compile C and Assembly source code for the Raspberry Pi's 64-bit ARM architecture (ARM Cortex-A53). You need to install a `aarch64` toolchain (e.g., `aarch64-linux-gnu-gcc`).
* **Linker**: In bare-metal programming, a custom linker script is used to define the memory layout of the final program. The linker combines object files into an executable ELF file.
* **Object Copy**: The Raspberry Pi bootloader cannot load ELF files directly. A tool like `objcopy` is used to convert the ELF file into a raw binary image (`kernel8.img`) that the Pi can boot.

#### 2. Emulator (QEMU)

* **Purpose**: QEMU is used to test the kernel image in an emulated environment before deploying it to a real Raspberry Pi.
* **Installation**: You need to install `qemu-system-aarch64`.
* **Usage**: The kernel can be tested on QEMU to verify basic functionality and inspect assembly output.

#### 3. SD Card Preparation

* **Goal**: To make the SD card bootable on a Raspberry Pi 3, it must contain a FAT-formatted partition with the necessary firmware and the compiled kernel image (`kernel8.img`).
* **Firmware**: The GPU firmware files (`bootcode.bin`, `fixup.dat`, `start.elf`) are essential for the boot process. These can be downloaded from the official Raspberry Pi firmware repository.
* **Kernel Image**: Your compiled `kernel8.img` must be placed in the FAT partition alongside the firmware files.

#### 4. Hardware Interaction (UART)

* **Hardware**: A USB to UART serial converter is used to communicate between the host computer and the Raspberry Pi.
* **Connection**: The TX, RX, and GND pins of the converter must be connected to the corresponding GPIO pins on the Raspberry Pi.
* **Serial Console**: A terminal program like `screen` or `putty` is used on the host machine to interact with the Pi's serial output at a baud rate of 115200.(In my example is `screen`) This allows you to view boot messages and interact with the shell.

## Lab 1: Bare-Metal Interactive Shell for Raspberry Pi

### Basic Exercise 1: Basic Initialization

* **Goal:**
    * Initialize the BSS segment to zero.
    * Set the stack pointer to a proper address.
* **Implementation (`boot.S`):**
    * The `clear_bss` code block iterates from `__bss_start` to `__bss_end`, writing zero to each memory location.
    * The `stack_setup` code block loads the `_stack_top` address into the stack pointer register (`sp`).

***

### Basic Exercise 2: Mini UART

* **Goal:**
    * Set up the Mini UART to act as a communication bridge between the Raspberry Pi and a host computer.
* **Implementation (`uart.c`):**
    * The `uart_init()` function configures GPIO pins 14 and 15 for UART functionality.
    * It initializes all necessary Mini UART control registers, including baud rate and data size settings.
    * Functions like `uart_send()` and `uart_recv()` provide the required two-way communication.

***

### Basic Exercise 3: Simple Shell

* **Goal:**
    * Implement an interactive shell.
    * The shell must support `help` and `hello` commands.
* **Implementation (`kernel.c`):**
    * The `shell()` function establishes an infinite loop to continuously process user input.
    * It uses `strcmp()` to parse commands.
    * My implementation includes the required `help` and `hello` commands.
    * It also features extended commands: `boardrev`, `armmem`, `modinfo`, and `coreid`.
    * Input handling correctly processes both `\r` and `\n` to prevent display issues.

***

### Basic Exercise 4: Mailbox

* **Goal:**
    * Use the mailbox interface to retrieve hardware information from the GPU.
    * Required information to print: board revision, ARM memory base address, and ARM memory size.
* **Implementation (`mailbox.c` & `kernel.c`):**
    * The `mailbox.c` file provides the low-level driver, including the `mailbox_call()` function to communicate with the GPU.
    * `get_board_revision()` and `get_arm_memory()` functions create the specific message formats for hardware queries.
    * These functions are integrated into the shell via the `boardrev`, `armmem`, and `modinfo` commands, allowing for easy display of the required information.
