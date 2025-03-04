# Makefile
CC = aarch64-linux-gnu-gcc # 或者 clang
CFLAGS = -Wall -nostdlib -nostartfiles -ffreestanding -Iinclude -mgeneral-regs-only -g
# -Wall: Enable all the commonly used warning messages.
# -nostdlib: Do not use the standard library when linking.
# -nostartfiles: Do not use the standard startup files when linking.
# -ffreestanding: Indicate that the environment is freestanding, meaning it does not have the standard library or startup files.
# -Iinclude: Add the 'include' directory to the list of directories to be searched for header files.
# -mgeneral-regs-only: Restrict the compiler to use only the general-purpose registers.
# -g: Generate debug information to be used by GDB debugger.
LD = aarch64-linux-gnu-ld # 或者 ld.lld
OBJCOPY = aarch64-linux-gnu-objcopy # 或者 llvm-objcopy

# 定義目標檔案
TARGET = kernel8.img
ELF_TARGET = kernel8.elf

# 定義原始碼檔案
SRCS = boot.S kernel.c uart.c strcmp.c mailbox.c
#  

# 定義物件檔案
# WARN
OBJS = $(SRCS:.S=.o)
OBJS := $(OBJS:.c=.o)

# 連結腳本
LD_SCRIPT = linker.ld

# 預設目標
all: $(TARGET)

# 產生核心映像檔
$(TARGET): $(ELF_TARGET)
	$(OBJCOPY) -O binary $(ELF_TARGET) $(TARGET)

# 產生 ELF 檔
$(ELF_TARGET): $(OBJS) $(LD_SCRIPT)
	$(LD) -T $(LD_SCRIPT) -o $(ELF_TARGET) $(OBJS)

# 編譯 C 程式碼
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 編譯組合語言程式碼
%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

# 清除編譯產生的檔案
clean:
	rm -f $(TARGET) $(ELF_TARGET) $(OBJS)

