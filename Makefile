# Makefile
CC = aarch64-linux-gnu-gcc
CFLAGS = -Wall -nostdlib -nostartfiles -ffreestanding -Iinclude -mgeneral-regs-only -g
# -Wall: Enable all the commonly used warning messages.
# -nostdlib: Do not use the standard library when linking.
# -nostartfiles: Do not use the standard startup files when linking.
# -ffreestanding: Indicate that the environment is freestanding, meaning it does not have the standard library or startup files.
# -Iinclude: Add the 'include' directory to the list of directories to be searched for header files.
# -mgeneral-regs-only: Restrict the compiler to use only the general-purpose registers.
# -g: Generate debug information to be used by GDB debugger.
LD = aarch64-linux-gnu-ld
OBJCOPY = aarch64-linux-gnu-objcopy

# Define targets
KERNEL_TARGET = script/kernel8.img
KERNEL_ELF_TARGET = kernel8.elf
BOOTLOADER_TARGET = bootloader.img
BOOTLOADER_ELF_TARGET = bootloader.elf

# Define source files with new paths
KERNEL_SRCS = $(wildcard src/kernel/*.c) $(wildcard src/kernel/*.S) $(wildcard src/lib/*.c) \
				$(wildcard boot/kernel/*.S)
# boot/boot_kernel.S kernel/kernel.c lib/uart.c lib/strcmp.c kernel/mailbox.c \
# lab2: kernel/power.c kernel/cpio.c kernel/alloc.c kernel/devicetree.c \
# lab3: kernel/run_userprogram.c kernel/exception_entry.c boot/exception.S \
# 		kernel/async_io.c kernel/gpu_interrupt.c kernel/timeout.c kernel/task_queue.c

BOOTLOADER_SRCS = $(wildcard src/bootloader/*.c) $(wildcard src/lib/*.c) $(wildcard boot/bootloader/*.S)
# boot/bootloader.S lib/uart.c lib/strcmp.c
# Define object files
KERNEL_OBJS = $(KERNEL_SRCS:.S=.o)
KERNEL_OBJS := $(KERNEL_OBJS:.c=.o)
BOOTLOADER_OBJS = $(BOOTLOADER_SRCS:.S=.o)
BOOTLOADER_OBJS := $(BOOTLOADER_OBJS:.c=.o)
# Linker scripts
KERNEL_LD_SCRIPT = boot/kernel/linker_kernel.ld
BOOTLOADER_LD_SCRIPT = boot/bootloader/linker_bootloader.ld

# Default target
all: $(KERNEL_TARGET) $(BOOTLOADER_TARGET) 

# Generate kernel image
$(KERNEL_TARGET): $(KERNEL_ELF_TARGET)
	$(OBJCOPY) -O binary $(KERNEL_ELF_TARGET) $(KERNEL_TARGET)

# Generate kernel ELF
$(KERNEL_ELF_TARGET): $(KERNEL_OBJS) $(KERNEL_LD_SCRIPT)
	$(LD) -T $(KERNEL_LD_SCRIPT) -o $(KERNEL_ELF_TARGET) $(KERNEL_OBJS)

# Generate bootloader image
$(BOOTLOADER_TARGET): $(BOOTLOADER_ELF_TARGET)
	$(OBJCOPY) -O binary $(BOOTLOADER_ELF_TARGET) $(BOOTLOADER_TARGET)

# Generate bootloader ELF
$(BOOTLOADER_ELF_TARGET): $(BOOTLOADER_OBJS) $(BOOTLOADER_LD_SCRIPT)
	$(LD) -T $(BOOTLOADER_LD_SCRIPT) -o $(BOOTLOADER_ELF_TARGET) $(BOOTLOADER_OBJS)

# Compile C source files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Compile assembly source files
%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

# Clean up generated files
clean:
	rm -f $(KERNEL_TARGET) $(KERNEL_ELF_TARGET) $(BOOTLOADER_TARGET) $(BOOTLOADER_ELF_TARGET)
	find . -name "*.o" -type f -delete

qemu:
	qemu-system-aarch64 -machine raspi3b -kernel bootloader.img -nographic -serial null -serial pty -initrd initramfs.cpio -dtb bcm2710-rpi-3-b-plus.dtb -S -s

show_qemu:
	sudo minicom -D /dev/pts/7 -b 115200
gdb_init:
	gdb-multiarch -x .gdbinit
python:
	/bin/python3 /home/kuan/lab/lab4/script/upload_kernel.py

show_raspberryPi:
	sudo minicom -D /dev/ttyUSB0 -b 115200