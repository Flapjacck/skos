# skos

## About

A simple operating system project for learning and experimentation purposes.

## Features

**Currently Implemented:**

- GRUB bootloader integration
- GDT/IDT setup and interrupt handling  
- PS/2 keyboard driver with full scancode support
- Timer driver and PIC management
- Basic shell with input buffering
- Memory management and VGA text mode

**Planned:**

- File system and process management
- Virtual memory and system calls

## Requirements

**Linux system required** (native, WSL2, or VM)

```bash
# Ubuntu/Debian
sudo apt install build-essential nasm qemu-system-x86 grub-pc-bin xorriso

# Arch Linux  
sudo pacman -S base-devel nasm qemu grub xorriso

# Fedora
sudo dnf install gcc nasm qemu grub2-tools xorriso
```

## Quick Start

```bash
git clone <repository-url>
cd skos
make          # Build kernel and create ISO
make run      # Launch in QEMU
```

## Resources

- [OSDev Wiki](https://wiki.osdev.org/) - OS development guide
- [Intel Manuals](https://software.intel.com/content/www/us/en/develop/articles/intel-sdm.html) - x86 architecture reference

---
