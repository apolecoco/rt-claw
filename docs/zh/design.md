# rt-claw 演进设计

**中文** | [English](../en/design.md)

## 定位

rt-claw = **Hardware-first Real-Time AI Runtime**

不是"在 MCU 上跑 AI"，而是**实时硬件控制系统 + AI 增强**。
核心价值：将原子化硬件能力（GPIO、传感器、LCD、网络）暴露为 LLM 工具，
AI 动态编排，无需重新编写/编译/烧录嵌入式代码。

```
+------------------+     +------------------+
|  实时硬件控制    |     |  AI 智能决策     |
|  (确定性, 低延迟) |<--->|  (尽力而为)       |
+------------------+     +------------------+
         |                        |
    FreeRTOS 任务优先级      HTTP/HTTPS 到云端
    直接操作外设             Tool Use 回调
```

关键原则：
- **链路短**：感知 → 决策 → 执行，中间不超过两层抽象
- **确定性优先**：实时任务不依赖 AI 响应
- **资源敬畏**：ESP32-C3 仅 240KB 可用 SRAM，每一字节都有成本

## 事件优先级模型

基于 FreeRTOS 任务优先级直接实现，不引入额外调度框架。

| 层级 | 名称 | 延迟要求 | 实现方式 | 示例 |
|------|------|----------|----------|------|
| P0 | Reflex | 1-10ms | ISR + 高优先级任务 (prio 24) | GPIO 安全策略、看门狗 |
| P1 | Control | 10-50ms | 高优先级任务 (prio 20) | 传感器轮询、PID 控制 |
| P2 | Interaction | 50-150ms | 中优先级任务 (prio 15) | Shell ACK、LCD 刷新、Gateway |
| P3 | AI | 尽力而为 | 低优先级任务 (prio 5-10) | LLM API 调用、蜂群同步 |

当前模块的优先级映射（`claw_config.h`）：

```
Gateway:    prio 15  → P2 Interaction
Scheduler:  prio 10  → P2/P3 边界
Swarm:      prio 12  → P2 Interaction
Heartbeat:  prio 5   → P3 AI（隐含，由 scheduler 触发）
AI Engine:  prio 5   → P3 AI（shell 调用时继承 shell 线程优先级）
```

**不需要新增代码**——只需在文档中明确优先级约定，新模块遵循此映射。

## 三条执行路径

```
用户输入 / 传感器事件
        |
        +--- [Reflex] ---> 本地规则直接执行（无 AI）
        |                   例：GPIO 安全拦截、温度告警自动关断
        |
        +--- [Interactive] ---> 本地即时 ACK + 后台 AI
        |                       例：Shell 输入 → 先打印"thinking..."
        |                            → AI 响应后再输出完整回复
        |
        +--- [Cognitive] ---> 纯 AI 驱动（多轮 Tool Use）
                              例：定时心跳巡检、飞书消息响应
```

**当前已实现：** Interactive 路径（shell thinking 动画）、Cognitive 路径（heartbeat、feishu）。
**待实现：** Reflex 路径（GPIO 安全策略）。

## 能力模型

五层能力与现有模块的映射：

```
+----------------------------------------------------------+
|  Cognition（认知）                                        |
|  ai_engine + tools + heartbeat                           |
|  多轮对话、Tool Use、定时巡检                             |
+----------------------------------------------------------+
|  Reflex（反射）                                           |
|  GPIO safety policy + scheduler                          |
|  本地规则引擎、安全拦截、定时自动化                       |
+----------------------------------------------------------+
|  Expression（表达）                                       |
|  LCD + Shell + Feishu                                    |
|  文字、图形、IM 消息输出                                  |
+----------------------------------------------------------+
|  Action（执行）                                           |
|  tools/gpio + tools/sched + tools/net                    |
|  GPIO 操作、定时任务、HTTP 请求                           |
+----------------------------------------------------------+
|  Perception（感知）                                       |
|  WiFi scan + swarm discovery + 传感器（待扩展）          |
|  环境感知、节点发现、数据采集                             |
+----------------------------------------------------------+
```

## 借鉴 MimiClaw 的设计

以下设计经分析验证，值得引入 rt-claw。每项标注实现位置和预估工作量。

### 1. GPIO 安全策略

MimiClaw 的三层防御：CSV 白名单 → 硬件保留引脚拦截 → 平台差异化 → 人类可读拒绝提示。

**rt-claw 方案：**

```c
/* tools/tool_gpio.c — 静态白名单，编译时确定 */
static const struct {
    int pin;
    uint32_t allowed_modes;  /* bitmask: INPUT / OUTPUT / ADC */
    const char *label;       /* human-readable, 用于 AI 拒绝提示 */
} s_gpio_policy[] = {
    { 4,  GPIO_MODE_OUTPUT, "LED" },
    { 5,  GPIO_MODE_INPUT,  "Button" },
    /* ... platform-specific entries via #ifdef */
};

/* 拦截逻辑：在 tool_gpio_set() 入口检查 */
/* 未在白名单中 → 返回 "pin X is reserved for <label>, operation denied" */
```

**工作量：** ~50 行。修改 `claw/tools/tool_gpio.c`。

### 2. 三层记忆

MimiClaw 使用 MEMORY.md（长期）+ 每日笔记（YYYY-MM-DD.md）+ 会话 JSONL。
rt-claw 已有短期 RAM 环形缓冲 + NVS 长期存储，缺少中间层。

**rt-claw 方案：**

| 层级 | 存储 | 容量 | 用途 |
|------|------|------|------|
| 会话 | RAM 环形缓冲 | 20 条（`AI_MEMORY_MAX_MSGS`） | 当前对话上下文 |
| 长期 | NVS Flash | ~4KB | 用户偏好、关键事实 |
| 日志 | NVS Flash（可选） | ~2KB 循环 | 每日事件摘要 |

日志层为可选增强——当 heartbeat 发现值得记录的事件时，追加一行摘要到 NVS。
心跳巡检时读取近期日志作为上下文。

**工作量：** ~80 行。扩展 `claw/services/ai/ai_memory.c`。

### 3. 定时任务持久化

MimiClaw 的 cron 任务重启后恢复。rt-claw 的 scheduler 当前纯内存。

**rt-claw 方案：**

```c
/* core/scheduler.c — NVS 持久化 */
/* sched_add() 时写入 NVS: "sched_0" = "name|interval|count|remaining" */
/* sched_init() 时扫描 NVS 恢复（仅恢复 AI 创建的定时任务） */
/* 限制：回调函数不可序列化，持久化任务统一使用 sched_ai_callback() */
/*       即重启后定时任务重新触发 AI 调用，而非恢复原始回调 */
```

**工作量：** ~60 行。修改 `claw/core/scheduler.c`。

### 4. 双核任务绑定（ESP32-S3）

ESP32-S3 有两个 Xtensa LX7 核心。MimiClaw 将 WiFi/网络绑定到 Core 0，
应用逻辑绑定到 Core 1。

**rt-claw 方案：**

```
Core 0: WiFi + TLS + HTTP（ESP-IDF 默认）
Core 1: Shell + AI Engine + Tools + Scheduler

/* 实现：在 sdkconfig.defaults 中配置 */
CONFIG_FREERTOS_UNICORE=n         /* 已是默认 */
CONFIG_ESP_MAIN_TASK_CORE_AFFINITY_CPU1=y  /* main task 跑 Core 1 */
```

**工作量：** 配置变更，0 行代码。修改 `platform/esp32s3/sdkconfig.defaults`。

### 5. 多 LLM 提供商

MimiClaw 支持 10+ LLM API。rt-claw 当前仅支持 Claude API 格式。

**rt-claw 方案：**

不引入提供商抽象层（资源浪费）。利用现有 API 代理模式：

```
方案 A（推荐）：服务端代理统一格式
  device --HTTP--> proxy --HTTPS--> Claude / GPT / Gemini / ...
  设备端只需实现一种 API 格式（Claude Messages API）

方案 B（直连）：编译时选择
  #ifdef CONFIG_RTCLAW_API_FORMAT_OPENAI
  /* 构造 OpenAI 格式请求 */
  #else
  /* 构造 Claude 格式请求（默认） */
  #endif
```

方案 A 对设备零改动。方案 B 约 100 行，修改 `claw/services/ai/ai_engine.c`。

### 6. 上下文注入

MimiClaw 在 system prompt 中注入当前设备状态（WiFi、时间、传感器）。

**rt-claw 方案：**

```c
/* services/ai/ai_engine.c — 在构造 system prompt 时追加 */
/* [Device Context] */
/* - Platform: ESP32-S3, WiFi: connected (192.168.1.100) */
/* - Uptime: 3h 42m, Free heap: 156KB */
/* - Scheduled tasks: 2 active */
/* - Last heartbeat: 5 events collected */
```

**工作量：** ~30 行。修改 `claw/services/ai/ai_engine.c` 的 system prompt 构造。

### 7. Skill 文件系统

MimiClaw 将技能存储为独立文件，可动态加载。
rt-claw 已有 `ai_skill.c`，技能存储在 NVS 中。

**当前方案已足够**——NVS 是 ESP32 上最合理的持久化方式。
无需文件系统，保持现有实现。

## 模块演进映射

| 现有模块 | 当前角色 | 演进方向 |
|----------|----------|----------|
| `gateway` | 消息队列骨架 | 蜂群节点间路由（待多节点时实现） |
| `scheduler` | 定时回调 | + NVS 持久化 + deadline 感知 |
| `ai_engine` | Claude API 客户端 | + 上下文注入 + 多格式支持（可选） |
| `tools/gpio` | GPIO 读写 | + 安全策略白名单 |
| `tools/sched` | AI 创建定时任务 | + 持久化恢复 |
| `heartbeat` | 定时 AI 巡检 | + 事件日志聚合 |
| `swarm` | UDP 心跳发现 | + 能力广播 + 任务分发 |
| `wifi_manager` | WiFi STA 管理 | 当前完整，无需改动 |

## 资源预算

### ESP32-C3（240KB 可用 SRAM）

| 模块 | 当前 SRAM | 新增估算 | 说明 |
|------|-----------|----------|------|
| ESP-IDF + WiFi + TLS | ~160KB | — | 系统固定开销 |
| Gateway + Scheduler | ~12KB | +1KB | NVS 持久化缓冲 |
| AI Engine + Memory | ~15KB | +2KB | 上下文注入字符串 |
| Tools | ~4KB | +1KB | GPIO 安全策略表 |
| Swarm + Heartbeat | ~14KB | — | 无变化 |
| Shell + App | ~10KB | — | 无变化 |
| **合计** | **~215KB** | **+4KB** | 剩余 ~21KB 余量 |

### ESP32-S3（8MB PSRAM）

PSRAM 用于 TLS 缓冲区和大块内存分配（已配置 `SPIRAM_MALLOC_ALWAYSINTERNAL=2048`）。
内部 SRAM 预算与 C3 类似，但 PSRAM 提供充足余量。

## 实施路线图

按价值 / 工作量比排序。每项独立，无依赖关系。

| 阶段 | 内容 | 工作量 | 价值 |
|------|------|--------|------|
| 1 | GPIO 安全策略 | ~50 行 | 防止 AI 误操作硬件引脚 |
| 2 | 上下文注入 | ~30 行 | AI 感知设备状态，回复更准确 |
| 3 | 双核绑定（S3） | 配置项 | 网络与应用隔离，减少延迟抖动 |
| 4 | 定时任务持久化 | ~60 行 | 重启恢复 AI 创建的自动化任务 |
| 5 | 事件日志层 | ~80 行 | 心跳巡检获得历史上下文 |
| 6 | 多 LLM 格式（可选） | ~100 行 | 直连 OpenAI 兼容 API |

**总计约 320 行代码变更**，覆盖 80% 的设计演进目标。

## 不做的事

明确列出**不引入**的机制，避免过度工程：

- **不引入事件总线**：FreeRTOS 消息队列 + 任务优先级已足够
- **不引入提供商抽象层**：编译时选择或代理模式更适合资源受限环境
- **不引入文件系统**：NVS 键值对覆盖所有持久化需求
- **不引入复杂中间件**：感知 → 决策 → 执行，最多两层间接
- **不引入动态加载**：MCU 上静态链接，功能由编译时 Kconfig 开关控制
