SRCDIR   = src
BUILDDIR = build
OBJDIR   = $(BUILDDIR)/obj
BINDIR   = $(BUILDDIR)/bin

OBJFILES = $(patsubst $(SRCDIR)/%.c,$(OBJDIR)/%.obj,$(wildcard $(SRCDIR)/*.c)) $(patsubst $(SRCDIR)/%.S,$(OBJDIR)/%.obj,$(wildcard $(SRCDIR)/*.S))
LIBFILES = $(wildcard $(SRCDIR)/*.a)

# Cross-platform directory stuff
ifeq ($(OS),Windows_NT)
RM         = del /q /f $(NOSTDOUT) $(NOSTDERR)
RMDIR      = if exist "$1" rmdir /s /q "$1"
MKDIR      = if not exist "$1" mkdir "$1"
else
RM         = rm -f
RMDIR      = rm -rf $1
MKDIR      = mkdir -p $1
endif
ENSUREDIR  = $(call MKDIR,$(dir $@))

# set for your model (check the tutorial i followed's cmake system to figure out this)
# yea this sucks, ill fix later™
C_DEFINES = -DIOBPLUS=1 -DRPI3=1

TOOLCHAIN = compiler/gcc-arm-none-eabi-10-2020-q4-major/bin/arm-none-eabi
C_INCLUDES = -I$(SRCDIR)/inc -I$(SRCDIR)
C_FLAGS   = -mfpu=crypto-neon-fp-armv8 -mfloat-abi=hard -march=armv8-a+crc -mtune=cortex-a53 -mno-unaligned-access -O2 -g -nostartfiles
ASM_FLAGS = -mfpu=crypto-neon-fp-armv8 -mfloat-abi=hard -march=armv8-a+crc -mtune=cortex-a53 -mno-unaligned-access

# end of stuff that needs to be set per-model


all: $(BINDIR)/kernel.img

# Build C object
$(OBJDIR)/%.obj: $(SRCDIR)/%.c
	@echo [C obj]:   $^ -^> $@
	@$(ENSUREDIR)
	@$(TOOLCHAIN)-gcc $(C_INCLUDES) $(C_DEFINES) $(C_FLAGS) -o $@ -c $^

# Build ASM object
$(OBJDIR)/%.obj: $(SRCDIR)/%.S
	@echo [ASM obj]: $^ -^> $@
	@$(ENSUREDIR)
	@$(TOOLCHAIN)-gcc $(C_INCLUDES) $(C_DEFINES) $(ASM_FLAGS) -o $@ -c $^

# Link ELF executable
$(BINDIR)/kernel.elf: $(OBJFILES) $(LIBFILES)
	@echo [Link]: $^ -^> $@
	@$(ENSUREDIR)
	@$(TOOLCHAIN)-gcc $(C_INCLUDES) $(C_DEFINES) $(C_FLAGS) $^ -o $(BINDIR)/kernel.elf
#	@$(TOOLCHAIN)-objdump --source-comment=# bin/kernel.elf > kernel.disasm

# Extract the kernel image
$(BINDIR)/kernel.img: $(BINDIR)/kernel.elf
	@echo [Extract]: $^ -^> $@
	@$(ENSUREDIR)
	@$(TOOLCHAIN)-objcopy $(BINDIR)/kernel.elf -O binary $(BINDIR)/kernel.img
	@echo Done! Output is in $@

.PHONY: clean
clean:
	@$(call RMDIR,$(BUILDDIR))