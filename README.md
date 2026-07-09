# ESP32 智能语音分类垃圾桶

基于 ESP32 + ESP-IDF 的智能语音分类垃圾桶项目。支持**语音识别**、**蓝牙 APP**、**按键**、**红外感应**四种方式自动开关垃圾桶盖，并检测垃圾桶是否已满。

## 功能特性

- 🎙️ **语音控制** — 天问 ASRPRO 语音模块，唤醒词"聪明蛋"，支持 8 种语音命令
- 📱 **蓝牙控制** — 手机 APP 通过蓝牙 SPP 发送指令控制
- 🔘 **按键控制** — 4 个独立按键，一按即开
- 👋 **红外感应** — 手靠近自动开盖，无需接触
- 📏 **满桶检测** — 超声波测距，距离 < 10cm 判定为已满
- 🖥️ **OLED 显示** — 实时显示 4 个垃圾桶的开关状态和满桶状态
- ⏱️ **自动关闭** — 打开 10 秒后自动关闭 (可配置)
- ⚙️ **menuconfig 可配置** — 引脚、角度、延时等全部可通过菜单调整

## 硬件清单

| 硬件 | 型号 | 数量 |
|------|------|------|
| 主控 | ESP32-WROOM-32 开发板 | 1 |
| 语音模块 | 天问 ASRPRO | 1 |
| 舵机驱动板 | PCA9685 (I2C) | 1 |
| IO 扩展 | PCF8574 (I2C, 0x20) | 1 |
| 舵机 | SG90/MG996R (180°) | 4 |
| 超声波传感器 | HC-SR04 | 4 |
| 红外反射传感器 | TCRT5000 模块 | 4 |
| OLED 显示屏 | SSD1306 128×64 (I2C, 0x3C) | 1 |
| 按键 | 微动开关 | 4 |
| 电源 | 5V/3A | 1 |

> 详细接线请参考 [docs/hardware.md](docs/hardware.md)

## 软件架构

```
main/
├── main.c              # 入口，初始化硬件 + 启动 FreeRTOS 任务
├── config.h            # 全局配置 (引脚/参数)
├── Kconfig.projbuild   # menuconfig 菜单定义
├── drivers/            # 硬件驱动层 (HAL)
│   ├── pca9685.c       # PCA9685 舵机驱动 (I2C)
│   ├── pcf8574.c       # PCF8574 IO 扩展 (红外传感器)
│   ├── hc_sr04.c       # HC-SR04 超声波 (esp_timer 微秒级测距)
│   ├── ssd1306.c       # SSD1306 OLED (u8g2)
│   └── button.c        # 按键 (GPIO ISR + 消抖)
├── services/           # 业务逻辑层
│   ├── event_bus.c     # 事件总线 (Queue + Event Group)
│   └── bin_manager.c   # 垃圾桶状态机 + 自动关闭 + Sensor/Button/Servo 任务
├── comm/               # 通信层
│   ├── voice_uart.c    # 天问 ASRPRO UART 通信
│   └── bluetooth_spp.c # 蓝牙 SPP (Bluedroid)
└── ui/
    └── display.c       # OLED 显示任务
```

### FreeRTOS 任务架构

```
┌──────────┐ ┌──────────┐ ┌──────────┐ ┌──────────┐
│ Button   │ │ Sensor   │ │ Voice    │ │ BT SPP   │
│ Task     │ │ Task     │ │ Task     │ │ Task     │
│ (Prio 4) │ │ (Prio 5) │ │ (Prio 4) │ │ (Prio 3) │
└────┬─────┘ └────┬─────┘ └────┬─────┘ └────┬─────┘
     └────────────┼────────────┼────────────┘
                  ▼            ▼
           ┌─────────────────────┐
           │    Event Bus        │
           │  (Queue + EvtGrp)   │
           └─────────┬───────────┘
                     ▼
           ┌─────────────────────┐
           │   Servo Task        │
           │   (Prio 6)          │
           │   PCA9685 + Timer   │
           └─────────┬───────────┘
                     ▼
           ┌─────────────────────┐
           │   Display Task      │
           │   (Prio 2)          │
           │   SSD1306 OLED      │
           └─────────────────────┘
```

## 快速开始

### 环境要求

- [ESP-IDF v5.0+](https://docs.espressif.com/projects/esp-idf/zh_CN/latest/esp32/get-started/index.html)
- ESP32-WROOM-32 开发板

### 编译 & 烧录

```bash
# 1. 克隆项目
git clone <your-repo-url>
cd ESP32智能语音垃圾桶

# 2. 设置目标芯片
idf.py set-target esp32

# 3. 配置 (可选, 修改引脚/角度等)
idf.py menuconfig
# → 进入 "智能分类垃圾桶配置" 菜单

# 4. 编译
idf.py build

# 5. 烧录 (替换 PORT 为你的串口号)
idf.py -p /dev/ttyUSB0 flash monitor
```

### 首次使用

1. 上电后 ESP32 蓝牙设备名 `ESP32_SmartBin`，手机可搜索连接
2. OLED 屏幕显示 4 个垃圾桶当前状态
3. 语音唤醒: 说"聪明蛋" → 听到回复后说垃圾类别名称
4. 按键/红外/蓝牙方式开盖，10 秒后自动关闭

## 配置项

通过 `idf.py menuconfig` → `智能分类垃圾桶配置` 可调整:

| 配置项 | 默认值 | 说明 |
|--------|--------|------|
| 按键引脚 | 15, 4, 18, 19 | 4 路按键 GPIO |
| 超声波引脚 | 13/12, 14/27, 26/25, 33/32 | TRIG/ECHO |
| 语音模块 UART | UART2, TX=17, RX=16, 115200 | |
| I2C 引脚 | SDA=21, SCL=22 | |
| 舵机通道 | CH15, CH11, CH7, CH3 | PCA9685 通道 |
| 打开/关闭角度 | 80°/0° | |
| 自动关闭延时 | 10 秒 | |
| 满桶阈值 | 10 cm | 超声波距离 |
| OLED 刷新间隔 | 500 ms | |

## 语音命令

| 命令 | 打开垃圾桶 |
|------|-----------|
| "可回收垃圾" / "旧书本" | 🟦 可回收 |
| "厨余垃圾" / "苹果" | 🟩 厨余 |
| "有害垃圾" / "电池" | 🟥 有害 |
| "其他垃圾" / "石头" | 🟨 其他 |

> 唤醒词: **"聪明蛋"**

完整语音协议见 [docs/voice-protocol.md](docs/voice-protocol.md)

## License

MIT License — 详见 [LICENSE](LICENSE)
