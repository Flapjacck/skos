# skos

## About

A simple operating system project for learning and experimentation purposes.

## Features

**Currently Implemented:**

- GRUB bootloader integration
- GDT/IDT setup and interrupt handling  
- PS/2 keyboard driver with full scancode support
- Timer driver and PIC management
- Basic shell with input buffering and file system commands
- Memory management and VGA text mode
- ATA/IDE hard disk driver
- FAT32 file system support
- File operations: `ls`, `cat`, `fsinfo` commands

**Planned:**

- File creation and writing support
- Process management and virtual memory
- System calls and user mode

## Requirements

**Linux system required** (native, WSL2, or VM)

```bash
# Ubuntu/Debian
sudo apt install build-essential nasm qemu-system-x86 grub-pc-bin xorriso dosfstools

# Arch Linux  
sudo pacman -S base-devel nasm qemu grub xorriso dosfstools

# Fedora
sudo dnf install gcc nasm qemu grub2-tools xorriso dosfstools
```

**Note**: `dosfstools` is required for creating FAT32 disk images.

## Quick Start

```bash
git clone https://github.com/Flapjacck/skos.git
cd skos
make          # Build kernel, create ISO, and disk image automatically
make run      # Launch in QEMU with attached disk
```

### Disk Image Management

The build system automatically creates and manages a disk image:

- **Automatic Creation**: Running `make` creates a 64MB FAT32 disk image if it doesn't exist
- **Test Files**: The disk comes pre-populated with test files (README.TXT, TEST.TXT, HELLO.TXT)
- **QEMU Integration**: The disk is automatically attached when running `make run`

### Manual Disk Operations

```bash
# Create a new disk image manually
dd if=/dev/zero of=disk.img bs=1M count=64
mkfs.fat -F 32 disk.img

# Mount the disk to add files
mkdir -p mnt
sudo mount -o loop disk.img mnt
echo "Your content here" | sudo tee mnt/YOURFILE.TXT
sudo umount mnt

# Run with custom disk
make run  # Uses disk.img automatically
```

## Resources

- [OSDev Wiki](https://wiki.osdev.org/) - OS development guide
- [Intel Manuals](https://software.intel.com/content/www/us/en/develop/articles/intel-sdm.html) - x86 architecture reference

---
