all: os-image.bin

bootloader.bin: bootloader.asm
	nasm -f bin bootloader.asm -o bootloader.bin

kernel.o: kernel.c
	gcc -m16 -ffreestanding -c kernel.c -o kernel.o

kernel.bin: kernel.o
	ld -Ttext 0x1000 --oformat binary -o kernel.bin kernel.o

os-image.bin: bootloader.bin kernel.bin
	cat bootloader.bin kernel.bin > os-image.bin

run: os-image.bin
	qemu-system-i386 -fda os-image.bin

clean:
	rm -f *.bin *.o
