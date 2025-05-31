ARMGNU ?= aarch64-linux-gnu
CFLAGS = -Wall -nostdlib -nostartfiles -ffreestanding
# CFLAGS = -fno-stack-protector -ffreestanding -nostdlib -nostartfiles
LDFLAGS = -T fork_test.ld

all: userProcessStatus.img fork_test.img upload_img

userProcessStatus.img: userProcessStatus.o
	$(ARMGNU)-ld  -o userProcessStatus.elf userProcessStatus.o
	$(ARMGNU)-objcopy userProcessStatus.elf -O binary userProcessStatus.img
	cp ./userProcessStatus.img ../rootfs/
	(cd ../rootfs/ && find . | cpio -o -H newc > ../initramfs.cpio)

fork_test.img: fork_test.o
	$(ARMGNU)-ld $(LDFLAGS) -o fork_test.elf fork_test.o
	$(ARMGNU)-objcopy fork_test.elf -O binary fork_test.img
	cp ./fork_test.img ../rootfs/
	cp ./vm.img ../rootfs/
	(cd ../rootfs/ && find . | cpio -o -H newc > ../initramfs.cpio)
upload_img:
	cp ./vm.img ../rootfs/
	cp ./vfs1.img ../rootfs/
	(cd ../rootfs/ && find . | cpio -o -H newc > ../initramfs.cpio)
userProcessStatus.o: userProcessStatus.S
	$(ARMGNU)-gcc -MMD -c $< -o $@

fork_test.o: fork_test.c
	$(ARMGNU)-gcc $(CFLAGS) -MMD -c $< -o $@

exe_fork_test.o: exe_fork_test.S
	$(ARMGNU)-gcc -MMD -c $< -o $@

clean:
	rm -f *.o *.d *.elf *.img