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
KERNEL_TARGET = send_kernel/kernel8.img
KERNEL_ELF_TARGET = kernel8.elf
BOOTLOADER_TARGET = bootloader.img
BOOTLOADER_ELF_TARGET = bootloader.elf

# Define source files
KERNEL_SRCS = boot_kernel.S kernel.c uart.c strcmp.c mailbox.c power.c
BOOTLOADER_SRCS = boot_bootloader.S bootloader.c uart.c strcmp.c

# Define object files
KERNEL_OBJS = $(KERNEL_SRCS:.S=.o)
KERNEL_OBJS := $(KERNEL_OBJS:.c=.o)
BOOTLOADER_OBJS = $(BOOTLOADER_SRCS:.S=.o)
BOOTLOADER_OBJS := $(BOOTLOADER_OBJS:.c=.o)

# Linker scripts
KERNEL_LD_SCRIPT = linker_kernel.ld
BOOTLOADER_LD_SCRIPT = linker_bootloader.ld

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
	rm -f $(KERNEL_TARGET) $(KERNEL_ELF_TARGET) $(BOOTLOADER_TARGET) $(BOOTLOADER_ELF_TARGET) $(KERNEL_OBJS) $(BOOTLOADER_OBJS)

