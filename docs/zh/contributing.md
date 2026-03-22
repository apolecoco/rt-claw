# rt-claw 贡献指南

[English](../en/contributing.md) | **中文**

## 快速检查清单

提交补丁前，请确认：

1. 代码至少在一个平台上编译通过
2. 代码通过 `scripts/check-patch.sh` 检查
3. 每个提交都有 `Signed-off-by` 标签
4. 提交信息遵循以下格式

## 提交信息格式

```
子系统: 简短变更描述

可选的详细说明，解释动机和方法。
每行不超过 76 字符。

Signed-off-by: Your Name <your@email.com>
```

### 规则

- **标题行**：`子系统: 描述`（子系统小写，结尾不加句号）
- **标题长度**：最多 76 字符
- **正文**：与标题之间隔一个空行，每行不超过 76 字符
- **Signed-off-by**：每个提交必须包含（`git commit -s`）

### 子系统前缀

| 前缀       | 范围                                      |
|------------|-------------------------------------------|
| `osal`     | 操作系统抽象层（`osal/`）                 |
| `gateway`  | 消息路由（`claw/services/gateway.*`）         |
| `swarm`    | 蜂群服务（`claw/services/swarm/`）        |
| `net`      | 网络服务（`claw/services/net/`）          |
| `ai`       | AI 引擎（`claw/services/ai/`）           |
| `sched`    | 调度器（`claw/services/sched.*`）         |
| `feishu`   | 飞书 IM 集成（`claw/services/im/`）      |
| `drivers`  | 硬件驱动（`drivers/`）                    |
| `platform` | 平台特定变更（`platform/`）               |
| `build`    | 构建系统（Meson、CMake、SCons）           |
| `docs`     | 文档变更                                  |
| `tools`    | Tool Use 框架（`claw/services/tools/`）            |
| `scripts`  | 构建/CI 脚本（`scripts/`）               |
| `main`     | 启动入口和初始化（`claw/init.*`）    |
| `claw`     | 跨模块核心变更                            |
| `ci`       | CI 工作流变更（`.github/`）              |
| `tests`    | 测试脚本（`tests/`）                      |

### 示例

```
gateway: add priority-based message routing

Messages now carry a priority field. The gateway dispatches
high-priority messages before low-priority ones in each
routing cycle.

Signed-off-by: Chao Liu <chao.liu.zevorn@gmail.com>
```

```
osal: implement mutex timeout for RT-Thread

Signed-off-by: Chao Liu <chao.liu.zevorn@gmail.com>
```

### 其他标签

- `Fixes: <sha> ("原始提交标题")` — 修复之前的提交时使用
- `Tested-by:` / `Reviewed-by:` — 测试和审查的署名

## 编码风格

完整编码风格指南参见 [编码风格](coding-style.md)。

要点：
- 4 空格缩进，禁用 Tab
- 目标行宽 80 字符
- 公共 API 使用 `claw_` 前缀
- 使用传统 C 注释（`/* */`）
- C99（gnu99）

## 代码检查工具

### 风格检查

```bash
# 检查范围内的所有源文件（claw/ + osal/ + include/）
scripts/check-patch.sh

# 检查暂存区变更（提交前推荐使用）
scripts/check-patch.sh --staged

# 检查指定文件
scripts/check-patch.sh --file claw/services/gateway.c
```

### 签名检查

```bash
# 检查 main 之后的提交
scripts/check-dco.sh

# 检查最近 3 个提交
scripts/check-dco.sh --last 3
```

### Git Hooks

安装 git hooks 来自动检查风格和提交信息：

```bash
scripts/install-hooks.sh          # 安装
scripts/install-hooks.sh --remove # 卸载
```

安装的 hooks：
- **pre-commit**：对 `claw/`、`osal/` 和 `include/` 中的暂存变更运行 `check-patch.sh --staged`
- **commit-msg**：验证提交信息格式和 Signed-off-by

## 补丁准则

### 拆分补丁

每个提交应该是独立的、可编译的变更。不要在一个提交中混合不相关的修改。

### 不要包含无关变更

不要将风格修复与功能变更混在一起。如果代码需要重新格式化，
请作为单独的提交提交。

### 测试你的变更

- 提交前至少在一个平台上构建通过
- 如果添加新功能，尽可能包含基本测试或演示

## 开发者原创证书

通过在提交中添加 `Signed-off-by` 标签，你声明：

1. 该贡献全部或部分由你创建，且你有权在 MIT 许可证下提交；或
2. 该贡献基于你所知覆盖在合适的开源许可证下的既有工作，
   且你有权提交该工作及其修改；或
3. 该贡献由声明了 (1) 或 (2) 的其他人直接提供给你，
   且你未对其进行修改。
