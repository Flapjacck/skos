all: os-image.bin

bootloader.bin: src/boot/bootloader.asm
	nasm -f bin src/boot/bootloader.asm -o bootloader.bin

kernel.o: src/kernel/kernel.c
	gcc -m32 -fno-pie -fno-pic -ffreestanding -c src/kernel/kernel.c -o kernel.o

kernel.bin: kernel.o
	ld -m elf_i386 -Ttext 0x1000 --oformat binary -e kmain -o kernel.bin kernel.o

os-image.bin: bootloader.bin kernel.bin
	cat bootloader.bin kernel.bin > os-image.bin

run: os-image.bin
	qemu-system-i386 -fda os-image.bin

clean:
	rm -f *.bin *.o
