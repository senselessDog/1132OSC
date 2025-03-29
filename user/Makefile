ARMGNU ?= aarch64-linux-gnu

all: userProcessStatus.img

userProcessStatus.img: userProcessStatus.o
	$(ARMGNU)-ld -o userProcessStatus.elf userProcessStatus.o
	$(ARMGNU)-objcopy userProcessStatus.elf -O binary userProcessStatus.img
	cp ./userProcessStatus.img ../rootfs/
	(cd ../rootfs/ && find . | cpio -o -H newc > ../initramfs.cpio)

userProcessStatus.o: userProcessStatus.S
	$(ARMGNU)-gcc -MMD -c $< -o $@

clean:
	rm -f *.o *.d *.elf *.img

dump: userProcessStatus.img
	qemu-aarch64 -M raspi3 -kernel userProcessStatus.img -display none -d in_asm