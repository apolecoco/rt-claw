# RT-Claw | 只需一美元硬件成本，快速部署你的专属龙虾！

一颗成本一美元左右的 ESP32-C3，160MHz 主频，400KB 内存——这配置拿来跑 AI？

我们做到了。RT-Claw（实时龙虾），一只跑在嵌入式芯片上的 AI 龙虾。

![RT-Claw](../../images/logo.png)

**RT-Claw** 是一个运行在嵌入式设备上的 AI 助理。它不需要 GPU，不需要 Linux，甚至不需要真实硬件——一台普通电脑上的 QEMU 就能完整运行。

## 它能干什么？

RT-Claw 把芯片上的硬件能力——GPIO、传感器、LCD 屏幕、网络——全部原子化为"工具"，交给大模型动态编排。你只需要用自然语言描述需求，AI 会自己决定调用哪些硬件、按什么顺序执行。

比如跟它说"画一只龙虾"，它会自动调用 lcd_circle、lcd_line、lcd_rect 等一系列绘图工具，在 320x240 的屏幕上完成创作：

![AI 绘图演示](../../images/demo.png)

不想对着串口终端聊天？接入飞书，直接在手机上跟你的嵌入式设备对话：

![飞书对话](../../images/feishu_talk1.png)

问它系统状态，它会自己调用 system_info 和 memory_info 工具，返回芯片型号、运行时间、内存占用率：

![系统监控](../../images/feishu_talk2.png)

让它设一个定时任务，也就一句话的事：

![定时任务](../../images/feishu_talk3.png)

## 技术亮点

- **多 RTOS 支持**：通过 OSAL 抽象层，同一份代码在 FreeRTOS 和 RT-Thread 上零修改运行
- **编译时可裁剪**：每个模块（Shell、LCD、蜂群、调度器、各类工具）都可通过 menuconfig 独立开关，适配从 256KB 到 4MB 的不同硬件
- **Tool Use**：30+ 内置工具覆盖 GPIO 控制、系统监控、LCD 绘图、音频播放、定时任务、HTTP 请求、长期记忆，LLM 通过函数调用自主编排
- **对话记忆**：短期 RAM 环形缓冲保持上下文连贯，长期 NVS Flash 持久化存储让设备重启也不会遗忘；AI 可保存、删除、查询记忆
- **音频**：ES8311 编解码器驱动，预设音效（成功、错误、通知、警报）；AI 可控制音量和蜂鸣
- **技能系统**：预定义 Skill 模板（绘图、系统监控、问候），一句话触发复杂操作流程
- **蜂群智能**：UDP 心跳发现局域网内的其他节点，能力位图广播，跨节点远程工具调用
- **定时任务**：AI 可自主创建、查看、删除定时任务，周期性执行自动化操作
- **IM 集成**：飞书长连接，无需公网 IP，设备上电即在线

## 三步开始

```bash
# 1. 一键安装工具链
./scripts/setup-esp-env.sh

# 2. 选一个配置预设
cd platform/esp32c3
cp sdkconfig.defaults.demo sdkconfig.defaults

# 3. 配置 API Key，编译，运行
idf.py set-target esp32c3
idf.py menuconfig    # 设置你的 LLM API Key
idf.py build && idf.py qemu monitor
```

没有开发板也没关系——QEMU 全功能仿真，零成本体验完整流程。

## 加入我们

RT-Claw 刚刚起步，还有太多想做的事：更多模型 API 适配、Web 配置页面、更多 IM 通道、更多硬件平台。如果你对嵌入式 AI 感兴趣，欢迎参与：

- **GitHub**: [github.com/zevorn/rt-claw](https://github.com/zevorn/rt-claw)
- **Gitee**: [gitee.com/zevorn/rt-claw](https://gitee.com/zevorn/rt-claw)
- **QQ 群**: [GTOC 开源社区](https://qm.qq.com/q/heSPPC9De8)
- **Telegram**: [GTOC Channel](https://t.me/gevico_channel)

给个 Star，就是最好的支持。
