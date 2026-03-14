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
- **Swarm amplification**: one node is limited; a swarm of nodes is boundless

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
|  LCD + OLED + Shell + Feishu + Audio (ES8311)            |
|  Text, graphics, IM message, audio output                |
+----------------------------------------------------------+
|  Action                                                   |
|  tools/gpio + tools/sched + tools/net                    |
|  GPIO ops, scheduled tasks, HTTP requests                |
+----------------------------------------------------------+
|  Perception                                               |
|  WiFi scan + swarm discovery + sensors (future)          |
|  Environment sensing, node discovery, data collection    |
+----------------------------------------------------------+
+----------------------------------------------------------+
```

## Swarm Intelligence — Core Differentiator

**A single node's resources are limited; a swarm's resources are boundless.**

One ESP32-C3 has 240KB SRAM, one set of GPIO pins, one WiFi antenna.
But 10 rt-claw nodes networked together provide:
- 10x GPIO pins distributed across physical locations
- 10x sensor coverage spanning a larger physical space
- 10x parallel AI call capacity for faster responses
- Capability sharing — Node A has LCD, Node B has temperature sensor;
  AI can read temperature from B and display it on A's screen

This is the **core differentiator** between rt-claw and single-device solutions
like MimiClaw.

### Current State

Implemented swarm infrastructure:

```
Node A                          Node B
+--------+    UDP broadcast     +--------+
| swarm  |<------ 5300 ------->| swarm  |
| discov.|    heartbeat 5s     | discov.|
+--------+                     +--------+
    |  join/leave event             |
    v                               v
+----------+                   +----------+
| heartbeat|                   | heartbeat|
| event agg|                   | event agg|
+----------+                   +----------+
```

- UDP broadcast discovery (port 5300, interval 5s)
- 16-byte compact heartbeat (magic + node_id + uptime + capabilities + load + port)
- Node state tracking (OFFLINE / DISCOVERING / ONLINE)
- 15-second timeout detection
- Node join/leave events pushed to heartbeat

### Evolution Roadmap

#### Step 1: Capability Bitmap Broadcast

The heartbeat packet already has a `capabilities` field (currently fixed at 0).
Populate it with real capability bits.

```c
/* services/swarm/swarm.c — capability bit definitions */
#define SWARM_CAP_GPIO      (1 << 0)  /* controllable GPIO */
#define SWARM_CAP_LCD       (1 << 1)  /* LCD display */
#define SWARM_CAP_SENSOR    (1 << 2)  /* sensors attached */
#define SWARM_CAP_CAMERA    (1 << 3)  /* camera */
#define SWARM_CAP_SPEAKER   (1 << 4)  /* audio output */
#define SWARM_CAP_AI        (1 << 5)  /* AI engine available */
#define SWARM_CAP_INTERNET  (1 << 6)  /* internet access */

/* In heartbeat_send() */
hb.capabilities = SWARM_CAP_GPIO | SWARM_CAP_AI | SWARM_CAP_INTERNET;
/* Actual value determined by platform / Kconfig */
```

**Effort:** ~20 lines. After discovery, any node can query "who has LCD" or
"who has sensors".

#### Step 2: Remote Tool Invocation (Claw Skill Provider)

Core idea: **one node's tools are the entire swarm's tools**.

```
Node A (has LCD)                   Node B (no LCD)
+-----------+                      +-----------+
| tool:     |    GW_MSG_SWARM      | AI Engine |
| lcd_draw  |<-----[RPC req]-------| "draw a   |
|           |------[RPC resp]----->|  circle"  |
+-----------+                      +-----------+
```

Implementation: reuse Gateway's `GW_MSG_SWARM` message type + existing UDP channel.

```c
/* Message format (fits in gateway_msg's 256-byte payload) */
struct swarm_rpc_msg {
    uint32_t src_node;          /* requester */
    uint32_t dst_node;          /* executor */
    uint16_t seq;               /* sequence number */
    uint8_t  type;              /* 0=request, 1=response */
    char     tool_name[32];     /* tool name */
    char     params_json[187];  /* input params JSON */
};

/* Node B's AI finds no local lcd_draw → queries swarm →
   Node A has SWARM_CAP_LCD → sends RPC → Node A executes → returns result */
```

Tool lookup logic (transparent to AI):
1. Try local `claw_tool_find(name)` → found: execute locally
2. Not found → query swarm capability table → found: send RPC
3. Neither → return "tool not available"

**Effort:** ~150 lines. Modify `claw/tools/` + `claw/services/swarm/`.

#### Step 3: Distributed Perception Aggregation

Multi-node sensor data converges to one node for decision making.

```
Node C (temperature sensor)  --heartbeat_post--> Node A (AI decision hub)
Node D (humidity sensor)     --heartbeat_post--> aggregates all node data
Node E (light sensor)        --heartbeat_post--> generates environment report
```

Extend heartbeat event format to support cross-node event broadcasting.
Nodes broadcast sensor readings as events; AI node aggregates and decides.

**Effort:** ~100 lines. Extend `core/heartbeat.c` + `services/swarm/swarm.c`.

#### Step 4: Task Migration

When one node is heavily loaded, migrate AI tasks to idle nodes.

```
Node A (load=80%)  --> "handle this AI request for me" --> Node B (load=20%)
```

Heartbeat packet already carries `load` field for load awareness.
This is the most distant goal — requires steps 1-3 to be stable first.

### Swarm Resource Comparison

| Metric | Single Node (ESP32-C3) | 5-Node Swarm | 10-Node Swarm |
|--------|----------------------|-------------|--------------|
| Usable SRAM | 240KB | 1.2MB | 2.4MB |
| GPIO Pins | 22 | 110 | 220 |
| Sensor Locations | 1 point | 5 points | 10 points |
| Parallel AI Calls | 1 | 5 | 10 |
| Physical Coverage | ~5m | ~25m | ~50m |
| Peripheral Types | limited to one board | shared across all | shared across all |

**Key insight:** the swarm lets every node access the entire network's hardware
capabilities, while each node only bears its own resource cost.

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
| `scheduler` | Timed callbacks + NVS persistence | + deadline awareness |
| `ai_engine` | Claude API client + context injection | + multi-format (optional) |
| `ai_memory` | RAM ring buffer + NVS long-term | + event journal layer |
| `tools/gpio` | GPIO read/write + safety policy | + ADC/PWM support |
| `tools/sched` | AI-created tasks + persistence | Stable |
| `tools/audio` | ES8311 beep/volume/preset sounds | + recording, streaming |
| `heartbeat` | Periodic AI check-in | + event log aggregation |
| `swarm` | UDP heartbeat + capability bitmap + remote RPC | + distributed perception → task migration |
| `feishu` | WebSocket long connection IM | + multi-IM support |
| `wifi_manager` | WiFi STA management | Stable |

## Resource Budget

### ESP32-C3 (240KB usable SRAM)

| Module | Current | Added | Notes |
|--------|---------|-------|-------|
| ESP-IDF + WiFi + TLS | ~160KB | — | Fixed system overhead |
| Gateway + Scheduler | ~12KB | +1KB | NVS persistence buffer |
| AI Engine + Memory | ~15KB | +2KB | Context injection string |
| Tools | ~4KB | +1KB | GPIO safety policy table |
| Swarm + Heartbeat | ~14KB | +3KB | Capability bitmap + RPC buffer |
| Shell + App | ~10KB | — | No change |
| **Total** | **~215KB** | **+7KB** | ~18KB headroom |

### ESP32-S3 (8MB PSRAM)

PSRAM handles TLS buffers and large allocations
(`SPIRAM_MALLOC_ALWAYSINTERNAL=2048`).
Internal SRAM budget similar to C3; PSRAM provides ample headroom.

## Implementation Roadmap

Ordered by value / effort ratio. Each item is independent.

| Phase | Content | Effort | Value | Status |
|-------|---------|--------|-------|--------|
| 1 | GPIO safety policy | ~50 lines | Prevent AI from damaging hardware pins | Done |
| 2 | Context injection | ~30 lines | AI-aware of device state, more accurate | Done |
| 3 | Dual-core binding (S3) | Config only | Isolate network from app, reduce jitter | Done |
| 4 | Swarm capability bitmap | ~20 lines | Nodes know each other's hardware | Done |
| 5 | Scheduler persistence | ~60 lines | Restore AI automations across reboots | Done |
| 6 | Swarm remote tool invocation | ~150 lines | One node's tools = entire swarm's tools | Done |
| 7 | Event journal layer | ~80 lines | Heartbeat patrol gets historical context | |
| 8 | Distributed perception | ~100 lines | Multi-node sensor data convergence | |
| 9 | Multi-LLM format (opt.) | ~100 lines | Direct connect to OpenAI-compatible APIs | |

**Total: ~590 lines of code changes** covering the full design evolution.
Swarm-related work is ~270 lines — the highest-differentiation investment.

## What We Won't Build

Explicitly listed to prevent over-engineering:

- **No event bus**: FreeRTOS message queues + task priorities are sufficient
- **No provider abstraction layer**: Compile-time selection or proxy is better for constrained environments
- **No filesystem**: NVS key-value pairs cover all persistence needs
- **No complex middleware**: Sense → decide → act, two layers of indirection max
- **No dynamic loading**: Static linking on MCU; features controlled by Kconfig toggles
- **No service registry**: Swarm nodes use direct UDP broadcast; no central coordinator
- **No consensus protocol**: Swarm is loosely coupled — node failure doesn't affect others
