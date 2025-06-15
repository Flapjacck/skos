# Compiler and linker settings
CC = gcc
CFLAGS = -m32 -ffreestanding -Wall -Wextra -fno-exceptions -fno-stack-protector
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
	keyboard.o \
	shell.o \
	memory.o \
	timer.o

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