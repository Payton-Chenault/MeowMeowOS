CC = i686-elf-gcc
LD = i686-elf-ld
AS = nasm
OBJCOPY = i686-elf-objcopy
QEMU = qemu-system-x86_64

SRC_DIR = src
BUILD_DIR = build
BIN_DIR = bin

INCLUDE_FLAGS = -I./src -I./src/kernel
CFLAGS = $(INCLUDE_FLAGS) -g -ffreestanding -nostdlib -nostartfiles -nodefaultlibs -Wall -O0 -std=gnu99
ASFLAGS = -f elf -g
LDFLAGS = -T $(SRC_DIR)/linker.ld -ffreestanding -O0 -nostdlib

ENTRY_ASM = $(SRC_DIR)/kernel/kernel.asm
ENTRY_OBJ = $(BUILD_DIR)/kernel.asm.o

C_SOURCES = $(shell find $(SRC_DIR)/kernel -name "*.c")
ASM_SOURCES = $(shell find $(SRC_DIR)/kernel -name "*.asm" | grep -v "kernel.asm")

C_OBJS = $(C_SOURCES:$(SRC_DIR)/kernel/%.c=$(BUILD_DIR)/%.o)
ASM_OBJS = $(ASM_SOURCES:$(SRC_DIR)/kernel/%.asm=$(BUILD_DIR)/%.asm.o)

OBJS = $(ENTRY_OBJ) $(C_OBJS) $(ASM_OBJS)

all: $(BIN_DIR)/MeowMeowOS.img

$(BUILD_DIR):
	@mkdir -p $(BUILD_DIR)

$(BIN_DIR):
	@mkdir -p $(BIN_DIR)

$(BIN_DIR)/boot.bin: $(SRC_DIR)/bootloader/boot.asm | $(BIN_DIR)
	$(AS) -f bin $< -o $@

$(BUILD_DIR)/%.o: $(SRC_DIR)/kernel/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -c $< -o $@

$(ENTRY_OBJ): $(ENTRY_ASM) | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

$(BUILD_DIR)/%.asm.o: $(SRC_DIR)/kernel/%.asm | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(AS) $(ASFLAGS) $< -o $@

# 1. Link to ELF (The "Debuggable" file)
$(BIN_DIR)/kernel.elf: $(OBJS) | $(BIN_DIR)
	$(CC) $(LDFLAGS) $(OBJS) -o $@

# 2. Extract raw binary from the ELF (The "Runnable" file)
$(BIN_DIR)/kernel.bin: $(BIN_DIR)/kernel.elf
	$(OBJCOPY) -O binary $< $@

$(BIN_DIR)/MeowMeowOS.bin: $(BIN_DIR)/boot.bin $(BIN_DIR)/kernel.bin
	cat $(BIN_DIR)/boot.bin $(BIN_DIR)/kernel.bin > $@

$(BIN_DIR)/MeowMeowOS.img: $(BIN_DIR)/MeowMeowOS.bin
	dd if=/dev/zero of=$@ bs=1M count=500 status=progress
	dd if=$< of=$@ conv=notrunc status=progress
	@echo "Disk Image Ready: $@"

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

run: $(BIN_DIR)/MeowMeowOS.img
	$(QEMU) -drive format=raw,file=$(BIN_DIR)/MeowMeowOS.img,index=0,media=disk -m 512M

debug: $(BIN_DIR)/MeowMeowOS.img
	$(QEMU) -drive format=raw,file=$(BIN_DIR)/MeowMeowOS.img,index=0,media=disk -serial stdio -machine pcspk-audiodev=audio0 -audiodev sdl,id=audio0 -d int -D qemu.log -m 512M | tee MeowMeowOS.log