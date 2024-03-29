# -----------------------------------------------------------------------
# Project Information
# -----------------------------------------------------------------------

PROJECT_IDX	= 4

# -----------------------------------------------------------------------
# Host Linux Variables
# -----------------------------------------------------------------------

SHELL       = /bin/sh
DISK        = /dev/sdb
TTYUSB1     = /dev/ttyUSB1
DIR_OSLAB   = $(HOME)/OSLab-RISC-V
DIR_QEMU    = $(DIR_OSLAB)/qemu
DIR_UBOOT   = $(DIR_OSLAB)/u-boot

# -----------------------------------------------------------------------
# Build and Debug Tools
# -----------------------------------------------------------------------

HOST_CC         = gcc
CROSS_PREFIX    = riscv64-unknown-linux-gnu-
CC              = $(CROSS_PREFIX)gcc
AR              = $(CROSS_PREFIX)ar
OBJDUMP         = $(CROSS_PREFIX)objdump
GDB             = $(CROSS_PREFIX)gdb
QEMU            = $(DIR_QEMU)/riscv64-softmmu/qemu-system-riscv64
UBOOT           = $(DIR_UBOOT)/u-boot
MINICOM         = minicom

# -----------------------------------------------------------------------
# Build/Debug Flags and Variables
# -----------------------------------------------------------------------

CFLAGS          = -O0 -fno-builtin -nostdlib -nostdinc -Wall -mcmodel=medany -ggdb3

BOOT_INCLUDE    = -I$(DIR_ARCH)/include
BOOT_CFLAGS     = $(CFLAGS) $(BOOT_INCLUDE) -Wl,--defsym=TEXT_START=$(BOOTLOADER_ENTRYPOINT) -T riscv.lds

KERNEL_INCLUDE  = -I$(DIR_ARCH)/include -Iinclude -Idrivers
KERNEL_CFLAGS   = $(CFLAGS) $(KERNEL_INCLUDE) -Wl,--defsym=TEXT_START=$(KERNEL_ENTRYPOINT) -T riscv.lds 

DECOMPR_INCLUDE = -I$(DIR_DEFLATE) -I$(DIR_ARCH)/include -Iinclude
DECOMPR_CFLAGS  = $(CFLAGS) $(DECOMPR_INCLUDE) -Wl,--defsym=TEXT_START=$(DECOMPRESS_ENTRYPOINT) -T riscv.lds

USER_INCLUDE    = -I$(DIR_TINYLIBC)/include
USER_CFLAGS     = $(CFLAGS) $(USER_INCLUDE)
USER_LDFLAGS    = -L$(DIR_BUILD) -ltinyc

QEMU_LOG_FILE   = $(DIR_OSLAB)/oslab-log.txt
QEMU_OPTS       = -nographic -machine virt -m 256M -kernel $(UBOOT) -bios none \
                     -drive if=none,format=raw,id=image,file=${ELF_IMAGE} \
                     -device virtio-blk-device,drive=image \
                     -monitor telnet::45454,server,nowait -serial mon:stdio \
                     -D $(QEMU_LOG_FILE) -d oslab
QEMU_DEBUG_OPT  = -s -S
QEMU_SMP_OPT	= -smp 2

# -----------------------------------------------------------------------
# UCAS-OS Entrypoints and Variables
# -----------------------------------------------------------------------

DIR_ARCH        = ./arch/riscv
DIR_BUILD       = ./build
DIR_DRIVERS     = ./drivers
DIR_INIT        = ./init
DIR_KERNEL      = ./kernel
DIR_LIBS        = ./libs
DIR_TINYLIBC    = ./tiny_libc
DIR_TEST        = ./test
DIR_TEST_PROJ   = $(DIR_TEST)/test_project$(PROJECT_IDX)
DIR_DECOMPRESS  = ./decompress
DIR_DEFLATE     = ./tools/deflate

BOOTLOADER_ENTRYPOINT   = 0x50200000
KERNEL_ENTRYPOINT       = 0xffffffc050202000
USER_ENTRYPOINT         = 0x10000
DECOMPRESS_ENTRYPOINT   = 0x59000000

# -----------------------------------------------------------------------
# UCAS-OS Kernel Source Files
# -----------------------------------------------------------------------

SRC_BOOT     = $(wildcard $(DIR_ARCH)/boot/*.S)
SRC_ARCH     = $(wildcard $(DIR_ARCH)/kernel/*.S)
SRC_BIOS     = $(wildcard $(DIR_ARCH)/bios/*.c)
SRC_DRIVER   = $(wildcard $(DIR_DRIVERS)/*.c)
SRC_INIT     = $(wildcard $(DIR_INIT)/*.c)
SRC_KERNEL   = $(wildcard $(DIR_KERNEL)/*/*.c)
SRC_LIBS     = $(wildcard $(DIR_LIBS)/*.c)
SRC_START   = $(wildcard $(DIR_ARCH)/kernel/*.c)
SRC_DECOMPR  = $(wildcard $(DIR_DECOMPRESS)/*.c)
SRC_DEFLATE1 = $(wildcard $(DIR_DEFLATE)/lib/*.c)
SRC_DEFLATE2 = $(wildcard $(DIR_DEFLATE)/*.c)
SRC_HEAD     = $(wildcard $(DIR_DECOMPRESS)/*.S)
SRC_LIBS_DECOMPRESS = $(wildcard $(DIR_LIBS)/string.c)


SRC_MAIN    = $(SRC_ARCH) $(SRC_START) $(SRC_INIT) $(SRC_BIOS) $(SRC_DRIVER) $(SRC_KERNEL) $(SRC_LIBS)
SRC_DECOMPRESS = $(SRC_HEAD) $(SRC_DECOMPR) $(SRC_LIBS_DECOMPRESS) $(SRC_DEFLATE1) $(SRC_DEFLATE2)

ELF_BOOT    = $(DIR_BUILD)/bootblock
ELF_MAIN    = $(DIR_BUILD)/main
ELF_IMAGE   = $(DIR_BUILD)/image
ELF_DECOMPRESS = $(DIR_BUILD)/decompress

# -----------------------------------------------------------------------
# UCAS-OS User Source Files
# -----------------------------------------------------------------------

SRC_CRT0    = $(wildcard $(DIR_ARCH)/crt0/*.S)
OBJ_CRT0    = $(DIR_BUILD)/$(notdir $(SRC_CRT0:.S=.o))

SRC_LIBC    = $(wildcard ./tiny_libc/*.c)
OBJ_LIBC    = $(patsubst %.c, %.o, $(foreach file, $(SRC_LIBC), $(DIR_BUILD)/$(notdir $(file))))
LIB_TINYC   = $(DIR_BUILD)/libtinyc.a

SRC_SHELL	= $(DIR_TEST)/shell.c
SRC_USER    = $(SRC_SHELL) $(wildcard $(DIR_TEST_PROJ)/*.c)
ELF_USER    = $(patsubst %.c, %, $(foreach file, $(SRC_USER), $(DIR_BUILD)/$(notdir $(file))))

# -----------------------------------------------------------------------
# Host Linux Tools Source Files
# -----------------------------------------------------------------------

SRC_CREATEIMAGE = ./tools/createimage.c
ELF_CREATEIMAGE = $(DIR_BUILD)/$(notdir $(SRC_CREATEIMAGE:.c=))

# -----------------------------------------------------------------------
# Top-level Rules
# -----------------------------------------------------------------------

all: dirs elf image asm # floppy

dirs:
	@mkdir -p $(DIR_BUILD)

clean:
	rm -rf $(DIR_BUILD)

floppy:
	sudo fdisk -l $(DISK)
	sudo dd if=$(DIR_BUILD)/image of=$(DISK)3 conv=notrunc

asm: $(ELF_BOOT) $(ELF_DECOMPRESS) $(ELF_MAIN) $(ELF_USER)
	for elffile in $^; do $(OBJDUMP) -d $$elffile > $(notdir $$elffile).txt; done

gdb:
	$(GDB) $(ELF_MAIN) $(ELF_USER) -ex "target remote:1234"

run:
	$(QEMU) $(QEMU_OPTS)

run-smp:
	$(QEMU) $(QEMU_OPTS) $(QEMU_SMP_OPT)

debug:
	$(QEMU) $(QEMU_OPTS) $(QEMU_DEBUG_OPT)

debug-smp:
	$(QEMU) $(QEMU_OPTS) $(QEMU_SMP_OPT) $(QEMU_DEBUG_OPT)

minicom:
	sudo $(MINICOM) -D $(TTYUSB1)

.PHONY: all dirs clean floppy asm gdb run debug viewlog minicom

# -----------------------------------------------------------------------
# UCAS-OS Rules
# -----------------------------------------------------------------------

$(ELF_BOOT): $(SRC_BOOT) riscv.lds
	$(CC) $(BOOT_CFLAGS) -o $@ $(SRC_BOOT) -e main

$(ELF_DECOMPRESS): $(SRC_DECOMPRESS) riscv.lds
	$(CC) $(DECOMPR_CFLAGS) -DFREESTANDING -o $@ $(SRC_DECOMPRESS)
  
$(ELF_MAIN): $(SRC_MAIN) riscv.lds
	$(CC) $(KERNEL_CFLAGS) -o $@ $(SRC_MAIN) -e _boot

$(OBJ_CRT0): $(SRC_CRT0)
	$(CC) $(USER_CFLAGS) -I$(DIR_ARCH)/include -c $< -o $@

$(LIB_TINYC): $(OBJ_LIBC)
	$(AR) rcs $@ $^

$(DIR_BUILD)/%.o: $(DIR_TINYLIBC)/%.c
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(DIR_BUILD)/%: $(DIR_TEST_PROJ)/%.c $(OBJ_CRT0) $(LIB_TINYC) riscv.lds
	$(CC) $(USER_CFLAGS) -o $@ $(OBJ_CRT0) $< $(USER_LDFLAGS) -Wl,--defsym=TEXT_START=$(USER_ENTRYPOINT) -T riscv.lds

$(DIR_BUILD)/%: $(DIR_TEST)/%.c $(OBJ_CRT0) $(LIB_TINYC) riscv.lds
	$(CC) $(USER_CFLAGS) -o $@ $(OBJ_CRT0) $< $(USER_LDFLAGS) -Wl,--defsym=TEXT_START=$(USER_ENTRYPOINT) -T riscv.lds

elf: $(ELF_BOOT) $(ELF_DECOMPRESS) $(ELF_MAIN) $(LIB_TINYC) $(ELF_USER)

.PHONY: elf

# -----------------------------------------------------------------------
# Host Linux Rules
# -----------------------------------------------------------------------

$(ELF_CREATEIMAGE): $(SRC_CREATEIMAGE) $(SRC_DEFLATE1) $(SRC_DEFLATE2)
	$(HOST_CC) $(SRC_CREATEIMAGE) $(SRC_DEFLATE1) $(SRC_DEFLATE2) -o $@ -ggdb -Wall -I$(DIR_DEFLATE) -I$(DIR_ARCH)/include -DFREESTANDING

image: $(ELF_CREATEIMAGE) $(ELF_BOOT) $(ELF_DECOMPRESS) $(ELF_MAIN) $(ELF_USER)
	cd $(DIR_BUILD) && ./$(<F) --vm $(filter-out $(<F), $(^F)) 
  #&& dd if=/dev/zero of=image oflag=append conv=notrunc bs=512MB count=2

.PHONY: image
