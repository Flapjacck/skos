@echo off
set PROJECTDIR=%~dp0
set BUILDDIR=%PROJECTDIR%build

if not exist %BUILDDIR% mkdir %BUILDDIR%

:: Build bootloader
nasm -f bin src/boot/boot.asm -o build/boot.bin

:: Run QEMU with explicit format
qemu-system-x86_64 -drive format=raw,file=build/boot.bin,index=0,if=floppy