# Kernelua
> Pronounced "`Kernel-Lua`" (as one word), capitalized as a noun. `Lua` by itself has a capital `L`: http://www.lua.org/about.html#name.

Kernelua is a Rasberry Pi kernel written in C that allows running Lua as the operating system of a device. The primary goal is to run ComputerCraft's CraftOS on real hardware (ideally in a Pocket Computer enclosure).  
This project is based on the valvers.com tutorial, check it out: https://www.valvers.com/open-software/raspberry-pi/bare-metal-programming-in-c-part-1.

# Installing
Kernelua is not ready for production use yet, so to run it you must create a bootable SD card image manually.  
The SD card must contain `bootcode.bin`, `fixup.dat`, and `start.elf` from the Rasberry Pi repo: https://github.com/raspberrypi/firmware, and the `kernel.img` created by building this project (output in `build/bin`, see #Compiling below).  

Additionally, I had to add a `config.txt` with the following contents to fix some weird margins with my display:
```
disable_overscan=1
```
The firmware will still properly boot without this file present, at least on my RPi3B+, it just caused the display to appear blurry which completly ruined the point of using an 8x8 pixel font.

# Compiling
Kernelua is built using GNU make (unlike the tutorial which uses Cmake), so compiling is about as simple as running `make`.  
However, you must first download the ARM compiler from the Arm website: https://developer.arm.com/tools-and-software/open-source-software/developer-tools/gnu-toolchain/gnu-rm/downloads and extract it to `compiler/gcc-arm-none-eabi-VERSION/`.  
I only own a Rasberry Pi 3B+, and currently the build system only sets the proper flags for this model. If you have a different model, uhh look at the makefile and have fun :) Also if u use VS Code add the flags to the C/C++ extension settings so it doesn't yell at you.