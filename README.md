| 支持的目标芯片 | ESP32 | ESP32-C2 | ESP32-C3 | ESP32-C6 | ESP32-H2 | ESP32-P4 | ESP32-S2 | ESP32-S3 |
| ---------- | ----- | -------- | -------- | -------- | -------- | -------- | -------- | -------- |

# CT-Watcher (智能安防-人体感知器)

本项目是一个基于 ESP32/ESP32-S3 和 **ESP-IDF v5.2.2** 框架开发的物联网智能安防监控设备。
设备通过人体传感器检测人员进入，在触发时通过本地 I2S 功放播放 MP3 语音警报，并同步通过 MQTT 协议将状态上报至**中国电信 CTWing 物联网平台**。

## ✨ 主要功能

* **网络通信**：支持 Wi-Fi (STA 模式) 连接路由器。
* **平台接入**：基于标准 MQTT 协议接入电信 CTWing 平台，采用“一机一密”特征串认证，支持透传 JSON 格式数据上报与指令下发订阅。
* **行为感知**：集成人体传感器 (HumanIR)，实现高灵敏度的人体移动检测。
* **语音告警**：集成 `esp_audio_codec` 音频解码库，本地 SPIFFS 挂载 `.mp3` 音频文件，通过 I2S 总线驱动喇叭实现本地声光报警。
* **环境与外设支持**：底层已集成 DHT11 温湿度读取、OLED 屏幕显示、LED 状态指示及独立按键检测（可按需扩展业务逻辑）。

## 📂 目录结构

```text
CT-Watcher/
├── main/
│   ├── AUDIO/          # I2S 音频初始化及播放逻辑 (bsp_Audio.c/.h)
│   ├── Hardware/       # 外设硬件驱动层
│   │   ├── DHT11/      # 温湿度传感器驱动
│   │   ├── HumanIR/    # 人体红外传感器驱动
│   │   ├── Key/        # 独立按键驱动
│   │   ├── Led/        # LED 驱动
│   │   └── OLED/       # OLED 屏幕驱动及字库
│   ├── NETWORK/        # 网络协议栈
│   │   ├── MQTT/       # MQTT 连接及数据交互任务
│   │   └── STA/        # Wi-Fi STA 模式初始化
│   ├── CMakeLists.txt  # Main 组件构建脚本
│   └── main.c          # 应用程序入口及多任务调度
├── spiffs/             # 存放要打包进 SPIFFS 分区的文件 (如 people.mp3)
├── partitions.csv      # 自定义分区表 (包含 NVS, PHY, Factory 和 storage)
├── CMakeLists.txt      # 工程顶级构建脚本
└── README.md           # 工程说明文档