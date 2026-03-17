# Web 在线工具

[English](../en/web-tools.md) | **中文**

RT-Claw 官网提供浏览器端的固件刷写和串口调试工具，无需安装任何软件。

> 要求：Chrome 或 Edge 浏览器（需支持 Web Serial API）。

## 固件刷写

### 准备

1. 用 USB 线连接开发板到电脑
2. 打开 [rt-claw.com](https://www.rt-claw.com) 或本地 `website/index.html`
3. 滚动到 **Flash** 区域

### 操作步骤

**1. 选择平台和开发板**

| 平台 | 开发板 | 说明 |
|------|--------|------|
| ESP32-C3 | Generic DevKit (4 MB) | 通用 ESP32-C3 开发板 |
| ESP32-C3 | Xiaozhi XMini (16 MB) | 小智 Mini，带 OLED 和音频 |
| ESP32-S3 | Generic DevKit (16 MB + 8 MB PSRAM) | 通用 ESP32-S3 开发板 |

切换平台时，开发板选项会自动更新。

**2. 选择版本**

| 选项 | 来源 | 说明 |
|------|------|------|
| latest (main) | GitHub Pages | main 分支最新 CI 构建 |
| v0.2.0, v0.1.0, ... | GitHub Release | 带标签的发布版本（需有含固件的 Release） |

没有 GitHub Release 的标签会显示为禁用状态，标注 "(no firmware)"。

**3. 选择波特率**

默认 921600（最快）。如果刷写不稳定，降低到 460800 或 115200。

**4. 点击 "Install RT-Claw"**

浏览器会弹出串口选择窗口，选择对应的 COM 口或 `/dev/ttyUSB*`。

**5. 等待完成**

烧录输出直接显示在右侧的串口监视器面板中。烧录期间串口面板的所有控制按钮
均被禁用（展开按钮除外），防止操作冲突。刷写完成后设备会自动重启。

### 擦除 Flash

点击 **Erase** 按钮可擦除整片 Flash（恢复出厂状态）。擦除后需重新刷写固件。

### 常见问题

| 问题 | 原因 | 解决 |
|------|------|------|
| 找不到串口 | 驱动未安装或线缆问题 | 安装 CH340/CP2102 驱动；换数据线 |
| Failed to open serial port | 串口被其他程序占用 | 关闭 idf.py monitor / Arduino IDE 等 |
| Chip mismatch | 平台选错 | 确认选择的平台与实际芯片一致 |
| Firmware not available | CI 尚未构建该固件 | 本地构建后手动烧录 |

## 串口终端

串口终端位于 Flash 区域右侧（或下方），用于与设备实时通信。

### 连接

1. 选择波特率（默认 115200）
2. 点击 **Connect**
3. 浏览器弹出串口选择窗口，选择设备

连接成功后状态指示灯变绿，按钮变为 **Disconnect**。

### 发送消息

在底部输入栏输入文本，按 **Enter** 或点击 **Send** 发送。
发送的内容以 `\r\n` 结尾，兼容嵌入式终端。

与 AI 对话示例：

```
你好
```

设备回复会实时显示在输出区域。

### ANSI 彩色渲染

终端支持 ANSI 转义码的彩色显示：

- 标准 8 色和亮色（SGR 30-37 / 90-97）
- 256 色扩展（SGR 38;5;N）
- 24-bit RGB（SGR 38;2;R;G;B）
- 粗体、暗淡属性

系统消息使用颜色区分：
- 绿色：连接成功
- 红色：错误
- 黄色：断开连接
- 灰色：复位

### 放大终端

点击标题栏的 **⬜** 按钮，终端向左扩展覆盖固件烧录面板。
再次点击恢复原始大小。

### 控制按钮

| 按钮 | 功能 |
|------|------|
| Connect / Disconnect | 打开或关闭串口连接 |
| Reset | 通过 DTR 信号复位设备 |
| Clear | 清空终端输出 |
| ⬜ | 展开 / 收回终端 |

### 注意事项

- 刷新页面时串口会自动断开
- 同一串口不能同时被两个程序使用
- 中文和 emoji 字符正确显示（UTF-8 流式解码）
- `\r`（回车符）支持行内覆写动画（如 thinking...）

## 文档中心

点击导航栏的 **Docs** 进入文档中心，或直接访问 `docs.html`。

### 功能

- 左侧导航栏列出所有文档
- 点击切换文档，无需刷新页面
- 文档内的链接（如 `[编码风格](coding-style.md)`）自动转为站内导航
- 中英文切换通过右上角 **EN / 中文** 按钮，文档内容自动重载

### URL 格式

```
docs.html?doc=getting-started    # 快速开始
docs.html?doc=usage              # 使用指南
docs.html?doc=architecture       # 架构设计
docs.html?doc=porting            # 移植与扩展
docs.html?doc=tuning             # 裁剪与优化
docs.html?doc=web-tools          # Web 在线工具
docs.html?doc=coding-style       # 编码风格
docs.html?doc=contributing       # 贡献指南
```

可直接分享带参数的 URL，接收者打开即可看到对应文档。
