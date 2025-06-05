# Compiler and linker settings
CC = gcc
CFLAGS = -m32 -ffreestanding -Wall -Wextra -fno-exceptions -fno-stack-protector
LDFLAGS = -m elf_i386 -T src/kernel/linker.ld

# Object files
KERNEL_OBJS = \
	boot.o \
	kernel.o

# Default target
all: myos.iso

# Compile assembly files
boot.o: src/boot/bootloader.asm
	nasm -f elf32 src/boot/bootloader.asm -o boot.o

# Compile C files
kernel.o: src/kernel/kernel.c
	$(CC) $(CFLAGS) -c src/kernel/kernel.c -o kernel.o

# Link the kernel
myos.bin: $(KERNEL_OBJS)
	$(LD) $(LDFLAGS) -o $@ $(KERNEL_OBJS)

# Create an ISO image
myos.iso: myos.bin
	mkdir -p isodir/boot/grub
	cp myos.bin isodir/boot/myos.bin
	cp src/boot/grub.cfg isodir/boot/grub/grub.cfg
	grub-mkrescue -o myos.iso isodir

# Run the OS in QEMU
run: myos.iso
	qemu-system-i386 -cdrom myos.iso

# Clean up
clean:
	rm -f *.o *.bin *.iso
	rm -rf isodir