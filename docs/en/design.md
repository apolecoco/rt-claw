# rt-claw Evolution Design

[中文](../zh/design.md) | **English**

## Positioning

rt-claw = **Hardware-first Real-Time AI Runtime**

Not "AI on MCU" — a real-time hardware control system with AI augmentation.
Core value: expose atomized hardware capabilities (GPIO, sensors, LCD, networking)
as LLM tools. AI orchestrates dynamically; no re-writing, compiling, or flashing
embedded code.

```
+---------------------+     +---------------------+
|  Real-Time HW Ctrl  |     |  AI Decision Making |
|  (deterministic,    |<--->|  (best-effort)      |
|   low latency)      |     |                     |
+---------------------+     +---------------------+
         |                            |
    FreeRTOS task priority       HTTP/HTTPS to cloud
    Direct peripheral access     Tool Use callbacks
```

Key principles:
- **Short chains**: sense → decide → act, at most two layers of indirection
- **Determinism first**: real-time tasks never depend on AI responses
- **Resource respect**: ESP32-C3 has only 240KB usable SRAM — every byte counts

## Event Priority Model

Implemented directly via FreeRTOS task priorities. No extra scheduling framework.

| Layer | Name | Latency | Implementation | Example |
|-------|------|---------|----------------|---------|
| P0 | Reflex | 1-10ms | ISR + highest priority task (prio 24) | GPIO safety, watchdog |
| P1 | Control | 10-50ms | High priority task (prio 20) | Sensor polling, PID |
| P2 | Interaction | 50-150ms | Medium priority task (prio 15) | Shell ACK, LCD, Gateway |
| P3 | AI | Best-effort | Low priority task (prio 5-10) | LLM API call, swarm sync |

Current module priority mapping (`claw_config.h`):

```
Gateway:    prio 15  → P2 Interaction
Scheduler:  prio 10  → P2/P3 boundary
Swarm:      prio 12  → P2 Interaction
Heartbeat:  prio 5   → P3 AI (implicit, triggered by scheduler)
AI Engine:  prio 5   → P3 AI (inherits shell thread priority when called)
```

**No new code needed** — just document priority conventions for new modules.

## Three Execution Paths

```
User Input / Sensor Event
        |
        +--- [Reflex] ---> Local rule, execute directly (no AI)
        |                   e.g. GPIO safety block, over-temp shutdown
        |
        +--- [Interactive] ---> Local ACK + background AI
        |                       e.g. Shell input → print "thinking..."
        |                            → full reply after AI responds
        |
        +--- [Cognitive] ---> Pure AI-driven (multi-turn Tool Use)
                              e.g. heartbeat patrol, Feishu message response
```

**Currently implemented:** Interactive path (shell thinking animation),
Cognitive path (heartbeat, Feishu).
**To implement:** Reflex path (GPIO safety policy).

## Capability Model

Five layers mapped to existing modules:

```
+----------------------------------------------------------+
|  Cognition                                                |
|  ai_engine + tools + heartbeat                           |
|  Multi-turn conversation, Tool Use, scheduled patrol     |
+----------------------------------------------------------+
|  Reflex                                                   |
|  GPIO safety policy + scheduler                          |
|  Local rule engine, safety interception, timed automation |
+----------------------------------------------------------+
|  Expression                                               |
|  LCD + Shell + Feishu                                    |
|  Text, graphics, IM message output                       |
+----------------------------------------------------------+
|  Action                                                   |
|  tools/gpio + tools/sched + tools/net                    |
|  GPIO ops, scheduled tasks, HTTP requests                |
+----------------------------------------------------------+
|  Perception                                               |
|  WiFi scan + swarm discovery + sensors (future)          |
|  Environment sensing, node discovery, data collection    |
+----------------------------------------------------------+
```

## Designs Adopted from MimiClaw

The following patterns were analyzed and validated for adoption.
Each item includes implementation location and estimated effort.

### 1. GPIO Safety Policy

MimiClaw's three-layer defense: CSV allowlist → hardware-reserved pin
blocking → platform differentiation → human-readable rejection hints.

**rt-claw approach:**

```c
/* tools/tool_gpio.c — static allowlist, determined at compile time */
static const struct {
    int pin;
    uint32_t allowed_modes;  /* bitmask: INPUT / OUTPUT / ADC */
    const char *label;       /* human-readable, used in AI rejection */
} s_gpio_policy[] = {
    { 4,  GPIO_MODE_OUTPUT, "LED" },
    { 5,  GPIO_MODE_INPUT,  "Button" },
    /* ... platform-specific entries via #ifdef */
};
```

**Effort:** ~50 lines. Modify `claw/tools/tool_gpio.c`.

### 2. Three-Layer Memory

| Layer | Storage | Capacity | Purpose |
|-------|---------|----------|---------|
| Session | RAM ring buffer | 20 msgs (`AI_MEMORY_MAX_MSGS`) | Current conversation |
| Long-term | NVS Flash | ~4KB | User preferences, key facts |
| Journal | NVS Flash (optional) | ~2KB circular | Daily event summaries |

Journal layer is an optional enhancement — when heartbeat finds notable events,
append a summary line to NVS. Read recent logs during heartbeat patrol.

**Effort:** ~80 lines. Extend `claw/services/ai/ai_memory.c`.

### 3. Scheduler Persistence

```c
/* core/scheduler.c — NVS persistence */
/* sched_add() writes to NVS: "sched_0" = "name|interval|count|remaining" */
/* sched_init() scans NVS to restore (only AI-created tasks) */
/* Limitation: callbacks not serializable; persistent tasks use sched_ai_callback() */
/*             i.e. on reboot, scheduled tasks re-trigger AI call, not original callback */
```

**Effort:** ~60 lines. Modify `claw/core/scheduler.c`.

### 4. Dual-Core Binding (ESP32-S3)

```
Core 0: WiFi + TLS + HTTP (ESP-IDF default)
Core 1: Shell + AI Engine + Tools + Scheduler

/* Implementation: sdkconfig.defaults configuration only */
CONFIG_ESP_MAIN_TASK_CORE_AFFINITY_CPU1=y
```

**Effort:** Config change only, 0 lines of code.

### 5. Multi-LLM Provider

No provider abstraction layer (resource waste). Two approaches:

- **Option A (recommended):** Server-side proxy normalizes API format.
  Device only implements Claude Messages API.
- **Option B (direct):** Compile-time `#ifdef CONFIG_RTCLAW_API_FORMAT_OPENAI`
  for OpenAI-compatible format. ~100 lines in `ai_engine.c`.

### 6. Context Injection

Enrich system prompt with device state at each AI call:

```
[Device Context]
- Platform: ESP32-S3, WiFi: connected (192.168.1.100)
- Uptime: 3h 42m, Free heap: 156KB
- Scheduled tasks: 2 active
```

**Effort:** ~30 lines. Modify `claw/services/ai/ai_engine.c`.

## Module Evolution Map

| Module | Current Role | Evolution |
|--------|-------------|-----------|
| `gateway` | Message queue skeleton | Inter-node routing (when multi-node ready) |
| `scheduler` | Timed callbacks | + NVS persistence + deadline awareness |
| `ai_engine` | Claude API client | + context injection + multi-format (optional) |
| `tools/gpio` | GPIO read/write | + safety policy allowlist |
| `tools/sched` | AI-created tasks | + persistence recovery |
| `heartbeat` | Periodic AI check-in | + event log aggregation |
| `swarm` | UDP heartbeat discovery | + capability broadcast + task dispatch |

## Resource Budget

### ESP32-C3 (240KB usable SRAM)

| Module | Current | Added | Notes |
|--------|---------|-------|-------|
| ESP-IDF + WiFi + TLS | ~160KB | — | Fixed system overhead |
| Gateway + Scheduler | ~12KB | +1KB | NVS persistence buffer |
| AI Engine + Memory | ~15KB | +2KB | Context injection string |
| Tools | ~4KB | +1KB | GPIO safety policy table |
| Swarm + Heartbeat | ~14KB | — | No change |
| Shell + App | ~10KB | — | No change |
| **Total** | **~215KB** | **+4KB** | ~21KB headroom |

### ESP32-S3 (8MB PSRAM)

PSRAM handles TLS buffers and large allocations
(`SPIRAM_MALLOC_ALWAYSINTERNAL=2048`).
Internal SRAM budget similar to C3; PSRAM provides ample headroom.

## Implementation Roadmap

Ordered by value / effort ratio. Each item is independent.

| Phase | Content | Effort | Value |
|-------|---------|--------|-------|
| 1 | GPIO safety policy | ~50 lines | Prevent AI from damaging hardware pins |
| 2 | Context injection | ~30 lines | AI-aware of device state, more accurate |
| 3 | Dual-core binding (S3) | Config only | Isolate network from app, reduce jitter |
| 4 | Scheduler persistence | ~60 lines | Restore AI automations across reboots |
| 5 | Event journal layer | ~80 lines | Heartbeat patrol gets historical context |
| 6 | Multi-LLM format (opt.) | ~100 lines | Direct connect to OpenAI-compatible APIs |

**Total: ~320 lines of code changes** covering 80% of the design evolution goals.

## What We Won't Build

Explicitly listed to prevent over-engineering:

- **No event bus**: FreeRTOS message queues + task priorities are sufficient
- **No provider abstraction layer**: Compile-time selection or proxy is better for constrained environments
- **No filesystem**: NVS key-value pairs cover all persistence needs
- **No complex middleware**: Sense → decide → act, two layers of indirection max
- **No dynamic loading**: Static linking on MCU; features controlled by Kconfig toggles
