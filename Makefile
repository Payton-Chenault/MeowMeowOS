
CC = i686-elf-gcc
LD = i686-elf-ld
AS = nasm
QEMU = qemu-system-x86_64

SRC_DIR = src
BUILD_DIR = build
BIN_DIR = bin

INCLUDE_FLAGS = -I./src -I./src/kernel
CFLAGS = $(INCLUDE_FLAGS) -g -ffreestanding -nostdlib -nostartfiles -nodefaultlibs -Wall -O0 -std=gnu99
ASFLAGS = -f elf -g
LDFLAGS = -T $(SRC_DIR)/linker.ld -ffreestanding -O0 -nostdlib

C_SOURCES = $(shell find $(SRC_DIR)/kernel -name "*.c")
ASM_SOURCES = $(shell find $(SRC_DIR)/kernel -name "*.asm")

OBJS = $(C_SOURCES:$(SRC_DIR)/kernel/%.c=$(BUILD_DIR)/%.o)
OBJS += $(ASM_SOURCES:$(SRC_DIR)/kernel/%.asm=$(BUILD_DIR)/%.asm.o)

all: $(BIN_DIR)/MeowMeowOS.bin

$(BIN_DIR)/boot.bin: $(SRC_DIR)/bootloader/boot.asm
	@mkdir -p $(BIN_DIR)
	$(AS) -f bin $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/kernel/%.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(BUILD_DIR)/%.asm.o: $(SRC_DIR)/kernel/%.asm
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

$(BIN_DIR)/kernel.bin: $(OBJS)
	@mkdir -p $(BIN_DIR)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

$(BIN_DIR)/MeowMeowOS.bin: $(BIN_DIR)/boot.bin $(BIN_DIR)/kernel.bin
	cat $(BIN_DIR)/boot.bin $(BIN_DIR)/kernel.bin > $@
	dd if=/dev/zero bs=512 count=32 >> $@

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

run: all
	$(QEMU) -hda $(BIN_DIR)/MeowMeowOS.bin