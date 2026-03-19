# CT-Watcher

![ESP-IDF](https://img.shields.io/badge/ESP--IDF-v5.2.2-blue.svg)
![Target](https://img.shields.io/badge/Target-ESP32--S3-orange.svg)
![Provisioning](https://img.shields.io/badge/Provisioning-BLE%20%2B%20WebUI-success.svg)
![License](https://img.shields.io/badge/License-MIT-green.svg)

**CT-Watcher** 是一款基于 ESP32-S3 和 **ESP-IDF v5.2.2** 框架开发的安防与环境监测终端。

本项目实现了人体移动检测、本地声光报警以及室内温湿度环境监测等功能。网络层采用混合配网 (Hybrid Provisioning) 架构，系统已实现网络与云端参数的解耦，无需在源码中硬编码配置信息。设备通过 MQTT 协议与物联网云平台进行双向通信，系统内置中国电信 CTWing 平台的默认连接参数以支持快速接入，同时提供本地 Web 服务以支持私有化云平台的参数覆写。

---

## 核心特性 (Key Features)

* **网络配置与异常恢复**：支持通过蓝牙 (BLE) 进行网络配置。系统内置网络容错机制，发生致命错误时系统将进行保护性挂起；运行时若发生断网且连续重试 5 次失败，OLED 屏幕将自动进入配网引导模式，提升了设备的异常恢复能力。
* **云端互联与动态配置**：系统默认集成 CTWing 平台的 MQTT 连接参数，配网成功后即可自动上线。同时，系统在后台运行局域网 Web 服务器，允许用户通过浏览器动态修改 MQTT 参数（如 Server URI、Client ID 等）以接入其他私有云平台，配置数据经加密后存储于 NVS 闪存中。
* **安防监测与声光报警**：集成 PIR 人体红外传感器用于防区入侵检测。系统内置 `esp_audio_codec` 解码库，结合 SPIFFS 文件系统与 MAX98357A (I2S) 功放，实现本地 MP3 音频解码播报；并联动基于 RMT 驱动的 WS2812B RGB 指示灯输出状态反馈。
* **可视化交互界面**：集成 128x64 OLED 屏幕，底层采用 `U8g2` 图形库并挂载中文字库。实现了开机自检进度的可视化显示（Checklist），以及运行期的多数据看板功能（包含网络状态、IP 地址、布防状态及温湿度数据）。
* **按键控制逻辑**：支持多路独立按键输入，底层实现软件防抖处理，提供本地一键布防/撤防、系统测试及 SOS 紧急求助等业务逻辑接口。

---

## 硬件引脚分配 (Pinout Map)

![CT-Watcher](./README.assets/CT-Watcher.png)

*进行硬件连接或 PCB 设计时，请参考下表。注意：所有外设模块的 GND 必须与 ESP32 共地；功放及 RGB 灯带等大功率外设建议采用独立的 5V 电源供电。*

| 硬件模块 | 信号线 | ESP32-S3 引脚 | 接线备注说明 |
| :--- | :--- | :--- | :--- |
| **独立按键 (BOOT)** | 输入 | **GPIO 0** | 系统保留引脚，上电初始化阶段不可拉低。 |
| **PIR 人体传感器** | OUT | **GPIO 1** | 硬件高低电平输入检测。 |
| **按键 1 (布防/撤防)**| 输入 | **GPIO 4** | 一端接引脚，另一端接 GND（开启内部上拉）。 |
| **I2S 功放 (MAX98357)**| BCLK | **GPIO 5** | I2S 比特时钟信号。 |
| **I2S 功放 (MAX98357)**| LRC/WS | **GPIO 6** | I2S 字选择/帧同步信号。 |
| **I2S 功放 (MAX98357)**| DIN | **GPIO 7** | I2S 音频数据输出。 |
| **OLED 屏幕 (I2C)** | SDA | **GPIO 8** | I2C 数据线 (U8g2 驱动)。 |
| **OLED 屏幕 (I2C)** | SCL | **GPIO 9** | I2C 时钟线 (U8g2 驱动)。 |
| **按键 2 (测试喇叭)** | 输入 | **GPIO 15** | 一端接引脚，另一端接 GND（开启内部上拉）。 |
| **按键 3 (SOS求助)** | 输入 | **GPIO 16** | 一端接引脚，另一端接 GND（开启内部上拉）。 |
| **DHT11 温湿度** | DATA | **GPIO 38** | 单总线通信数据线。 |
| **WS2812B 幻彩灯** | DIN | **GPIO 48** | RMT 外设驱动信号线。 |

---

## 软件架构 (Software Architecture)

系统基于 FreeRTOS 构建，采用解耦的多任务架构以保障各模块的实时性：
1. **OLED Display Task**：负责开机自检状态的渲染及运行期仪表盘数据的周期性刷新（基于 U8g2）。
2. **Prov & Web Task**：负责底层蓝牙配网服务的拉起、NVS 存储的读写操作，以及局域网 HTTP 配置服务器的后台监听。
3. **MQTT Report Task**：负责与云端的双向通信，实现基于“定时心跳、数据突变、状态翻转”的条件触发式上报逻辑，并异步解析云端下发的 JSON 控制指令。
4. **Audio & PIR Task**：周期轮询 PIR 传感器状态，触发入侵事件时联动屏幕状态更新、阻塞执行 MP3 解码播放及 LED 报警。
5. **Key Scan Task**：(规划中) 执行 10ms 周期的高频轮询与软件防抖，处理本地硬件交互事件。

---

## 快速部署 (Getting Started)

### 1. 编译与烧录

本项目集成了音频文件及中文字库，需挂载 SPIFFS 文件系统，因此必须启用自定义分区表：
1. 在终端运行配置菜单：`idf.py menuconfig`
2. 导航至 `Partition Table` -> 选择 `Custom partition table CSV`，确保引用的文件名为 `partitions.csv`。
3. 连接开发板，执行编译与烧录命令：
```bash
idf.py set-target esp32s3
idf.py build flash monitor
```

### 2. 网络配置流程 (BLE Provisioning)

当设备首次开机或由于网络变更导致连续 5 次重连失败时，屏幕将自动进入“配网引导界面”。
1. 打开移动端设备的蓝牙设置界面。
2. 扫描并连接名称为 **`CT-Watcher`** 的蓝牙设备。
3. 若系统请求配对密钥，请输入：**`DSL12345`**。
4. 蓝牙连接建立后，通过移动端界面选择目标 2.4G Wi-Fi 并输入凭证。
5. 网络配置完成后，设备将获取局域网 IP，屏幕恢复至标准数据看板。
6. 设备将自动使用内置参数连接 CTWing 云平台并启动数据上报。

### 3. 私有化云端部署 (进阶配置)

若需将设备迁移至用户自建的 MQTT 代理服务器（如 Home Assistant、EMQX 等），可通过 Web UI 覆写默认配置：
1. 确保操作终端（PC/手机）与 ESP32 处于同一局域网。
2. 在浏览器地址栏输入 OLED 屏幕显示的 **局域网 IP 地址**。
3. 访问 Web 页面，在表单中录入私有云的 `MQTT Server URI`、`Client ID` 及鉴权信息。
4. 提交表单后，系统会将新参数持久化至 NVS 并自动触发软复位，随后设备将连接至指定的私有云平台。
