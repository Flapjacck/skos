# SKOS Operating System

A simple operating system project for learning and experimentation purposes. [Followed this guide](https://wiki.osdev.org/Bare_Bones)

## Important Requirements

**Note:** This project must be built and developed on a Linux system. Windows is not recommended due to compatibility issues with the required tools and build process.

If you're using Windows, consider:

- Using Windows Subsystem for Linux (WSL)
- Setting up a Linux virtual machine
- Dual booting with a Linux distribution

## Tools Used

```shell
sudo apt install build-essential nasm qemu-system-x86 grub-pc-bin xorriso
```

## Build Instructions

Ensure you are on a Linux system, then run the Makefile to compile and run the operating system in QEMU:

```shell
make
make run
```
