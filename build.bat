@echo off
set PROJECTDIR=%~dp0
set BUILDDIR=%PROJECTDIR%build

if not exist %BUILDDIR% mkdir %BUILDDIR%

:: Build bootloader stages
nasm -f bin src/boot/boot.asm -o build/boot.bin
nasm -f bin src/boot/stage2.asm -o build/stage2.bin

:: Combine boot and stage2
copy /b build\boot.bin + build\stage2.bin build\final.bin

:: Run QEMU with explicit format
qemu-system-x86_64 -drive format=raw,file=build/final.bin,index=0,if=floppy