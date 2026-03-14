#!/bin/bash

set -euo pipefail

if [ $# -ne 1 ]; then
    echo "Usage: $0 <platform>" >&2
    exit 1
fi

platform="$1"

# shellcheck source=tests/qemu/common.sh
source "$(cd "$(dirname "$0")" && pwd)/common.sh"

trap restore_profile EXIT
init_log "$platform" online
cd "$PROJECT_ROOT"

case "$platform" in
    esp32c3-qemu)
        setup_esp_idf
        build_target "$platform"
        run_esp_qemu_until_log "$platform" 180 "[boot] AI>" "[boot] AI test failed:"
        ;;
    esp32s3-qemu)
        setup_esp_idf
        build_target "$platform"
        run_esp_qemu_until_log "$platform" 240 "[boot] AI>" "[boot] AI test failed:"
        ;;
    *)
        echo "Unsupported online platform: ${platform}" >&2
        exit 1
        ;;
esac

"${SCRIPT_DIR}/assert-log.sh" "$log_file" \
    "Testing AI connection"

if grep -Fq "[boot] AI>" "$log_file"; then
    echo "Online log: ${log_file}"
    exit 0
fi

if grep -Fq "[boot] AI test failed:" "$log_file"; then
    echo "AI boot test failed during online validation" >&2
    tail -n 200 "$log_file" >&2
    exit 1
fi

echo "Missing AI boot result in online validation log" >&2
tail -n 200 "$log_file" >&2
exit 1
