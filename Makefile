# rt-claw unified build entry
# Usage: make <platform>
#
# Run targets accept optional variables:
#   GDB=1       Enable GDB server (debug mode, port 1234)
#   GRAPHICS=1  Enable LCD display window (ESP32 QEMU only)

PROJECT_ROOT := $(shell pwd)
BUILD_DIR    := $(PROJECT_ROOT)/build

GDB      ?= 0
GRAPHICS ?= 0

.PHONY: help
help:
	@echo "rt-claw build system"
	@echo ""
	@echo "Build:"
	@echo "  make vexpress-a9-qemu      Build for QEMU vexpress-a9 (RT-Thread)"
	@echo "  make esp32c3-qemu          Build for ESP32-C3 QEMU (ESP-IDF)"
	@echo "  make esp32s3-qemu          Build for ESP32-S3 QEMU (ESP-IDF)"
	@echo ""
	@echo "Run (build + launch QEMU):"
	@echo "  make run-vexpress-a9-qemu"
	@echo "  make run-esp32c3-qemu"
	@echo "  make run-esp32s3-qemu"
	@echo ""
	@echo "Run options (pass as variables):"
	@echo "  make run-esp32c3-qemu GDB=1        Debug mode (GDB port 1234)"
	@echo "  make run-esp32c3-qemu GRAPHICS=1   LCD display window"
	@echo ""
	@echo "Clean:"
	@echo "  make clean                 Clean all build artifacts"
	@echo "  make clean-<platform>      Clean specific platform"
	@echo ""
	@echo "Checks:"
	@echo "  make check                 Run code style checks"

# --- QEMU vexpress-a9 (RT-Thread) ---
MESON_BUILDDIR_A9 := $(BUILD_DIR)/vexpress-a9-qemu
CROSS_FILE_A9     := platform/vexpress-a9-qemu/cross.ini
A9_PLATFORM       := platform/vexpress-a9-qemu

.PHONY: vexpress-a9-qemu
vexpress-a9-qemu:
	@if [ ! -f $(MESON_BUILDDIR_A9)/build.ninja ]; then \
		meson setup $(MESON_BUILDDIR_A9) --cross-file $(CROSS_FILE_A9); \
	fi
	meson compile -C $(MESON_BUILDDIR_A9)
	cd $(A9_PLATFORM) && scons -j$$(nproc)
	@cp -f $(A9_PLATFORM)/rtthread.elf $(MESON_BUILDDIR_A9)/
	@cp -f $(A9_PLATFORM)/rtthread.bin $(MESON_BUILDDIR_A9)/
	@cp -f $(A9_PLATFORM)/rtthread.map $(MESON_BUILDDIR_A9)/
	@echo "Output: $(MESON_BUILDDIR_A9)/"

.PHONY: run-vexpress-a9-qemu
run-vexpress-a9-qemu: vexpress-a9-qemu
	@if [ ! -f $(A9_PLATFORM)/sd.bin ]; then \
		echo "Creating SD card image..."; \
		dd if=/dev/zero of=$(A9_PLATFORM)/sd.bin bs=1024 count=65536; \
	fi
	@if [ "$(GDB)" = "1" ]; then \
		echo "Starting QEMU in debug mode (GDB port 1234)..."; \
		echo "Connect: arm-none-eabi-gdb $(MESON_BUILDDIR_A9)/rtthread.elf -ex 'target remote :1234'"; \
	fi
	qemu-system-arm --version
	qemu-system-arm \
		-M vexpress-a9 \
		-smp cpus=1 \
		-kernel $(MESON_BUILDDIR_A9)/rtthread.bin \
		-nographic \
		-sd $(A9_PLATFORM)/sd.bin \
		-nic user,model=lan9118 \
		$(if $(filter 1,$(GDB)),-S -s)

# --- ESP32-C3 QEMU (ESP-IDF) ---
# Prerequisite: source $$HOME/esp/esp-idf/export.sh
MESON_BUILDDIR_C3 := $(BUILD_DIR)/esp32c3-qemu
CROSS_FILE_C3     := platform/esp32c3-qemu/cross.ini
ESP_C3_PLATFORM   := platform/esp32c3-qemu

.PHONY: esp32c3-qemu
esp32c3-qemu:
	@if [ ! -f $(ESP_C3_PLATFORM)/sdkconfig ]; then \
		rm -rf $(ESP_C3_PLATFORM)/build; \
		cd $(ESP_C3_PLATFORM) && idf.py set-target esp32c3; \
	fi
	@if [ ! -f $(ESP_C3_PLATFORM)/build/compile_commands.json ]; then \
		cd $(ESP_C3_PLATFORM) && idf.py reconfigure; \
	fi
	python3 scripts/gen-esp32c3-cross.py
	@if [ ! -f $(MESON_BUILDDIR_C3)/build.ninja ]; then \
		meson setup $(MESON_BUILDDIR_C3) --cross-file $(CROSS_FILE_C3); \
	fi
	meson compile -C $(MESON_BUILDDIR_C3)
	cd $(ESP_C3_PLATFORM) && idf.py reconfigure && idf.py build
	@echo "Output: $(ESP_C3_PLATFORM)/build/rt-claw.bin"

.PHONY: run-esp32c3-qemu
run-esp32c3-qemu: esp32c3-qemu
	@echo ">>> Generating merged flash image ..."
	cd $(ESP_C3_PLATFORM)/build && esptool.py --chip esp32c3 merge_bin \
		--fill-flash-size 4MB -o flash_image.bin @flash_args
	@if [ "$(GDB)" = "1" ]; then \
		echo "Starting QEMU in debug mode (GDB port 1234)..."; \
		echo "Connect: riscv32-esp-elf-gdb $(ESP_C3_PLATFORM)/build/rt-claw.elf -ex 'target remote :1234'"; \
	fi
	@echo ">>> Starting QEMU (ESP32-C3, icount=1) ..."
	qemu-system-riscv32 \
		$(if $(filter 1,$(GRAPHICS)),,-nographic) \
		-icount 1 \
		-machine esp32c3 \
		-drive file=$(ESP_C3_PLATFORM)/build/flash_image.bin,if=mtd,format=raw \
		-global driver=timer.esp32c3.timg,property=wdt_disable,value=true \
		-nic user,model=open_eth \
		$(if $(filter 1,$(GDB)),-S -s)

# --- ESP32-S3 QEMU (ESP-IDF) ---
# Prerequisite: source $$HOME/esp/esp-idf/export.sh
MESON_BUILDDIR_S3 := $(BUILD_DIR)/esp32s3-qemu
CROSS_FILE_S3     := platform/esp32s3-qemu/cross.ini
ESP_S3_PLATFORM   := platform/esp32s3-qemu

.PHONY: esp32s3-qemu
esp32s3-qemu:
	@if [ ! -f $(ESP_S3_PLATFORM)/sdkconfig ]; then \
		rm -rf $(ESP_S3_PLATFORM)/build; \
		cd $(ESP_S3_PLATFORM) && idf.py set-target esp32s3; \
	fi
	@if [ ! -f $(ESP_S3_PLATFORM)/build/compile_commands.json ]; then \
		cd $(ESP_S3_PLATFORM) && idf.py reconfigure; \
	fi
	python3 scripts/gen-esp32s3-cross.py
	@if [ ! -f $(MESON_BUILDDIR_S3)/build.ninja ]; then \
		meson setup $(MESON_BUILDDIR_S3) --cross-file $(CROSS_FILE_S3); \
	fi
	meson compile -C $(MESON_BUILDDIR_S3)
	cd $(ESP_S3_PLATFORM) && idf.py reconfigure && idf.py build
	@echo "Output: $(ESP_S3_PLATFORM)/build/rt-claw.bin"

.PHONY: run-esp32s3-qemu
run-esp32s3-qemu: esp32s3-qemu
	@echo ">>> Generating merged flash image ..."
	cd $(ESP_S3_PLATFORM)/build && esptool.py --chip esp32s3 merge_bin \
		--fill-flash-size 4MB -o flash_image.bin @flash_args
	@if [ "$(GDB)" = "1" ]; then \
		echo "Starting QEMU in debug mode (GDB port 1234)..."; \
		echo "Connect: xtensa-esp32s3-elf-gdb $(ESP_S3_PLATFORM)/build/rt-claw.elf -ex 'target remote :1234'"; \
	fi
	@echo ">>> Starting QEMU (ESP32-S3, icount=3) ..."
	qemu-system-xtensa \
		$(if $(filter 1,$(GRAPHICS)),,-nographic) \
		-icount 3 \
		-machine esp32s3 \
		-drive file=$(ESP_S3_PLATFORM)/build/flash_image.bin,if=mtd,format=raw \
		-global driver=timer.esp32s3.timg,property=wdt_disable,value=true \
		-nic user,model=open_eth \
		$(if $(filter 1,$(GDB)),-S -s)

# --- Clean ---
.PHONY: clean
clean:
	rm -rf $(BUILD_DIR)
	cd platform/vexpress-a9-qemu && scons -c 2>/dev/null || true

.PHONY: clean-vexpress-a9-qemu
clean-vexpress-a9-qemu:
	rm -rf $(BUILD_DIR)/vexpress-a9-qemu
	cd platform/vexpress-a9-qemu && scons -c 2>/dev/null || true

.PHONY: clean-esp32c3-qemu
clean-esp32c3-qemu:
	rm -rf $(BUILD_DIR)/esp32c3-qemu
	rm -f platform/esp32c3-qemu/cross.ini

.PHONY: clean-esp32s3-qemu
clean-esp32s3-qemu:
	rm -rf $(BUILD_DIR)/esp32s3-qemu
	rm -f platform/esp32s3-qemu/cross.ini

# --- Checks ---
.PHONY: check
check:
	scripts/check-patch.sh

.PHONY: check-staged
check-staged:
	scripts/check-patch.sh --staged
