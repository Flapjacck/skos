@echo off
echo Creating SKOS structure...

:: Create build script
echo @echo off > build.bat
echo set PROJECTDIR=%%~dp0 >> build.bat
echo set BUILDDIR=%%PROJECTDIR%%build >> build.bat
echo. >> build.bat
echo if not exist %%BUILDDIR%% mkdir %%BUILDDIR%% >> build.bat
echo. >> build.bat
echo nasm -f bin src/boot/boot.asm -o build/boot.bin >> build.bat

echo Setup complete!