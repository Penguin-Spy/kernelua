SRCDIR = src
OBJDIR = build/obj
BINDIR = build/bin

OBJFILES = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.obj,$(wildcard $(SRCDIR)/*.c)) $(patsubst $(SRCDIR)/%.S,$(OBJDIR)/%.obj,$(wildcard $(SRCDIR)/*.S))

# set to point to where you downloaded the compiler, should be /gcc-arm-none-eabi-<VERSION>/bin/arm-none-eabi
# make sure to include the "arm-none-eabi" at the end, exactly as shown
TOOLCHAIN = compiler/gcc-arm-none-eabi-10-2020-q4-major/bin/arm-none-eabi

# set for your model (check the tutorial i followed's cmake system to figure out this)
# yea this sucks, ill fix later™
C_DEFINES = -DIOBPLUS=1 -DRPI3=1

C_INCLUDES = -I$(SRCDIR)/inc -I$(SRCDIR)
C_FLAGS = -mfpu=crypto-neon-fp-armv8 -mfloat-abi=hard -march=armv8-a+crc -mtune=cortex-a53 -mfpu=crypto-neon-fp-armv8 -mfloat-abi=hard -march=armv8-a+crc -mtune=cortex-a53 -O4 -g -nostartfiles -mfloat-abi=hard
ASM_FLAGS = -mfpu=crypto-neon-fp-armv8 -mfloat-abi=hard -march=armv8-a+crc -mtune=cortex-a53


# Build C object
$(OBJDIR)/%.obj: $(SRCDIR)/%.c
	@echo [C obj]:   $^ -^> $@
	@$(TOOLCHAIN)-gcc $(C_INCLUDES) $(C_DEFINES) $(C_FLAGS) -o $@ -c $^

# Build ASM object
$(OBJDIR)/%.obj: $(SRCDIR)/%.S
	@echo [ASM obj]: $^ -^> $@
	@$(TOOLCHAIN)-gcc $(C_INCLUDES) $(C_DEFINES) $(ASM_FLAGS) -o $@ -c $^

#$(BINDIR)/%.bin: $(OBJDIR)/%.obj
#	echo $^ $@
#	$(TOOLCHAIN)-gcc $(C_FLAGS)

$(BINDIR)/kernel.img: $(OBJFILES)
	@echo [Linking]: $^ -^> $@
	@$(TOOLCHAIN)-gcc $(C_INCLUDES) $(C_DEFINES) $(C_FLAGS) $^ -o $(BINDIR)/kernel.bin
	@$(TOOLCHAIN)-objcopy $(BINDIR)/kernel.bin -O binary $(BINDIR)/kernel.img
#$(TOOLCHAIN)-objdump.exe -l -S -D ./kernel.armc-016.rpi3bp
	@echo Done! Output is in $@

all: $(BINDIR)/kernel.img