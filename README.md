# ESP32-S3 ST7789 虚拟数据显示模块

这个 Arduino 项目驱动 2.0 英寸、240×320、ST7789 SPI 彩屏，显示 IE14 RS485 传感器的光照、温度、大气压、湿度和海拔，以及真实 Wi-Fi 状态、本机 IP、设备编号和 MQTT 服务器配置。

## 接线

请断电接线，并默认使用 3.3V 给屏幕模块供电。

| LCD | ESP32-S3 | 说明 |
| --- | --- | --- |
| GND | GND | 电源地 |
| VCC | 3V3 | 屏幕电源 |
| SCL | GPIO12 | SPI SCLK |
| SDA | GPIO11 | SPI MOSI，不是 I2C SDA |
| RST | ESP32-S3 EN | 与主控同步硬件复位，不占 GPIO |
| DC | GPIO10 | 数据或命令选择 |
| CS | GPIO9 | SPI 片选 |

### RC522 RFID 读卡器

RC522 与屏幕共用 SPI 时钟和 MOSI，通过独立片选引脚避免冲突。RC522 只能使用 3.3V 供电。

| RC522 | ESP32-S3 | 说明 |
| --- | --- | --- |
| 3.3V | 3V3 | 禁止连接 5V |
| GND | GND | 公共地 |
| SCK | GPIO12 | 与屏幕共用 SPI 时钟 |
| MOSI | GPIO11 | 与屏幕共用 SPI MOSI |
| MISO | GPIO13 | RFID 数据输出 |
| SDA / SS | GPIO14 | RFID 独立片选 |
| RST | GPIO15 | RFID 复位 |
| IRQ | 不连接 | 当前使用轮询方式 |

程序使用该 TJDZ 模块配套示例中的兼容初始化参数。串口每秒输出一次寻卡状态：`no ISO14443A card detected` 表示射频阶段没有收到兼容卡片响应；`card detected, but UID read failed` 表示已经检测到卡片，但读取 UID 失败。该模块只能读取 13.56MHz ISO14443A 卡（例如 MIFARE S50），不能读取 125kHz ID 卡。

### IE14 RS485 传感器

IE14 必须通过 MAX3485、SP3485 或同类 3.3V TTL 转 RS485 收发器连接，不能将 A、B 线直接接到 ESP32。

| ESP32-S3 | RS485 收发器 | 说明 |
| --- | --- | --- |
| GPIO17 | DI / TX | UART1 发送 |
| GPIO18 | RO / RX | UART1 接收 |
| A | IE14 A | RS485 差分线 |
| B | IE14 B | RS485 差分线 |

当前使用自动收发方向控制的 RS-485 模块，因此不需要连接 DE/RE。GPIO16 预留给 MG90S 舵机信号。传感器使用外部 12-24V 供电。默认通信参数为从站地址 `0x01`、9600 8N1；程序每秒读取一次寄存器 `0x0065` 至 `0x006B`。

### 28BYJ-48 步进电机

步进电机必须通过 ULN2003 驱动板连接，不得直接连接 ESP32-S3 GPIO。驱动板使用外部 5V 供电，并与 ESP32-S3 共地。

| ULN2003 | ESP32-S3 | 说明 |
| --- | --- | --- |
| IN1 | GPIO39 | 电机相位 1 |
| IN2 | GPIO40 | 电机相位 2 |
| IN3 | GPIO41 | 电机相位 3 |
| IN4 | GPIO42 | 电机相位 4 |
| VCC | 外部 5V | 电机电源 |
| GND | GND | 与 ESP32-S3 共地 |

`StepperMotor` 使用八拍半步和非阻塞更新。正步数或正角度为一个方向，负值为反方向。示例：

```cpp
stepperMotor.moveSteps(2048);            // 转动约半圈
stepperMotor.moveDegrees(-90.0f, 2500);  // 反向转动约 90 度
stepperMotor.stop();                      // 停止并释放线圈
```

只有在 `stepperMotor.isBusy()` 为 `false` 时启动下一次动作。主循环必须持续调用 `stepperMotor.update(micros())`。

### PS2 摇杆

摇杆必须使用 ESP32-S3 的 3.3V 供电，不能直接以 5V 供电后将 X/Y 模拟输出接入 ESP32。

| 摇杆 | ESP32-S3 |
| --- | --- |
| V | 3V3 |
| G | GND |
| X / VRx | GPIO1 |
| Y / VRy | GPIO2 |
| B / SW | 不连接 |

程序以 12 位 ADC（0-4095）读取 X/Y；摇杆按键不使用，GPIO5 分配给第二路继电器。

### EC11 旋转编码器

| 编码器 | ESP32-S3 |
| --- | --- |
| V | 3V3 |
| G | GND |
| A | GPIO6 |
| B | GPIO7 |
| S | GPIO38 |

编码器以 A/B 相位判断方向；每 4 个有效相位边沿计为 1 步，顺时针为正、逆时针为负。按下编码器时，`ENCODER` 行追加显示 `PUSH`。

### MAX4466 声音传感器

本项目使用 MAX4466 模块（不是 MAX4467/MAX4468）。模块以 GPIO8 的 ADC 峰峰值计算 `MIC LEVEL`，用于显示实时声音活动强度；它不是校准后的分贝值。

| MAX4466 | ESP32-S3 | 说明 |
| --- | --- | --- |
| VCC | 3V3 | 模块供电，禁止接 5V 后将 OUT 接入 ESP32 |
| GND | GND | 公共地 |
| OUT | GPIO8 | 模拟音频输出 |

模块增益可由板载电位器调节；若正常环境下读数长期为 0 或接近 100%，请调节电位器后再测试。

### 其他输出引脚

以下引脚已经集中定义在 `PeripheralPins.h`，但对应控制功能尚未接入主循环；在确认继电器触发电平、电机驱动方式和舵机动作范围后再启用。

| 外设 | 模块端 | ESP32-S3 | 供电要求 |
| --- | --- | --- | --- |
| 两路继电器 | IN1 | GPIO4 | 模块外部 5V，确认支持 3.3V 控制 |
| 两路继电器 | IN2 | GPIO5 | 模块外部 5V，确认支持 3.3V 控制 |
| MG90S | SIGNAL | GPIO16 | 舵机外部稳定 5V |
| WS2812B | DIN | GPIO21 | 矩阵外部 5V |
| 直流电机驱动 | IN1 | GPIO47 | 电机按额定电压外部供电 |
| 直流电机驱动 | IN2 | GPIO48 | 电机按额定电压外部供电 |

所有外部电源必须与 ESP32-S3 共地。GPIO43、GPIO44 保留给 UART0，不分配给普通外设。

## Arduino IDE 配置

1. 开发板选择 `ESP32S3 Dev Module`。
2. Flash Size 选择 `16MB`。
3. PSRAM 选择 `OPI PSRAM`。
4. 安装 `Adafruit GFX Library`。
5. 安装 `Adafruit ST7735 and ST7789 Library`。
6. 安装 `MFRC522` 库。
7. 打开 `iot-controller.ino`，选择开发板串口并上传。

## 屏幕内容

- `TEMP`：IE14 实时温度。
- `HUMIDITY`：IE14 实时湿度。
- `PRESSURE`：IE14 实时大气压。
- `LIGHT`：IE14 实时光照。
- `ALTITUDE`：IE14 根据气压计算的海拔。
- `JOYSTICK X / Y`：PS2 摇杆实时坐标。
- `MIC LEVEL`：MAX4466 实时声音活动强度（0-100%）。
- `ENCODER`：EC11 累计步数和按压状态。
- `OFFLINE / LIVE`：正在连接或 Wi-Fi 已经断开。
- `SSID / IP`：显示真实网络名称和 ESP32-S3 获得的 IPv4 地址。
- `MQTT`：当前显示 `NOT SET`，等确定 MQTT 服务器后再配置。

传感器每秒轮询一次；界面每 500 毫秒刷新一次。第二页的 `SENSOR` 字段显示 RS485 通信状态。

## 后续接入真实模块

IE14 数据源由 `Rs485EnvironmentDataSource` 管理。它在 UART1 上发送 Modbus 请求 `01 03 00 65 00 07 14 17`，校验从站地址、功能码、数据长度和 CRC 后，再将环境数据更新到屏幕。`MockDataSource` 仅保留给测试使用。

真实 Wi-Fi 由 `WiFiDataSource` 管理。`begin()` 启动连接，`poll()` 检查状态并每 10 秒自动重连，`readNetwork()` 向界面提供连接状态、SSID 和 IP。Wi-Fi 名称和密码保存在 `WifiCredentials.h`。

ESP32-S3 只支持 2.4GHz Wi-Fi。当前配置使用实验室的 2.4GHz 网络 `773`，不能使用 `773_5G`。

全部外设引脚集中定义在 `PeripheralPins.h`。LCD 的显示参数保留在 `DashboardView.h`，RS485 的通信参数保留在 `Rs485EnvironmentDataSource.h`。

## 首次上板检查

- 白屏：检查 VCC、GND、CS、DC、EN/RST 和 SPI 引脚，确认屏幕确实是 ST7789。
- 画面旋转：调整 `DashboardConfig::kRotation`，可选值为 0、1、2、3。
- 颜色红蓝颠倒或画面偏移：需要根据具体屏幕模组调整 ST7789 初始化参数。
- 反复重启：检查供电和串口启动日志，先断开其他大电流执行器。
