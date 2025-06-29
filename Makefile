# Compiler and linker settings
CC = gcc
CFLAGS = -m32 -ffreestanding -Wall -Wextra -fno-exceptions -fstack-protector -g
LDFLAGS = -m elf_i386 -T src/kernel/linker.ld

# Object files
KERNEL_OBJS = \
	boot.o \
	kernel.o \
	gdt.o \
	gdt_asm.o \
	idt.o \
	idt_asm.o \
	pic.o \
	debug.o \
	keyboard.o \
	shell.o \
	memory.o \
	timer.o \
	ata.o \
	fat32.o

# Default target
all: myos.iso

# Compile assembly files
boot.o: src/boot/bootloader.asm
	nasm -f elf32 src/boot/bootloader.asm -o boot.o

# Compile C files
kernel.o: src/kernel/kernel.c
	$(CC) $(CFLAGS) -c src/kernel/kernel.c -o kernel.o

# Compile GDT C implementation
gdt.o: src/kernel/gdt.c
	$(CC) $(CFLAGS) -c src/kernel/gdt.c -o gdt.o

# Compile GDT assembly functions
gdt_asm.o: src/kernel/gdt.asm
	nasm -f elf32 src/kernel/gdt.asm -o gdt_asm.o

# Compile IDT C implementation
idt.o: src/kernel/idt.c
	$(CC) $(CFLAGS) -c src/kernel/idt.c -o idt.o

# Compile IDT assembly functions
idt_asm.o: src/kernel/idt.asm
	nasm -f elf32 src/kernel/idt.asm -o idt_asm.o

# Compile PIC C implementation
pic.o: src/kernel/pic.c
	$(CC) $(CFLAGS) -c src/kernel/pic.c -o pic.o

# Compile debug C implementation
debug.o: src/kernel/debug.c
	$(CC) $(CFLAGS) -c src/kernel/debug.c -o debug.o

# Compile keyboard driver
keyboard.o: src/drivers/keyboard.c
	$(CC) $(CFLAGS) -c src/drivers/keyboard.c -o keyboard.o

# Compile shell driver
shell.o: src/drivers/shell.c
	$(CC) $(CFLAGS) -c src/drivers/shell.c -o shell.o

# Compile timer driver
timer.o: src/drivers/timer.c
	$(CC) $(CFLAGS) -c src/drivers/timer.c -o timer.o

# Compile memory manager
memory.o: src/kernel/memory.c
	$(CC) $(CFLAGS) -c src/kernel/memory.c -o memory.o

# Compile ATA driver
ata.o: src/drivers/ata.c
	$(CC) $(CFLAGS) -c src/drivers/ata.c -o ata.o

# Compile FAT32 file system
fat32.o: src/kernel/fat32.c
	$(CC) $(CFLAGS) -c src/kernel/fat32.c -o fat32.o

# Link the kernel
myos.bin: $(KERNEL_OBJS)
	$(LD) $(LDFLAGS) -o $@ $(KERNEL_OBJS)

# Create an ISO image
myos.iso: myos.bin
	mkdir -p isodir/boot/grub
	cp myos.bin isodir/boot/myos.bin
	cp src/boot/grub.cfg isodir/boot/grub/grub.cfg
	grub-mkrescue -o myos.iso isodir

# Create disk image if it doesn't exist
disk.img:
	@if [ ! -f disk.img ]; then \
		echo "Creating 64MB disk image..."; \
		dd if=/dev/zero of=disk.img bs=1M count=64; \
		echo "Formatting with FAT32..."; \
		mkfs.fat -F 32 disk.img; \
		echo "Mounting disk image..."; \
		mkdir -p mnt; \
		sudo mount -o loop disk.img mnt; \
		echo "Adding test files..."; \
		echo "This is a test file for SKOS FAT32 implementation." | sudo tee mnt/README.TXT > /dev/null; \
		echo "Hello from SKOS file system!" | sudo tee mnt/TEST.TXT > /dev/null; \
		echo "Another test file with more content for testing." | sudo tee mnt/HELLO.TXT > /dev/null; \
		sudo umount mnt; \
		echo "Disk image created successfully!"; \
	fi

# Run the OS in QEMU with disk attached
run: myos.iso disk.img
	qemu-system-i386 -cdrom myos.iso -hda disk.img -boot d

# Run with debugging enabled
debug: myos.iso disk.img
	qemu-system-i386 -cdrom myos.iso -hda disk.img -boot d -s -S

# Clean up
clean:
	rm -f *.o *.bin *.iso
	rm -rf isodir