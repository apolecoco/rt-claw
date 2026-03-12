# CLAUDE.md

rt-claw: OpenClaw-inspired AI assistant on embedded RTOS (FreeRTOS + RT-Thread) via OSAL.

## Build

```bash
# ESP32-C3 (ESP-IDF + FreeRTOS)
source $HOME/esp/esp-idf/export.sh
cd platform/esp32c3
idf.py set-target esp32c3 && idf.py build

# QEMU vexpress-a9 (RT-Thread)
cd platform/qemu-a9-rtthread
scons -j$(nproc)
```

## Run

```bash
# ESP32-C3 QEMU
idf.py qemu monitor                    # serial only
idf.py qemu --graphics monitor         # with LCD

# RT-Thread QEMU
tools/qemu-run.sh
```

## Code Style

- C99 (gnu99), 4-space indent, ~80 char line width (max 100)
- Naming: `snake_case` for variables/functions, `CamelCase` for structs/enums, `UPPER_CASE` for constants/enum values
- Public API prefix: `claw_` (OSAL), subsystem prefix for services (e.g. `gateway_`)
- Comments: `/* C style only */`, no `//`
- Header guards: `CLAW_<PATH>_<NAME>_H`
- Include order (src/): `claw_os.h` -> system headers -> project headers
- Always use braces for control flow blocks
- License header on every source file: `SPDX-License-Identifier: MIT`

Full reference: [docs/en/coding-style.md](docs/en/coding-style.md)

## Commit Convention

Format: `subsystem: short description` (max 76 chars), body wrapped at 76 chars.

Every commit **must** include `Signed-off-by` (`git commit -s`).

Subsystem prefixes: `osal`, `gateway`, `swarm`, `net`, `ai`, `platform`, `build`, `docs`, `tools`, `scripts`, `main`.

## Checks

```bash
# Style check (src/ and osal/ only)
scripts/check-patch.sh                 # all source files
scripts/check-patch.sh --staged        # staged changes only
scripts/check-patch.sh --file <path>   # specific file

# DCO (Signed-off-by) check
scripts/check-dco.sh                   # commits since main
scripts/check-dco.sh --last 3          # last 3 commits

# Install git hooks (pre-commit + commit-msg)
scripts/install-hooks.sh
```

## Testing

No unit test framework yet. Verify changes by:

1. Build passes on at least one platform
2. `scripts/check-patch.sh --staged` passes
3. QEMU boot test: `idf.py qemu monitor` or `tools/qemu-run.sh`

## Key Paths

| Path | Purpose |
|------|---------|
| `osal/include/claw_os.h` | OSAL API (the only header core code includes) |
| `osal/freertos/` | FreeRTOS OSAL implementation |
| `osal/rtthread/` | RT-Thread OSAL implementation |
| `src/claw_init.c` | Boot entry point |
| `src/claw_config.h` | Compile-time configuration (`CLAW_<SUBSYS>_<PARAM>`) |
| `src/core/gateway.*` | Message router |
| `src/services/{ai,net,swarm}/` | Service modules |
| `src/tools/` | Tool Use framework |
| `platform/esp32c3/` | ESP-IDF project |
| `platform/qemu-a9-rtthread/` | RT-Thread BSP |
