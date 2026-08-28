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

# User program sources
USER_SOURCES = $(shell find $(USR_DIR)/progs -name "*.c" 2>/dev/null || true)
USER_OBJS = $(USER_SOURCES:$(USR_DIR)/progs/%.c=$(BUILD_DIR)/user/%.o)
USER_ELFS = $(USER_SOURCES:$(USR_DIR)/progs/%.c=$(BIN_DIR)/%.elf)

# User libc C sources
USER_LIB_SOURCES = $(shell find $(USR_DIR)/libs -name "*.c" 2>/dev/null || true)
USER_LIB_OBJS = $(USER_LIB_SOURCES:$(USR_DIR)/libs/%.c=$(BUILD_DIR)/user/libs/%.o)

# crt0 runtime object
USER_CRT0_OBJ = $(BUILD_DIR)/user/libs/crt0.o

# QEMU configuration - single-threaded TCG avoids mutex bugs, std vga required for VBE, RTL8139 for Network
QEMU_FLAGS = -drive format=raw,file=bin/MeowMeowOS.img -m 512M -accel tcg,thread=single -vga std -netdev user,id=n0 -device rtl8139,netdev=n0

# Debug flags
QEMU_DEBUG_FLAGS = -serial stdio
HOST_OS = $(shell uname -s)
FIRST_BOOT_TIMEOUT ?= 30
FIRST_BOOT_LOG = build/first-boot.log
QEMU_WINDOW_WIDTH ?= 1200
QEMU_WINDOW_HEIGHT ?= 800
QEMU_RESIZE_SCRIPT = scripts/resize_qemu_window.sh

ifeq ($(HOST_OS),Darwin)
QEMU_DISPLAY_FLAGS = -display cocoa,zoom-to-fit=on
else ifeq ($(IS_WSL),1)
export DISPLAY ?= :0
export SDL_VIDEODRIVER ?= x11
export LIBGL_ALWAYS_SOFTWARE ?= 1
QEMU_DISPLAY_FLAGS = -display sdl,gl=off
else
QEMU_DISPLAY_FLAGS = -display sdl,gl=off
endif

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

# User libc objects
$(BUILD_DIR)/user/libs/%.o: $(USR_DIR)/libs/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -c $< -o $@

# User crt0 object (NASM)
$(USER_CRT0_OBJ): $(USR_DIR)/libs/crt0.asm | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(AS) -f elf $< -o $@

# User program objects
$(BUILD_DIR)/user/%.o: $(USR_DIR)/progs/%.c | $(BUILD_DIR)
	@mkdir -p $(dir $@)
	$(CC) $(USER_CFLAGS) -c $< -o $@

# User ELF link rule: program + libc + crt0
$(BIN_DIR)/%.elf: $(BUILD_DIR)/user/%.o $(USER_LIB_OBJS) $(USER_CRT0_OBJ) | $(BIN_DIR)
	$(CC) $< $(USER_LIB_OBJS) $(USER_CRT0_OBJ) $(USER_LDFLAGS) -o $@

# Target to convert logo.png into splash.bmp via Python
generate_splash:
	@echo "Generating splash.bmp from asset..."
	python3 scripts/convert_logo.py

inject: $(BIN_DIR)/MeowMeowOS.img $(USER_ELFS) generate_splash
	@echo "Injecting User Programs and Splash Asset into FAT16 Disk..."
	@if [ -z "$(USER_ELFS)" ]; then echo "No user programs found in $(USR_DIR)/progs to inject."; exit 0; fi
	@if [ "$(HOST_OS)" = "Darwin" ]; then \
		if ! command -v mcopy >/dev/null 2>&1; then \
			echo "ERROR: mcopy is required on macOS. Install it with: brew install mtools"; \
			exit 1; \
		fi; \
		for elf in $(USER_ELFS); do \
			if [ -f "$$elf" ]; then \
				mcopy -o -i $(BIN_DIR)/MeowMeowOS.img "$$elf" "::/$$(basename $$elf)"; \
				echo " -> Injected $$(basename $$elf)"; \
			fi; \
		done; \
		if [ -f "bin/splash.bmp" ]; then \
			mcopy -o -i $(BIN_DIR)/MeowMeowOS.img bin/splash.bmp "::/splash.bmp"; \
			echo " -> Injected splash.bmp"; \
		fi; \
		echo "--- Contents of disk according to mtools: ---"; \
		mdir -i $(BIN_DIR)/MeowMeowOS.img ::; \
		echo "----------------------------------------------"; \
		exit 0; \
	else \
		sudo mkdir -p /mnt/meowos; \
		if sudo mount -t vfat -o loop $(BIN_DIR)/MeowMeowOS.img /mnt/meowos; then \
		for elf in $(USER_ELFS); do \
			if [ -f "$$elf" ]; then \
				sudo cp $$elf /mnt/meowos/$$(basename $$elf); \
				echo " -> Injected $$(basename $$elf)"; \
			fi \
		done; \
		if [ -f "bin/splash.bmp" ]; then \
			sudo cp bin/splash.bmp /mnt/meowos/splash.bmp; \
			echo " -> Injected splash.bmp"; \
		fi; \
		echo "--- Contents of disk according to Linux: ---"; \
		ls -la /mnt/meowos; \
		echo "--------------------------------------------"; \
		sudo sync; \
		sudo umount /mnt/meowos; \
	else \
		echo "WARNING: Mount failed. You need to run 'format' in the OS first."; \
		fi; \
	fi

clean:
	rm -rf $(BUILD_DIR) $(BIN_DIR)

run: $(BIN_DIR)/MeowMeowOS.img
ifeq ($(HOST_OS),Darwin)
	$(QEMU) $(QEMU_FLAGS) $(QEMU_DISPLAY_FLAGS) & qemu_pid=$$!; \
	QEMU_WINDOW_WIDTH=$(QEMU_WINDOW_WIDTH) QEMU_WINDOW_HEIGHT=$(QEMU_WINDOW_HEIGHT) sh $(QEMU_RESIZE_SCRIPT); \
	wait $$qemu_pid
else
	$(QEMU) $(QEMU_FLAGS) $(QEMU_DISPLAY_FLAGS)
endif

debug: $(BIN_DIR)/MeowMeowOS.img
ifeq ($(HOST_OS),Darwin)
	$(QEMU) $(QEMU_FLAGS) $(QEMU_DISPLAY_FLAGS) $(QEMU_DEBUG_FLAGS) | tee MeowMeowOS.log & qemu_pid=$$!; \
	QEMU_WINDOW_WIDTH=$(QEMU_WINDOW_WIDTH) QEMU_WINDOW_HEIGHT=$(QEMU_WINDOW_HEIGHT) sh $(QEMU_RESIZE_SCRIPT); \
	wait $$qemu_pid
else
	$(QEMU) $(QEMU_FLAGS) $(QEMU_DISPLAY_FLAGS) $(QEMU_DEBUG_FLAGS) | tee MeowMeowOS.log
endif

debug-full: $(BIN_DIR)/MeowMeowOS.img
ifeq ($(HOST_OS),Darwin)
	$(QEMU) $(QEMU_FLAGS) $(QEMU_DISPLAY_FLAGS) -serial stdio -d int -D qemu.log | tee MeowMeowOS.log & qemu_pid=$$!; \
	QEMU_WINDOW_WIDTH=$(QEMU_WINDOW_WIDTH) QEMU_WINDOW_HEIGHT=$(QEMU_WINDOW_HEIGHT) sh $(QEMU_RESIZE_SCRIPT); \
	wait $$qemu_pid
else
	$(QEMU) $(QEMU_FLAGS) $(QEMU_DISPLAY_FLAGS) -serial stdio -d int -D qemu.log | tee MeowMeowOS.log
endif

first-run: $(BIN_DIR)/MeowMeowOS.img $(USER_ELFS)
	@echo "First boot: waiting for FAT16 formatting to complete..."
	@rm -f $(FIRST_BOOT_LOG)
	@$(QEMU) $(QEMU_FLAGS) -display none -serial file:$(FIRST_BOOT_LOG) & qemu_pid=$$!; \
	polls=0; max_polls=$$(( $(FIRST_BOOT_TIMEOUT) * 10 )); \
	while ! grep -q "FAT16.*Mounted at LBA" $(FIRST_BOOT_LOG) 2>/dev/null; do \
		if ! kill -0 $$qemu_pid 2>/dev/null; then \
			echo "ERROR: QEMU exited before FAT16 initialization completed."; \
			wait $$qemu_pid 2>/dev/null || true; \
			exit 1; \
		fi; \
		if [ $$polls -ge $$max_polls ]; then \
			echo "ERROR: FAT16 initialization timed out after $(FIRST_BOOT_TIMEOUT) seconds."; \
			kill $$qemu_pid 2>/dev/null || true; \
			wait $$qemu_pid 2>/dev/null || true; \
			exit 1; \
		fi; \
		polls=$$((polls + 1)); \
		sleep 0.1; \
	done; \
	kill $$qemu_pid 2>/dev/null || true; \
	wait $$qemu_pid 2>/dev/null || true
	$(MAKE) inject
	@echo "User programs and splash asset injected. Starting MeowMeowOS..."
	$(MAKE) debug

compile:
	$(MAKE) clean
	$(MAKE) all
	$(MAKE) debug

.PHONY: all clean run debug debug-full first-run compile inject generate_splash