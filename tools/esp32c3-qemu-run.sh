#!/bin/bash
# Launch rt-claw on ESP32-C3 QEMU (Espressif fork)
#
# Usage:
#   ./esp32c3-qemu-run.sh               - serial only (default)
#   ./esp32c3-qemu-run.sh --graphics    - with LCD display window
#
# Prerequisites:
#   1. ESP-IDF installed and sourced (. $IDF_PATH/export.sh)
#   2. Espressif QEMU installed:
#      python $IDF_PATH/tools/idf_tools.py install qemu-riscv32
#   3. Project built: ./tools/esp32c3-build.sh

set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PLATFORM_DIR="$(dirname "$SCRIPT_DIR")/platform/esp32c3"

cd "$PLATFORM_DIR" || exit 1

if [ -z "$IDF_PATH" ]; then
    echo "Error: IDF_PATH not set. Source ESP-IDF first:"
    echo "  source \$HOME/esp/esp-idf/export.sh"
    exit 1
fi

if [ ! -d "build" ]; then
    echo "Error: build/ not found. Build first:"
    echo "  ./tools/esp32c3-build.sh"
    exit 1
fi

FLASH_SIZE="4MB"
FLASH_IMAGE="build/flash_image.bin"

echo ">>> Generating merged flash image ..."
esptool.py --chip esp32c3 merge_bin \
    --fill-flash-size "$FLASH_SIZE" \
    -o "$FLASH_IMAGE" \
    @build/flash_args

DISPLAY_FLAG="-nographic"
if [ "$1" = "--graphics" ]; then
    DISPLAY_FLAG=""
fi

echo ">>> Starting QEMU (ESP32-C3, icount=1) ..."
exec qemu-system-riscv32 $DISPLAY_FLAG \
    -icount 1 \
    -machine esp32c3 \
    -drive "file=$FLASH_IMAGE,if=mtd,format=raw" \
    -global driver=timer.esp32c3.timg,property=wdt_disable,value=true \
    -nic user,model=open_eth
