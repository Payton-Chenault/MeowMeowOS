CC = i686-elf-gcc
LD = i686-elf-ld
AS = nasm
OBJCOPY = i686-elf-objcopy
QEMU = qemu-system-x86_64

SRC_DIR = src
BUILD_DIR = build
BIN_DIR = bin
USR_DIR = src/usr

INCLUDE_FLAGS = -I./src -I./src/kernel
CFLAGS = $(INCLUDE_FLAGS) -g -ffreestanding -nostdlib -nostartfiles -nodefaultlibs -Wall -O0 -std=gnu99
ASFLAGS = -f elf -g
LDFLAGS = -Wl,-T,$(SRC_DIR)/linker.ld -ffreestanding -O0 -nostdlib

USER_LDFLAGS = -Wl,-T,$(USR_DIR)/user_linker.ld -nostdlib -m32 -no-pie
USER_CFLAGS = -I$(USR_DIR)/libs -g -ffreestanding -nostdlib -Wall -O0 -std=gnu99 -m32 -fno-pic -fno-pie

ENTRY_ASM = $(SRC_DIR)/kernel/kernel.asm
ENTRY_OBJ = $(BUILD_DIR)/kernel.asm.o
C_SOURCES = $(shell find $(SRC_DIR)/kernel -name "*.c")
ASM_SOURCES = $(shell find $(SRC_DIR)/kernel -name "*.asm" | grep -v "kernel.asm")
C_OBJS = $(C_SOURCES:$(SRC_DIR)/kernel/%.c=$(BUILD_DIR)/%.o)
ASM_OBJS = $(ASM_SOURCES:$(SRC_DIR)/kernel/%.asm=$(BUILD_DIR)/%.asm.o)
OBJS = $(ENTRY_OBJ) $(C_OBJS) $(ASM_OBJS)

USER_SOURCES = $(shell find $(USR_DIR)/progs -name "*.c" 2>/dev/null || true)
USER_OBJS = $(USER_SOURCES:$(USR_DIR)/progs/%.c=$(BUILD_DIR)/user/%.o)
USER_ELFS = $(USER_SOURCES:$(USR_DIR)/progs/%.c=$(BIN_DIR)/%.elf)

# QEMU configuration - single-threaded TCG avoids mutex bugs
QEMU_FLAGS = -drive format=raw,file=bin/MeowMeowOS.img -m 512M -accel tcg,thread=single

# Debug flags (without -d int to reduce risk; you can add it back if needed)
QEMU_DEBUG_FLAGS = -serial stdio

all: $(BIN_DIR)/MeowMeowOS.img $(USER_ELFS) inject

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

$(BIN_DIR)/kernel.elf: $(OBJS) | $(BIN_DIR)
	$(CC) $(OBJS) $(LDFLAGS) -o $@

$(BIN_DIR)/kernel.bin: $(BIN_DIR)/kernel.elf
	$(OBJCOPY) -O binary $< $@

$(BIN_DIR)/MeowMeowOS.bin: $(BIN_DIR)/boot.bin $(BIN_DIR)/kernel.bin
	cat $(BIN_DIR)/boot.bin $(BIN_DIR)/kernel.bin > $@

$(BIN_DIR)/MeowMeowOS.img: $(BIN_DIR)/MeowMeowOS.bin $(BIN_DIR)/boot.bin
	@if [ ! -f $@ ]; then \
		echo "Creating new 500MB disk image..."; \
		dd if=/dev/zero of=$@ bs=1M count=500 status=progress; \
		echo "Writing bootsector and kernel..."; \
		dd if=$(BIN_DIR)/boot.bin of=$@ bs=512 count=1 conv=notrunc status=progress; \
		dd if=$(BIN_DIR)/kernel.bin of=$@ bs=512 seek=1 conv=notrunc status=progress; \
	else \
		echo "Updating kernel (Preserving FAT16 Boot Sector)..."; \
		dd if=$(BIN_DIR)/kernel.bin of=$@ bs=512 seek=1 conv=notrunc status=progress; \
	fi
	@echo "Disk Image Ready: $@"

$(BUILD_DIR)/user/%.o: $(USR_DIR)/progs/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -c $< -o $@

$(BIN_DIR)/%.elf: $(BUILD_DIR)/user/%.o | $(BIN_DIR)
	$(CC) $< $(USER_LDFLAGS) -o $@

inject: $(BIN_DIR)/MeowMeowOS.img $(USER_ELFS)
	@echo "Injecting User Programs into FAT16 Disk..."
	@if [ -z "$(USER_ELFS)" ]; then echo "No user programs found in $(USR_DIR)/progs to inject."; exit 0; fi
	@sudo mkdir -p /mnt/meowos
	@if sudo mount -t vfat -o loop $(BIN_DIR)/MeowMeowOS.img /mnt/meowos; then \
		for elf in $(USER_ELFS); do \
			if [ -f "$$elf" ]; then \
				sudo cp $$elf /mnt/meowos/$$(basename $$elf); \
				echo " -> Injected $$(basename $$elf)"; \
			fi \
		done; \
		echo "--- Contents of disk according to Linux: ---"; \
		ls -la /mnt/meowos; \
		echo "--------------------------------------------"; \
		sudo sync; \
		sudo umount /mnt/meowos; \
	else \
		echo "WARNING: Mount failed. You need to run 'format' in the OS first."; \
	fi

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

run: $(BIN_DIR)/MeowMeowOS.img
	$(QEMU) $(QEMU_FLAGS)

debug: $(BIN_DIR)/MeowMeowOS.img
	$(QEMU) $(QEMU_FLAGS) $(QEMU_DEBUG_FLAGS) | tee MeowMeowOS.log

# Debug with full logging (may trigger QEMU bugs; use only if necessary)
debug-full: $(BIN_DIR)/MeowMeowOS.img
	$(QEMU) $(QEMU_FLAGS) -serial stdio -d int -D qemu.log | tee MeowMeowOS.log
compile-inject-debug:
	make compile
	make inject
	make debug

compile:
	make clean
	./build.sh
	make debug

.PHONY: all clean run debug debug-full compile inject