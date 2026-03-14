# rt-claw 演进设计

**中文** | [English](../en/design.md)

## 定位

rt-claw = **Hardware-first Real-Time AI Runtime**

不是"在 MCU 上跑 AI"，而是**实时硬件控制系统 + AI 增强**。
核心价值：将原子化硬件能力（GPIO、传感器、LCD、网络）暴露为 LLM 工具，
AI 动态编排，无需重新编写/编译/烧录嵌入式代码。

```
+------------------+     +------------------+
|  实时硬件控制     |     |  AI 智能决策       |
|  (确定性, 低延迟) |<--->|  (尽力而为)        |
+------------------+     +------------------+
         |                        |
    FreeRTOS 任务优先级      HTTP/HTTPS 到云端
    直接操作外设             Tool Use 回调
```

关键原则：
- **链路短**：感知 → 决策 → 执行，中间不超过两层抽象
- **确定性优先**：实时任务不依赖 AI 响应
- **资源敬畏**：ESP32-C3 仅 240KB 可用 SRAM，每一字节都有成本
- **蜂群放大**：单节点资源有限，多节点联合资源无界

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
|  多轮对话、Tool Use、定时巡检                               |
+----------------------------------------------------------+
|  Reflex（反射）                                           |
|  GPIO safety policy + scheduler                          |
|  本地规则引擎、安全拦截、定时自动化                           |
+----------------------------------------------------------+
|  Expression（表达）                                       |
|  LCD + OLED + Shell + Feishu + Audio (ES8311)            |
|  文字、图形、IM 消息、音频输出                               |
+----------------------------------------------------------+
|  Action（执行）                                           |
|  tools/gpio + tools/sched + tools/net                    |
|  GPIO 操作、定时任务、HTTP 请求                             |
+----------------------------------------------------------+
|  Perception（感知）                                       |
|  WiFi scan + swarm discovery + 传感器（待扩展）            |
|  环境感知、节点发现、数据采集                                |
+----------------------------------------------------------+
+----------------------------------------------------------+
```

## 蜂群智能 — 核心特色

**单个硬件资源有限，多个硬件联合起来资源无界。**

一个 ESP32-C3 只有 240KB SRAM、一组 GPIO、一个 WiFi 天线。
但 10 个 rt-claw 节点组网后，就拥有：
- 10 倍的 GPIO 引脚，分布在不同物理位置
- 10 倍的传感器覆盖面，感知更大的物理空间
- 10 倍的并行 AI 调用能力，响应更快
- 能力互通——Node A 有 LCD，Node B 有温度传感器，
  AI 可以让 B 读温度、让 A 显示在屏幕上

这是 rt-claw 区别于 MimiClaw 等单机方案的**核心差异点**。

### 当前状态

已实现的蜂群基础设施：

```
Node A                          Node B
+--------+    UDP broadcast     +--------+
| swarm  |<------ 5300 ------->| swarm  |
| 发现层  |    heartbeat 5s     | 发现层  |
+--------+                     +--------+
    |  join/leave event             |
    v                               v
+----------+                   +----------+
| heartbeat|                   | heartbeat|
| 事件聚合  |                   | 事件聚合  |
+----------+                   +----------+
```

- UDP 广播发现（端口 5300，间隔 5 秒）
- 16 字节紧凑心跳包（magic + node_id + uptime + capabilities + load + port）
- 节点状态追踪（OFFLINE / DISCOVERING / ONLINE）
- 15 秒超时检测
- 节点加入/离开事件推送到 heartbeat

### 演进路线

#### 第一步：能力位图广播

心跳包中已有 `capabilities` 字段（当前固定为 0），填充真实能力位。

```c
/* services/swarm/swarm.c — 能力位定义 */
#define SWARM_CAP_GPIO      (1 << 0)  /* 有可控 GPIO */
#define SWARM_CAP_LCD       (1 << 1)  /* 有 LCD 显示 */
#define SWARM_CAP_SENSOR    (1 << 2)  /* 有传感器 */
#define SWARM_CAP_CAMERA    (1 << 3)  /* 有摄像头 */
#define SWARM_CAP_SPEAKER   (1 << 4)  /* 有扬声器 */
#define SWARM_CAP_AI        (1 << 5)  /* AI 引擎可用 */
#define SWARM_CAP_INTERNET  (1 << 6)  /* 有外网连接 */

/* heartbeat_send() 中填充 */
hb.capabilities = SWARM_CAP_GPIO | SWARM_CAP_AI | SWARM_CAP_INTERNET;
/* 平台 / Kconfig 决定具体值 */
```

**工作量：** ~20 行。节点发现后即可查询"谁有 LCD"、"谁有传感器"。

#### 第二步：远程工具调用（Claw Skill Provider）

核心思想：**一个节点的工具就是整个蜂群的工具**。

```
Node A (有 LCD)                    Node B (无 LCD)
+-----------+                      +-----------+
| tool:     |    GW_MSG_SWARM      | AI Engine |
| lcd_draw  |<-----[RPC 请求]------| "在屏幕上 |
|           |------[RPC 响应]----->|  画个圆"  |
+-----------+                      +-----------+
```

实现方式：复用 Gateway 的 `GW_MSG_SWARM` 消息类型 + 已有 UDP 通道。

```c
/* 消息格式（复用 gateway_msg 的 256 字节 payload） */
struct swarm_rpc_msg {
    uint32_t src_node;          /* 请求方 */
    uint32_t dst_node;          /* 执行方 */
    uint16_t seq;               /* 序列号 */
    uint8_t  type;              /* 0=request, 1=response */
    char     tool_name[32];     /* 工具名 */
    char     params_json[187];  /* 输入参数 JSON */
};

/* Node B 的 AI 发现本地没有 lcd_draw → 查询蜂群 →
   发现 Node A 有 SWARM_CAP_LCD → 发送 RPC → Node A 执行 → 返回结果 */
```

AI 不需要知道工具在哪个节点上。工具查找逻辑：
1. 先查本地 `claw_tool_find(name)` → 找到则本地执行
2. 未找到 → 查蜂群节点能力表 → 找到则发送 RPC
3. 都找不到 → 返回 "tool not available"

**工作量：** ~150 行。修改 `claw/tools/` + `claw/services/swarm/`。

#### 第三步：分布式感知聚合

多节点传感器数据汇聚到一个节点做决策。

```
Node C (温度传感器)  --heartbeat_post--> Node A (AI 决策中心)
Node D (湿度传感器)  --heartbeat_post--> 聚合所有节点数据
Node E (光照传感器)  --heartbeat_post--> 生成环境报告
```

实现：扩展 heartbeat 事件格式，支持跨节点事件推送。
节点将传感器读数作为事件广播，AI 节点聚合后统一决策。

**工作量：** ~100 行。扩展 `core/heartbeat.c` + `services/swarm/swarm.c`。

#### 第四步：任务迁移

当一个节点负载高时，将 AI 任务迁移到空闲节点。

```
Node A (load=80%)  --> "帮我处理这个 AI 请求" --> Node B (load=20%)
```

心跳包中已有 `load` 字段，可用于负载感知。
这是最远期的目标，需要前三步都稳定后再实现。

### 蜂群资源对比

| 指标 | 单节点 (ESP32-C3) | 5 节点蜂群 | 10 节点蜂群 |
|------|-------------------|-----------|------------|
| 可用 SRAM | 240KB | 1.2MB | 2.4MB |
| GPIO 引脚 | 22 | 110 | 220 |
| 传感器位置 | 1 点 | 5 点 | 10 点 |
| 并行 AI 调用 | 1 | 5 | 10 |
| 物理覆盖范围 | ~5m | ~25m | ~50m |
| 外设种类 | 受限于单板 | 互通有无 | 互通有无 |

**关键洞察：** 蜂群让每个节点都能访问整个网络的硬件能力，
而每个节点自身只需承担自己的资源成本。

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
| `scheduler` | 定时回调 + NVS 持久化 | + deadline 感知 |
| `ai_engine` | Claude API 客户端 + 上下文注入 | + 多格式支持（可选） |
| `ai_memory` | RAM 环形缓冲 + NVS 长期存储 | + 事件日志层 |
| `tools/gpio` | GPIO 读写 + 安全策略 | + ADC/PWM 支持 |
| `tools/sched` | AI 创建定时任务 + 持久化 | 稳定 |
| `tools/audio` | ES8311 蜂鸣/音量/预设音效 | + 录音、流式播放 |
| `heartbeat` | 定时 AI 巡检 | + 事件日志聚合 |
| `swarm` | UDP 心跳 + 能力位图 + 远程 RPC | + 分布式感知 → 任务迁移 |
| `feishu` | WebSocket 长连接 IM | + 多 IM 通道支持 |
| `wifi_manager` | WiFi STA 管理 | 稳定 |

## 资源预算

### ESP32-C3（240KB 可用 SRAM）

| 模块 | 当前 SRAM | 新增估算 | 说明 |
|------|-----------|----------|------|
| ESP-IDF + WiFi + TLS | ~160KB | — | 系统固定开销 |
| Gateway + Scheduler | ~12KB | +1KB | NVS 持久化缓冲 |
| AI Engine + Memory | ~15KB | +2KB | 上下文注入字符串 |
| Tools | ~4KB | +1KB | GPIO 安全策略表 |
| Swarm + Heartbeat | ~14KB | +3KB | 能力位图 + RPC 缓冲 |
| Shell + App | ~10KB | — | 无变化 |
| **合计** | **~215KB** | **+7KB** | 剩余 ~18KB 余量 |

### ESP32-S3（8MB PSRAM）

PSRAM 用于 TLS 缓冲区和大块内存分配（已配置 `SPIRAM_MALLOC_ALWAYSINTERNAL=2048`）。
内部 SRAM 预算与 C3 类似，但 PSRAM 提供充足余量。

## 实施路线图

按价值 / 工作量比排序。每项独立，无依赖关系。

| 阶段 | 内容 | 工作量 | 价值 | 状态 |
|------|------|--------|------|------|
| 1 | GPIO 安全策略 | ~50 行 | 防止 AI 误操作硬件引脚 | Done |
| 2 | 上下文注入 | ~30 行 | AI 感知设备状态，回复更准确 | Done |
| 3 | 双核绑定（S3） | 配置项 | 网络与应用隔离，减少延迟抖动 | Done |
| 4 | 蜂群能力位图 | ~20 行 | 节点知道彼此有什么硬件 | Done |
| 5 | 定时任务持久化 | ~60 行 | 重启恢复 AI 创建的自动化任务 | Done |
| 6 | 蜂群远程工具调用 | ~150 行 | 一个节点的工具 = 整个蜂群的工具 | Done |
| 7 | 事件日志层 | ~80 行 | 心跳巡检获得历史上下文 | |
| 8 | 分布式感知聚合 | ~100 行 | 多节点传感器数据汇聚决策 | |
| 9 | 多 LLM 格式（可选） | ~100 行 | 直连 OpenAI 兼容 API | |

**总计约 590 行代码变更**，覆盖全部设计演进目标。
其中蜂群相关约 270 行，是最具差异化价值的投入。

## 不做的事

明确列出**不引入**的机制，避免过度工程：

- **不引入事件总线**：FreeRTOS 消息队列 + 任务优先级已足够
- **不引入提供商抽象层**：编译时选择或代理模式更适合资源受限环境
- **不引入文件系统**：NVS 键值对覆盖所有持久化需求
- **不引入复杂中间件**：感知 → 决策 → 执行，最多两层间接
- **不引入动态加载**：MCU 上静态链接，功能由编译时 Kconfig 开关控制
- **不引入服务注册中心**：蜂群节点直接 UDP 广播，无需中心协调者
- **不引入共识协议**：蜂群是松耦合的——节点离线不影响其他节点工作
