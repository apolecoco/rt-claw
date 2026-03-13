# rt-claw unified build entry
# Usage: make <platform>

PROJECT_ROOT := $(shell pwd)
BUILD_DIR    := $(PROJECT_ROOT)/build

.PHONY: help
help:
	@echo "rt-claw build system"
	@echo ""
	@echo "Targets:"
	@echo "  make vexpress-a9-qemu          Build for QEMU vexpress-a9 (RT-Thread)"
	@echo "  make esp32c3-qemu     Build for ESP32-C3 QEMU (ESP-IDF + FreeRTOS)"
	@echo "  make esp32s3-qemu     Build for ESP32-S3 QEMU (ESP-IDF + FreeRTOS)"
	@echo "  make run-vexpress-a9-qemu      Run RT-Thread on QEMU"
	@echo "  make run-esp32c3-qemu Run ESP32-C3 on QEMU"
	@echo "  make run-esp32s3-qemu Run ESP32-S3 on QEMU"
	@echo "  make clean            Clean all build artifacts"
	@echo "  make check            Run code style checks"
	@echo ""
	@echo "Build output: $(BUILD_DIR)/<platform>/"

# --- QEMU vexpress-a9 (RT-Thread) ---
MESON_BUILDDIR_A9 := $(BUILD_DIR)/vexpress-a9-qemu
CROSS_FILE_A9     := platform/vexpress-a9-qemu/cross.ini

.PHONY: vexpress-a9-qemu
vexpress-a9-qemu:
	@if [ ! -f $(MESON_BUILDDIR_A9)/build.ninja ]; then \
		meson setup $(MESON_BUILDDIR_A9) --cross-file $(CROSS_FILE_A9); \
	fi
	meson compile -C $(MESON_BUILDDIR_A9)
	cd platform/vexpress-a9-qemu && scons -j$$(nproc)
	@cp -f platform/vexpress-a9-qemu/rtthread.elf $(MESON_BUILDDIR_A9)/
	@cp -f platform/vexpress-a9-qemu/rtthread.bin $(MESON_BUILDDIR_A9)/
	@cp -f platform/vexpress-a9-qemu/rtthread.map $(MESON_BUILDDIR_A9)/
	@echo "Output: $(MESON_BUILDDIR_A9)/"

.PHONY: run-vexpress-a9-qemu
run-vexpress-a9-qemu: vexpress-a9-qemu
	tools/qemu-run.sh -M vexpress-a9-qemu

# --- ESP32-C3 QEMU (ESP-IDF) ---
# Prerequisite: source $$HOME/esp/esp-idf/export.sh
MESON_BUILDDIR_C3  := $(BUILD_DIR)/esp32c3-qemu
CROSS_FILE_C3      := platform/esp32c3-qemu/cross.ini
ESP_C3_PLATFORM    := platform/esp32c3-qemu

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
	tools/qemu-run.sh -M esp32c3-qemu

# --- ESP32-S3 QEMU (ESP-IDF) ---
# Prerequisite: source $$HOME/esp/esp-idf/export.sh
MESON_BUILDDIR_S3  := $(BUILD_DIR)/esp32s3-qemu
CROSS_FILE_S3      := platform/esp32s3-qemu/cross.ini
ESP_S3_PLATFORM    := platform/esp32s3-qemu

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
	tools/qemu-run.sh -M esp32s3-qemu

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
