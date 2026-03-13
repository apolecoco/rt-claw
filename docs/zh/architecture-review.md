# 架构审查与优化计划

[English](../en/architecture-review.md) | **中文**

日期：2026-03-13
范围：全代码库审查（OSAL、Core、Services、Tools、Platform）

## 概要

rt-claw 架构设计方向正确，OSAL 抽象模式清晰。审查发现 10 个问题，
分为三个优先级：2 个 Bug（P0）、2 个代码规范问题（P1）、
6 个设计/架构改进（P2）。本文档描述每个问题的影响和修复方案。

## 问题清单

### P0 — Bug（立即修复）

#### P0-1：FreeRTOS Timer 回调类型不安全 ✅

**文件：** `osal/freertos/claw_os_freertos.c`

**问题：** FreeRTOS timer 回调签名与 OSAL 声明不匹配，强制类型转换
导致 `arg` 参数读取错误。

**已解决：** 添加 `timer_ctx_t` trampoline 结构体，通过
`pvTimerGetTimerID()` 取回真正的用户回调和 arg。

#### P0-2：RT-Thread MQ Send 冗余分支 ✅

**文件：** `osal/rtthread/claw_os_rtthread.c`

**问题：** if/else 两个分支代码完全相同，属于死代码。

**已解决：** 删除冗余分支，保留单行调用。

---

### P1 — 代码规范（尽快修复）

#### P1-1：Header Guard 命名不统一 ✅

**问题：** 多个头文件使用 `__CLAW_*_H__`（双下划线前缀为 C 标准
保留命名空间）。

**已解决：** 统一重命名为 `CLAW_<PATH>_<NAME>_H` 格式。

#### P1-2：日志默认关闭 ✅

**问题：** `s_log_enabled` 默认值为 0，早期 init 阶段日志静默丢弃。

**已解决：** 两个 OSAL 实现中 `s_log_enabled` 默认值改为 1。

---

### P2 — 架构改进（规划讨论）

#### P2-1：OSAL 缺少网络抽象 ✅

**问题：** OSAL 无网络 API，核心代码中 HTTP 传输使用 `#ifdef`
平台分支。

**已解决：** 新增 `claw_net.h`，提供 `claw_net_post()` HTTP 客户端
API，各平台在 `osal/` 目录下实现。

#### P2-2：Gateway 是空壳路由器 ✅

**问题：** `gateway.c` 收到消息只打日志，无路由功能。

**已解决：** 精简为节点间路由最小骨架。移除事件总线机制
（subscribe、dispatch、handler 表、mutex）和未使用字段。
Gateway 仅保留消息队列和线程——路由逻辑待 swarm 多节点通信
实现时添加。Swarm 节点事件改为直接日志输出。

#### P2-3：无统一服务接口 ✅

**问题：** 服务生命周期模式不一致。

**已解决：** 定义 `claw_service_t` 结构体（name/init/start/stop），
服务注册到 `claw_init.c` 的表中，启动时遍历执行。

#### P2-4：启动时阻塞式 AI 调用 ✅

**问题：** `claw_init()` 同步调用 `ai_chat_raw()`，最坏阻塞 ~18 秒。

**已解决：** AI 连通性测试移到独立低优先级线程
（`ai_boot_test_thread`），启动不再阻塞。

#### P2-5：Gateway 消息固定 256 字节 Payload — 延后

**问题：** 每条消息 ~260 字节，不论实际负载大小。

**状态：** 当前可接受。Gateway 已精简为最小骨架，待节点间路由
实现且内存压力增大时再优化。

#### P2-6：Tool 注册表线程安全 — 延后

**问题：** `s_tools[]` 无互斥保护。

**状态：** 当前风险低（注册在 init 时完成）。动态注册时再加锁。

---

## 待办事项

### B-1：OpenClaw 风格心跳（定时 AI 巡检）

**灵感来源：** OpenClaw 的 heartbeat 机制——定时驱动 LLM 检查任务
文件，有可执行事项时主动通知用户。

**概念：** 定时任务周期性调用 `ai_chat()`，审查待处理事项（传感器
告警、swarm 状态变化、定时提醒等），有可操作内容时通过 IM 推送
摘要给用户。

**归属：** Scheduler 服务（`claw/core/scheduler`），而非 gateway
或 swarm。Scheduler 已具备定时器基础设施，这将作为新的定时任务
类型。

**状态：** 规划中——尚未实现。

## 实施顺序

| 阶段 | 问题 | 状态 |
|------|------|------|
| 第一阶段 | P0-1, P0-2, P1-1, P1-2 | ✅ 已完成 |
| 第二阶段 | P2-4, P2-2 | ✅ 已完成 |
| 第三阶段 | P2-1, P2-3 | ✅ 已完成 |
| 延后 | P2-5, P2-6 | 低优先级 |
| 待办 | B-1 | 规划中 |
