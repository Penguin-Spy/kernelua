SRCDIR   = src
BUILDDIR = build
FONTDIR = font

OBJFILES = $(patsubst $(SRCDIR)/%.c,$(BUILDDIR)/%.obj,$(wildcard $(SRCDIR)/*.c)) $(patsubst $(SRCDIR)/%.S,$(BUILDDIR)/%.obj,$(wildcard $(SRCDIR)/*.S))
LIBFILES = src/libuspi.a src/liblua.a

ENSUREDIR  = mkdir -p $(dir $@)

# set for your model (check the tutorial i followed's cmake system to figure out this)
# yea this sucks, ill fix later™
C_DEFINES = -DIOBPLUS=1 -DRPI3=1

TOOLCHAIN = compiler/gcc-arm-none-eabi-10-2020-q4-major/bin/arm-none-eabi
ARCH = -mfpu=crypto-neon-fp-armv8 -mfloat-abi=hard -march=armv8-a+crc -mtune=cortex-a53
C_FLAGS   = $(ARCH) -O2 -nostartfiles

# end of stuff that needs to be set per-model

CFLAGS := -I$(SRCDIR)/inc -I$(SRCDIR) -I$(FONTDIR) $(C_DEFINES) $(C_FLAGS)
CC := $(TOOLCHAIN)-gcc

all: kernel.img

# Generate font header file
$(FONTDIR)/font.h: $(FONTDIR)/klscii.png
	@echo "[Font]:    $^ → $@"
	@$(ENSUREDIR)
	@py font/generate_font.py $^ $@

# Build C object
$(BUILDDIR)/%.obj: $(SRCDIR)/%.c
	@echo "[C obj]:   $^ → $@"
	@$(ENSUREDIR)
	@$(CC) $(CFLAGS) -c $^ -o $@

# Build ASM object
$(BUILDDIR)/%.obj: $(SRCDIR)/%.S
	@echo "[ASM obj]: $^ → $@"
	@$(ENSUREDIR)
	@$(CC) $(CFLAGS) -c $^ -o $@

# Link ELF executable
$(BUILDDIR)/kernel.elf: $(FONTDIR)/font.h $(OBJFILES) $(LIBFILES)
	@echo "[Link]:   $^ → $@"
	@$(ENSUREDIR)
	@$(CC) $(CFLAGS) $^ -l:libm.a -o $@
#	@$(TOOLCHAIN)-objdump --source-comment=# bin/kernel.elf > kernel.disasm

# Extract the kernel image
kernel.img: $(BUILDDIR)/kernel.elf
	@echo "[Extract]: $^ → $@"
	@$(ENSUREDIR)
	@$(TOOLCHAIN)-objcopy $^ -O binary $@
	@echo "Done! Output is in $@"

.PHONY: clean
clean:
	@rm -f kernel.img
	@rm -rf $(BUILDDIR)
