# ESP32-S3 ST7789 虚拟数据显示模块

这个 Arduino 项目驱动 2.0 英寸、240×320、ST7789 SPI 彩屏，显示虚拟温度和湿度，以及真实 Wi-Fi 状态、本机 IP、设备编号和 MQTT 服务器配置。数据接口已经分离，后续接入真实温湿度传感器时不需要重写界面。

## 接线

请断电接线，并默认使用 3.3V 给屏幕模块供电。

| LCD | ESP32-S3 | 说明 |
| --- | --- | --- |
| GND | GND | 电源地 |
| VCC | 3V3 | 屏幕电源 |
| SCL | GPIO12 | SPI SCLK |
| SDA | GPIO11 | SPI MOSI，不是 I2C SDA |
| RST | GPIO10 | 屏幕复位 |
| DC | GPIO9 | 数据或命令选择 |
| CS | GPIO8 | SPI 片选 |

## Arduino IDE 配置

1. 开发板选择 `ESP32S3 Dev Module`。
2. Flash Size 选择 `16MB`。
3. PSRAM 选择 `OPI PSRAM`。
4. 安装 `Adafruit GFX Library`。
5. 安装 `Adafruit ST7735 and ST7789 Library`。
6. 打开 `iot-controller.ino`，选择开发板串口并上传。

## 屏幕内容

- `TEMPERATURE`：23.0～27.0°C 之间缓慢变化的虚拟温度。
- `HUMIDITY`：50.0～66.0% 之间缓慢变化的虚拟湿度。
- `WIFI OK / LIVE`：ESP32-S3 已经连接 Wi-Fi。
- `OFFLINE / LIVE`：正在连接或 Wi-Fi 已经断开。
- `SSID / IP`：显示真实网络名称和 ESP32-S3 获得的 IPv4 地址。
- `MQTT`：当前显示 `NOT SET`，等确定 MQTT 服务器后再配置。

界面每 500 毫秒读取一次数据，但字段没有变化时不会重新绘制，避免频繁全屏刷新造成闪烁。

## 后续接入真实模块

真实温湿度接入点位于 `MockDataSource::readEnvironment()`。可以保留 `EnvironmentData` 返回类型，将函数内部替换为 SHT30、AHT20 或 BME280 读取代码。

真实 Wi-Fi 由 `WiFiDataSource` 管理。`begin()` 启动连接，`poll()` 检查状态并每 10 秒自动重连，`readNetwork()` 向界面提供连接状态、SSID 和 IP。Wi-Fi 名称和密码保存在 `WifiCredentials.h`。

ESP32-S3 只支持 2.4GHz Wi-Fi。当前配置使用实验室的 2.4GHz 网络 `773`，不能使用 `773_5G`。

LCD 引脚集中定义在 `DashboardView.h` 的 `DashboardConfig` 中。如果改变接线，只需要修改该区域。

## 首次上板检查

- 白屏：检查 VCC、GND、CS、DC、RST 和 SPI 引脚，确认屏幕确实是 ST7789。
- 画面旋转：调整 `DashboardConfig::kRotation`，可选值为 0、1、2、3。
- 颜色红蓝颠倒或画面偏移：需要根据具体屏幕模组调整 ST7789 初始化参数。
- 反复重启：检查供电和串口启动日志，先断开其他大电流执行器。
