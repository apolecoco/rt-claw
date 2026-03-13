# Architecture Review & Optimization Plan

**English** | [中文](../zh/architecture-review.md)

Date: 2026-03-13
Scope: Full codebase review (OSAL, Core, Services, Tools, Platform)

## Executive Summary

The rt-claw architecture is well-structured with a clean OSAL abstraction
pattern. However, the review identified 10 issues across three severity
levels: 2 bugs (P0), 2 code hygiene issues (P1), and 6 design/architecture
improvements (P2). This document describes each issue, its impact, and
the planned fix.

## Issue Inventory

### P0 — Bugs (Fix Immediately)

#### P0-1: FreeRTOS Timer Callback Type Mismatch ✅

**File:** `osal/freertos/claw_os_freertos.c`

**Problem:** FreeRTOS timer callbacks have signature
`void cb(TimerHandle_t xTimer)`, but the OSAL declares
`void (*callback)(void *arg)`. The code force-casted the user callback
pointer, so any timer callback that uses `arg` would read garbage.

**Resolution:** Added `timer_ctx_t` trampoline struct. The trampoline
retrieves the real callback and arg via `pvTimerGetTimerID()`.

#### P0-2: RT-Thread MQ Send Redundant Branch ✅

**File:** `osal/rtthread/claw_os_rtthread.c`

**Problem:** Both `if` and `else` branches were identical (dead code).

**Resolution:** Collapsed to a single call.

---

### P1 — Code Hygiene (Fix Soon)

#### P1-1: Header Guard Naming Inconsistency ✅

**Standard (per coding-style.md):** `CLAW_<PATH>_<NAME>_H`

**Problem:** Multiple headers used `__CLAW_*_H__` (double underscore
prefix is reserved by the C standard).

**Resolution:** All header guards renamed to `CLAW_<PATH>_<NAME>_H`.

#### P1-2: Logging Disabled at Boot ✅

**Problem:** `s_log_enabled` defaulted to 0, silently dropping all
`CLAW_LOGI`/`CLAW_LOGW`/`CLAW_LOGE` calls during early init.

**Resolution:** Default `s_log_enabled` to 1 in both OSAL
implementations.

---

### P2 — Architecture Improvements (Plan & Discuss)

#### P2-1: OSAL Network Abstraction Missing ✅

**Problem:** No networking API in OSAL. All HTTP transport used
`#ifdef` platform switches in core code.

**Resolution:** Added `claw_net.h` with `claw_net_post()` HTTP client
API. Per-platform implementations in `osal/freertos/` and
`osal/rtthread/`.

#### P2-2: Gateway Is a No-Op Router ✅

**Problem:** `gateway.c` received messages but only logged them.
No handler registration, no dispatch, no subscriber pattern.

**Resolution:** Stripped to minimal skeleton for future inter-node
routing. Removed event-bus machinery (subscribe, dispatch, handler
table, mutex) and unused `src_channel` / `dst_channel` fields.
Gateway now only keeps a message queue and thread — routing logic
will be added when swarm multi-node communication is implemented.
Swarm node events use direct logging instead of gateway dispatch.

#### P2-3: No Unified Service Interface ✅

**Problem:** Services had inconsistent lifecycle patterns.

**Resolution:** Defined `claw_service_t` struct with `name`, `init`,
`start`, `stop` fields. Services registered in a table in
`claw_init.c`, iterated during boot.

#### P2-4: Blocking AI Call at Boot ✅

**Problem:** `claw_init()` made a synchronous `ai_chat_raw()` call,
blocking boot for up to ~18 seconds if the API was unreachable.

**Resolution:** AI connectivity test moved to a separate low-priority
thread (`ai_boot_test_thread`), boot completes without blocking.

#### P2-5: Fixed-Size Gateway Message Payload — Deferred

**Problem:** `struct gateway_msg` embeds `uint8_t payload[256]` —
every message costs ~260 bytes regardless of actual payload size.

**Status:** Acceptable for now. Gateway is a minimal skeleton; revisit
when inter-node routing is implemented and memory pressure increases.

#### P2-6: Tool Registry Thread Safety — Deferred

**Problem:** `s_tools[]` and `s_tool_count` in `claw_tools.c` have no
mutex protection.

**Status:** Low risk — registration is init-time only. Add a read lock
if dynamic tool registration is added later.

---

## Backlog

### B-1: OpenClaw-Style Heartbeat (Periodic AI Check-in)

**Inspired by:** OpenClaw's heartbeat mechanism — a timer-driven LLM
check that periodically reviews a task file and proactively notifies
the user of actionable items.

**Concept:** A scheduled task that periodically calls `ai_chat()` to
review pending items (e.g. sensor alerts, swarm status changes,
scheduled reminders) and pushes a summary to the user via IM if
anything is actionable.

**Where it fits:** Scheduler service (`claw/core/scheduler`), not
gateway or swarm. The scheduler already has timer infrastructure;
this would be a new scheduled task type.

**Status:** Planned — not yet implemented.

## Implementation Order

| Phase | Issues | Status |
|-------|--------|--------|
| Phase 1 | P0-1, P0-2, P1-1, P1-2 | ✅ Done |
| Phase 2 | P2-4, P2-2 | ✅ Done |
| Phase 3 | P2-1, P2-3 | ✅ Done |
| Deferred | P2-5, P2-6 | Low priority |
| Backlog | B-1 | Planned |
