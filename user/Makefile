ARMGNU ?= aarch64-linux-gnu

all: userProcessStatus.img fork_test.img syscall_test.img

userProcessStatus.img: userProcessStatus.o
	$(ARMGNU)-ld -o userProcessStatus.elf userProcessStatus.o
	$(ARMGNU)-objcopy userProcessStatus.elf -O binary userProcessStatus.img
	cp ./userProcessStatus.img ../rootfs/
	(cd ../rootfs/ && find . | cpio -o -H newc > ../initramfs.cpio)

fork_test.img: fork_test.o
	$(ARMGNU)-ld -o fork_test.elf fork_test.o
	$(ARMGNU)-objcopy fork_test.elf -O binary fork_test.img
	cp ./fork_test.img ../rootfs/
	(cd ../rootfs/ && find . | cpio -o -H newc > ../initramfs.cpio)

syscall_test.img: syscall_test.o
	$(ARMGNU)-ld -o syscall_test.elf syscall_test.o
	$(ARMGNU)-objcopy syscall_test.elf -O binary syscall_test.img
	cp ./syscall_test.img ../rootfs/
	(cd ../rootfs/ && find . | cpio -o -H newc > ../initramfs.cpio)

userProcessStatus.o: userProcessStatus.S
	$(ARMGNU)-gcc -MMD -c $< -o $@

fork_test.o: fork_test.c
	$(ARMGNU)-gcc -MMD -c $< -o $@

syscall_test.o: syscall_test.c
	$(ARMGNU)-gcc -MMD -c $< -o $@

clean:
	rm -f *.o *.d *.elf *.img

dump: userProcessStatus.img
	qemu-aarch64 -M raspi3 -kernel userProcessStatus.img -display none -d in_asm