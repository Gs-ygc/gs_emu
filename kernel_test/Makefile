
CROSS_COMPILE=/opt/riscv32-uclibc/bin/riscv32-buildroot-linux-uclibc-
CC=$(CROSS_COMPILE)gcc

CFLAGS:=-mabi=ilp32 -fPIE -pie -static -march=rv32ima -Os -s -g -ffunction-sections -fdata-sections
LDFLAGS:=-Wl,-elf2flt=-r -Wl,-gc-sections
# ~/下载/sixtyfourmb-2.dtb
# ../firmware/minimal.dtb
init: src/init.c
	$(CC) $(CFLAGS) $^ $(LDFLAGS) -o rootfs/sbin/$@

cpio: init
	cd rootfs ; find . -print0 | cpio --null -ov --format=newc > ~/initram.cpio

kernel:
	make -C /home/gs2ygc/OS_STU/linux-6.9.1 ARCH=riscv CROSS_COMPILE=riscv32-unknown-linux-gnu- -j32

qemu:kernel
	qemu-system-riscv32 -cpu rv32,mmu=false -m 128M -nographic -M virt -kernel /home/gs2ygc/OS_STU/linux-6.9.1/arch/riscv/boot/Image -bios none

nemu:kernel
	make -C ../
	../build/riscv32-nemu-interpreter --mode=3 --dtb=/home/gs2ygc/STU_PA/ics2023/nemu/firmware/sixtyfourmb-3.dtb --kernel=/home/gs2ygc/OS_STU/linux-6.9.1/arch/riscv/boot/Image

