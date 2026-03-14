# rt-claw Architecture

**English** | [中文](../zh/architecture.md)

## Overview

rt-claw brings the OpenClaw personal assistant concept to embedded RTOS devices.
Each rt-claw node is a lightweight, real-time capable unit that can operate
standalone or join a swarm of nodes for distributed intelligence.

Multi-RTOS support through OSAL (OS Abstraction Layer) — same core code runs
on FreeRTOS (ESP-IDF) and RT-Thread with zero modification.

## OSAL — OS Abstraction Layer

The key architectural decision: all rt-claw core code depends only on `osal/claw_os.h`.

```
claw/*.c  --->  #include "osal/claw_os.h"  (compile-time interface)
                        |
          +-------------+-------------+
          |                           |
  claw_os_freertos.c          claw_os_rtthread.c
  (linked on ESP-IDF)         (linked on RT-Thread)
```

Abstracted primitives:
- Thread, Mutex, Semaphore, Message Queue, Timer
- Memory allocation (malloc/free)
- Logging (CLAW_LOGI/LOGW/LOGE/LOGD)
- Tick / time

Design: interface header + per-RTOS implementation file (link-time binding).
Zero runtime overhead. No function pointers. No conditional compilation in core code.

## Core Services

### Gateway (claw/core/gateway)

Inter-node message routing skeleton for swarm communication.

- Thread-safe message queue (via `claw_mq_*`)
- Message types: DATA, CMD, EVENT, SWARM
- Routing logic not yet implemented — messages are logged only

### Swarm Service (claw/services/swarm)

Node discovery and coordination for building a mesh of rt-claw devices.

- UDP broadcast discovery on local network
- ESP-NOW for ultra-low-latency P2P (ESP32 platforms)
- Heartbeat-based liveness detection
- Capability advertisement
- Task distribution across the swarm

### Network Service (claw/services/net)

Platform-aware networking:

- ESP32-C3: WiFi (802.11 b/g/n) + MQTT + mbedTLS
- QEMU-A9: Ethernet (smc911x) + lwIP + MQTT
- Common: MQTT topic-based channel system (maps to OpenClaw's multi-channel)

### AI Engine (claw/services/ai)

LLM API client with Tool Use support:

- Claude API integration with streaming HTTP requests
- Tool Use: LLM-driven hardware control via function calling; 30+ built-in tools covering GPIO, system info, LCD, audio, scheduler, HTTP requests, and long-term memory
- Conversation memory (short-term RAM ring buffer + long-term NVS storage)
- Skill system: predefined and AI-created reusable prompt templates
- HTTP/HTTPS transport (ESP-IDF uses esp_http_client with TLS; RT-Thread uses BSD sockets via API proxy)

### Scheduler (claw/core/scheduler)

Timer-driven task execution:

- Up to 8 concurrent scheduled tasks with 1-second tick resolution
- AI can create, list, and remove tasks via tool calls
- Supports one-shot and repeating tasks

### IM Service (claw/services/im)

Instant messaging integrations:

- Feishu (Lark): WebSocket long connection, no public IP required
- Event subscription: `im.message.receive_v1`
- Planned: DingTalk, QQ, Telegram

### Shell (claw/shell)

UART REPL with chat-first design:

- Direct text input goes to AI engine
- `/commands` for system operations (14 built-in commands)
- Insert-mode editing with tab completion
- UTF-8 support

## Drivers

Linux-kernel style hardware driver layer: `drivers/<subsystem>/<vendor>/`.
Public headers mirror the structure under `include/drivers/`.

| Driver | Path | Description |
|--------|------|-------------|
| WiFi Manager | `drivers/net/espressif/` | ESP32 WiFi STA management (shared C3/S3) |
| ES8311 Audio | `drivers/audio/espressif/` | I2C audio codec with preset sound effects |
| SSD1306 OLED | `drivers/display/espressif/` | I2C OLED display (128x64) |
| Console | `drivers/serial/espressif/` | Serial console driver |

## Platforms

### ESP32-C3 (platform/esp32c3/)

- CPU: RISC-V 32-bit (rv32imc), 160MHz
- RAM: 400KB SRAM (~240KB available for app)
- WiFi: 802.11 b/g/n
- BLE: Bluetooth 5.0 LE
- RTOS: ESP-IDF + FreeRTOS
- Build: Meson (cross-compile) + CMake/idf.py (link + flash)
- QEMU: Espressif fork (qemu-riscv32), UART only (no WiFi sim)

### ESP32-S3 (platform/esp32s3/)

- CPU: Xtensa LX7 (dual-core), 240MHz
- RAM: 512KB SRAM + 8MB PSRAM (real hardware)
- WiFi: 802.11 b/g/n
- BLE: Bluetooth 5.0 LE
- RTOS: ESP-IDF + FreeRTOS
- Build: Meson (cross-compile) + CMake/idf.py (link + flash)
- QEMU: Espressif fork (qemu-system-xtensa), OpenCores Ethernet (no WiFi sim)
- Boards: qemu (4MB), default (16MB + 8MB PSRAM real hardware)

### vexpress-a9 QEMU (platform/vexpress-a9/)

- CPU: ARM Cortex-A9 (dual-core)
- RTOS: RT-Thread
- Build: Meson (cross-compile) + SCons (link)
- Peripherals: UART, Ethernet, LCD, SD card

## Communication Flow

```
External (MQTT/HTTP)
       |
       v
  +---------+
  | net_svc |
  +----+----+
       |
       v
  +---------+     +---------+
  | gateway |---->| swarm   |---- other rt-claw nodes
  +----+----+     +---------+
       |
       v
  +---------+
  |ai_engine|
  +---------+
```

## ESP32-C3 Resource Budget

| Module | SRAM | Notes |
|--------|------|-------|
| ESP-IDF + WiFi + TLS | ~160KB | System overhead |
| Gateway | ~8KB | MQ 16x256B + thread |
| Swarm | ~12KB | 32 nodes + ESP-NOW |
| Net (MQTT) | ~25KB | Client + buffers |
| AI Engine | ~15KB | LLM client + conversation memory |
| App + CLI | ~10KB | Main + shell |
| **Total** | **~230KB** | ~170KB headroom |
