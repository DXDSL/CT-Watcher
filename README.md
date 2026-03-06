# CT-Watcher

![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.2.2-blue.svg)
![Target](https://img.shields.io/badge/Target-ESP32--S3-orange.svg)
![Provisioning](https://img.shields.io/badge/Provisioning-BLE%20%2B%20WebUI-success.svg)
![License](https://img.shields.io/badge/License-MIT-green.svg)

**CT-Watcher** 是一个基于 ESP32-S3 和 **ESP-IDF v5.2.2** 框架开发的高级智能安防与环境监测终端。

本项目不仅具备高灵敏度的人体移动检测与本地声光报警功能，还能实时监测室内温湿度。设备采用了商业级的“混合配网 (Hybrid Provisioning)”架构，无需在源码中硬编码任何账号密码，通过标准的 MQTT 协议与**中国电信 CTWing 物联网平台**进行实时双向通信，实现真正的无感部署与远程监控。

---

## ✨ 核心亮点 (Key Features)

* 🔗 **双模混合配网 (无硬编码)**：支持使用官方 APP 通过 **蓝牙 (BLE)** 无感配置 Wi-Fi。连网后后台静默拉起 **Web 服务器**，通过局域网网页动态下发 MQTT 参数，断网自动重试与无缝唤醒，配置永久加密保存于 NVS 闪存。
* 📡 **CTWing 云端互联**：基于 MQTT 协议接入电信平台（一机一密），支持心跳包机制、环境数据突变即时上报、以及云端指令实时下发。
* 🏃‍♂️ **人体行为感知**：集成 PIR 人体红外传感器，精准检测防区入侵行为。
* 🔊 **高保真语音告警**：内置 `esp_audio_codec` 解码库，结合 SPIFFS 独立分区与 MAX98357A (I2S) 功放，实现无卡顿的本地 MP3 语音/警报播报。
* 🌈 **幻彩状态指示**：集成 WS2812B RGB 幻彩灯珠，通过 RMT 硬件驱动，用不同颜色与呼吸频率直观反映系统当前状态。
* 🖥️ **OLED 可视化大屏**：实时展示网络状态、局域网 IP、布防模式、实时温湿度及突发事件全屏弹窗。
* 🕹️ **多维度按键交互**：支持本地硬件防抖的多按键输入，提供一键布防/撤防、本地试鸣、一键 SOS 紧急求助等实用功能。

---

## 🔌 硬件引脚分配 (Pinout Map)

![CT-Watcher](./README.assets/CT-Watcher.png)

*接线或绘制 PCB 时请严格参考下表。**注意：所有外设模块的 GND 必须与 ESP32 共地，大功率外设（如扬声器、灯带）建议独立 5V 供电。***

| 硬件模块 | 信号线 | ESP32-S3 引脚 | 接线备注说明 |
| :--- | :--- | :--- | :--- |
| **独立按键 (BOOT)** | 输入 | **GPIO 0** | 系统保留，上电瞬间切勿按下。 |
| **PIR 人体传感器** | OUT | **GPIO 1** | 硬件高低电平输入检测。 |
| **按键 1 (布防/撤防)**| 输入 | **GPIO 4** | 一端接引脚，另一端接 GND（内部上拉）。 |
| **I2S 功放 (MAX98357)**| BCLK | **GPIO 5** | I2S 比特时钟信号。 |
| **I2S 功放 (MAX98357)**| LRC/WS | **GPIO 6** | I2S 字选择/帧同步信号。 |
| **I2S 功放 (MAX98357)**| DIN | **GPIO 7** | I2S 音频数据输出。 |
| **OLED 屏幕 (I2C)** | SDA | **GPIO 8** | 软件模拟 I2C 数据线。 |
| **OLED 屏幕 (I2C)** | SCL | **GPIO 9** | 软件模拟 I2C 时钟线。 |
| **按键 2 (测试喇叭)** | 输入 | **GPIO 15** | 一端接引脚，另一端接 GND（内部上拉）。 |
| **按键 3 (SOS求助)** | 输入 | **GPIO 16** | 一端接引脚，另一端接 GND（内部上拉）。 |
| **DHT11 温湿度** | DATA | **GPIO 38** | 单总线通信数据线。 |
| **WS2812B 幻彩灯** | DIN | **GPIO 48** | 采用底层 RMT 驱动。 |

---

## ⚙️ 软件架构 (Software Architecture)

系统采用 FreeRTOS 解耦的多任务架构，确保音频解码、屏幕刷新与网络通信互不阻塞：
1.  **Prov & Web Task**：负责底层蓝牙配网的拉起、NVS 读写，以及局域网 HTTP 配置服务器的后台驻留。
2.  **MQTT Report Task**：实现“定时心跳 + 数据突变 + 状态翻转”的三重智能上报逻辑。
3.  **Audio & PIR Task**：轮询传感器状态，触发时阻塞式解码播放 MP3，并联动 LED 声光报警。
4.  **Key Scan Task**：10ms 高频轮询，执行软件防抖及触发本地交互逻辑。
5.  **OLED Display Task**：异步刷新 UI 界面，动态显示 IP 地址及传感器数据。

---

## 🚀 快速上手 (Getting Started)

### 1. 编译与烧录
本项目**无需在代码中修改任何 Wi-Fi 或 MQTT 密码**,直接编译烧录即可。

由于项目中包含 `people.mp3` 本地语音文件，必须启用自定义分区表：
1. 在终端运行：`idf.py menuconfig`
2. 导航至 `Partition Table` -> 选择 `Custom partition table CSV`，确保文件名为 `partitions.csv`。
3. 将开发板连接电脑，执行编译烧录：
```bash
idf.py set-target esp32s3
idf.py flash monitor
```

### 2. 第一步：蓝牙无感配网 (连接 Wi-Fi)

1. 下载并打开手机 APP：**ESP BLE Provisioning**
2. 点击 `Provision Device` -> `BLE`。
3. 找到并连接名为 **`CT-Watcher`** 的蓝牙设备。
4. 提示输入 Pop 验证码时，输入：**`DSL12345`**。
5. 选择您家里的 2.4G Wi-Fi 并输入密码，点击完成。
6. 设备将自动连上路由器，并在电脑串口 / OLED 屏幕上显示其获取到的**局域网 IP 地址**（例如：`192.168.1.100`）。

### 3. 第二步：局域网 Web 配置 (连接 CTWing 云端)

1. 确保您的电脑/手机与 ESP32 连在同一个路由器下。
2. 打开浏览器，直接在地址栏输入设备刚刚获取到的 **IP 地址**。
3. 此时会弹出一个精美的 **“CT-Watcher 云端配置”** 界面。
4. 填入您在 CTWing 平台申请的 `产品 ID`、`设备 ID` 和 `特征串`。
5. 点击“保存并连接”，设备将自动保存参数、重启并丝滑接入中国电信物联网云平台！

