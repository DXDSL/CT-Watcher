# CT-Watcher (智能安防-人体行为感知器)

![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.2.2-blue.svg)
![Target](https://img.shields.io/badge/Target-ESP32--S3-orange.svg)
![License](https://img.shields.io/badge/License-MIT-green.svg)

**CT-Watcher** 是一个基于 ESP32-S3 和 **ESP-IDF v5.2.2** 框架开发的多功能智能安防与环境监测终端。
设备不仅具备高灵敏度的人体移动检测与本地声光报警功能，还能作为室内温湿度计使用。通过标准的 MQTT 协议，设备能够与**中国电信 CTWing 物联网平台**进行实时双向通信，实现远程监控与告警推送。

---

## ✨ 核心功能 (Key Features)

* 📡 **云端互联**：基于 Wi-Fi (STA) 与 MQTT 协议接入 CTWing 平台（透传模式/一机一密），支持环境数据循环上报与云端指令实时下发。
* 🏃‍♂️ **行为感知**：集成 PIR 人体红外传感器，精准检测入侵行为。
* 🔊 **高保真语音告警**：内置 `esp_audio_codec` 解码库，结合 SPIFFS 分区与 MAX98357A (I2S) 功放，实现无卡顿的本地 MP3 语音/警报播放。
* 🌈 **幻彩状态指示**：集成 WS2812B RGB 幻彩灯珠，通过不同颜色与闪烁频率（如绿灯待机、红灯狂闪）直观反映系统状态。
* 🖥️ **OLED 可视化大屏**：实时展示网络状态、布防模式、实时温湿度及突发事件全屏弹窗。
* 🕹️ **多维度按键交互**：支持本地硬件防抖的多按键输入，提供一键布防/撤防、本地试鸣、一键 SOS 紧急求助等实用功能。

---

## 🔌 硬件引脚分配 (Pinout Map)

![CT-Watcher](./README.assets/CT-Watcher.png)

请在接线或绘制 PCB 时严格参考下表。**注意：所有外设模块的 GND 必须与 ESP32 共地。**

| 硬件模块 | 信号线 | ESP32-S3 引脚 | 接线备注说明 |
| :--- | :--- | :--- | :--- |
| **独立按键 (BOOT)** | 输入 | **GPIO 0** | 原有系统保留，上电瞬间切勿按下。 |
| **PIR 人体传感器** | OUT | **GPIO 1** | 高低电平输入检测。 |
| **按键 1 (布防/撤防)**| 输入 | **GPIO 4** | 一端接 GPIO 4，另一端接 GND（内部上拉）。 |
| **I2S 功放 (MAX98357)**| BCLK | **GPIO 5** | I2S 比特时钟信号。*(建议接 5V 供电)* |
| **I2S 功放 (MAX98357)**| LRC/WS | **GPIO 6** | I2S 字选择/帧同步信号。 |
| **I2S 功放 (MAX98357)**| DIN | **GPIO 7** | I2S 音频数据输出。 |
| **OLED 屏幕 (I2C)** | SDA | **GPIO 8** | 软件模拟 I2C 数据线。 |
| **OLED 屏幕 (I2C)** | SCL | **GPIO 9** | 软件模拟 I2C 时钟线。 |
| **按键 2 (测试喇叭)** | 输入 | **GPIO 15** | 一端接 GPIO 15，另一端接 GND（内部上拉）。 |
| **按键 3 (SOS求助)** | 输入 | **GPIO 16** | 一端接 GPIO 16，另一端接 GND（内部上拉）。 |
| **DHT11 温湿度** | DATA | **GPIO 38** | 单总线通信。 |
| **WS2812B 幻彩灯** | DIN | **GPIO 48** | 采用 RMT 驱动。*(建议接 5V 供电)* |

---

## ⚙️ 软件依赖与架构 (Software Architecture)

### 第三方组件依赖
项目使用了 ESP-IDF Component Manager 自动管理依赖，编译时会自动下载：
* `espressif/esp_audio_codec`：用于 MP3 等音频格式的高效解码。
* `espressif/led_strip`：用于通过 RMT 硬件驱动 WS2812B 幻彩灯带。

### FreeRTOS 多任务调度
系统采用解耦的多任务架构，确保音频播放、屏幕刷新与网络通信互不阻塞：
1.  **Main Task**：负责初始化 NVS、SPIFFS、外设 GPIO，并在 Wi-Fi 连接成功后启动 MQTT。
2.  **Audio & PIR Task** (`pir_audio_task`)：负责轮询传感器，触发时阻塞式解码播放 MP3，并联动 LED 报警。
3.  **MQTT Report Task** (`DataReport_Task`)：以固定频率向云端推送温湿度及系统心跳数据。
4.  **Key Scan Task** (`key_scan_task`)：10ms 高频轮询按键状态，执行软件防抖及触发本地交互逻辑。
5.  **OLED Task** (`oled_display_task`)：以 500ms 频率刷新屏幕 UI 界面。

---

## 🚀 快速上手 (Getting Started)

### 1. 修改网络与平台配置
在编译前，请打开工程源码修改您的凭证信息：
* **Wi-Fi 账号密码**：进入 `main/NETWORK/STA/wifi_sta.c` 进行修改。
* **MQTT 平台密钥**：进入 `main/NETWORK/MQTT/mqtt.c` 修改为您在 CTWing 申请的一机一密参数：
    ```c
    #define MQTT_ADDRESS  "mqtt://2000506597.non-nb.ctwing.cn:1883" 
    #define MQTT_CLIENT   "您的设备编号 (Device ID)" 
    #define MQTT_USERNAME "您的设备编号 (Device ID)" 
    #define MQTT_PASSWORD "您的特征串 (Device Secret)" 
    ```

### 2. 启用自定义分区表 (配置 SPIFFS)
由于项目中包含 `people.mp3` 本地语音文件，必须启用自定义分区表。
1. 在终端运行：`idf.py menuconfig`
2. 导航至 `Partition Table` -> 选择 `Custom partition table CSV`。
3. 确保 `Custom partition CSV file` 的值为 `partitions.csv`。
4. 保存并退出 (`S` -> `Q`)。

### 3. 编译与烧录

将 ESP32-S3 通过 USB 连接至电脑，执行以下命令：
```bash
# 设定目标芯片
idf.py set-target esp32s3

# 编译、烧录并打开串口监视器 (请将 COMx 替换为实际串口号)
idf.py -p COMx flash monitor