

ROOT_DIR = ..
QEMU = qemu-system-x86_64
QEMU_FLAGS = -drive format=raw,file=$(ROOT_DIR)/bin/MeowMeowOS.img -m 512M -accel tcg,thread=single -vga std
QEMU_DISPLAY_FLAGS = -display gtk,zoom-to-fit=on

# 1. Compile everything by deferring to the main Makefile
all:
	@echo "[INFO] Building MeowMeowOS via root Makefile..."
	@$(MAKE) -C $(ROOT_DIR) all

check-env:
	@echo "========================================"
	@echo "      WSLg DISPLAY DIAGNOSTICS          "
	@echo "========================================"
	@echo "WAYLAND_DISPLAY : $$WAYLAND_DISPLAY"
	@echo "DISPLAY         : $$DISPLAY"
	@echo "XDG_RUNTIME_DIR : $$XDG_RUNTIME_DIR"
	@echo "========================================"
	@if [ -z "$$WAYLAND_DISPLAY" ] && [ -z "$$DISPLAY" ]; then \
		echo "[ERROR] No WSLg display variables found!"; \
		echo "[ERROR] If you are using VSCode Remote, your terminal might not have GUI forwarding enabled."; \
		echo "[ERROR] Try opening a standard Windows Ubuntu terminal app and running this."; \
		exit 1; \
	else \
		echo "[OK] WSLg Display socket detected."; \
	fi

run: check-env all
	@echo "[INFO] Launching QEMU..."
	@echo "[WARN] Check your Windows Taskbar! The QEMU window may open in the background and will NOT steal focus."
	$(QEMU) $(QEMU_FLAGS) $(QEMU_DISPLAY_FLAGS)

debug: check-env all
	@echo "[INFO] Launching QEMU in Debug Mode..."
	@echo "[WARN] Check your Windows Taskbar for the QEMU window."
	$(QEMU) $(QEMU_FLAGS) $(QEMU_DISPLAY_FLAGS) -serial stdio | tee $(ROOT_DIR)/MeowMeowOS.log

first-run:
	@echo "[INFO] Compiling OS..."
	@$(MAKE) -C $(ROOT_DIR) all
	@echo "[INFO] Formatting FAT16 (Headless boot)..."
	@$(QEMU) $(QEMU_FLAGS) -display none -serial file:$(ROOT_DIR)/build/first-boot.log & qemu_pid=$$!; \
	while ! grep -q "FAT16.*Mounted at LBA" $(ROOT_DIR)/build/first-boot.log 2>/dev/null; do sleep 0.1; done; \
	kill $$qemu_pid 2>/dev/null || true
	@echo "[INFO] FAT16 Formatted. Injecting user programs..."
	@$(MAKE) -C $(ROOT_DIR) inject
	@echo "[INFO] Injection complete. Starting graphical OS..."
	$(MAKE) run