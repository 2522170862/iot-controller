# BLE Wi-Fi 配网模块复用与移植说明

本文说明如何把当前项目已经实现的 ESP32-S3 BLE Wi-Fi 配网功能移植到另一个 Arduino 工程，并继续使用当前 `uniapp-ble-provisioning` 微信小程序/Android App 完成配网。

本文中的示例凭据均为占位符。不要把真实 Wi-Fi 密码写入文档、日志或公开仓库。

## 1. 模块实现了什么

本模块由 ESP32-S3 固件端和 UniApp 手机端组成：

1. ESP32-S3 上电后优先读取 NVS 中最后一次验证成功的 Wi-Fi 凭据；
2. NVS 没有有效记录时，使用 Arduino 工程内的默认 Wi-Fi；
3. ESP32-S3 始终广播名称为 `IoT-Controller-XXXX` 的 BLE 设备；
4. 手机连接 BLE 后，调用手机系统扫描附近 Wi-Fi；
5. 用户选择 2.4GHz Wi-Fi，并通过 BLE 发送 SSID 和密码；
6. ESP32-S3 在主循环中非阻塞地验证新网络；
7. 成功后保存到 NVS，下次启动自动连接该网络；
8. 失败时不替换旧凭据，并重新连接切换前的网络；
9. Wi-Fi 切换期间其他外设主循环仍可继续运行；
10. MQTT 可根据实际 Wi-Fi 状态暂停并在网络恢复后重新连接。

当前手机端支持：

- 微信小程序；
- Android App；
- 手机扫描附近 Wi-Fi；
- BLE 设备搜索、连接、分包写入、通知接收和错误提示。

当前版本不支持 iOS。

## 2. 模块边界

### 2.1 固件端核心文件

移植时建议原样复制以下文件：

| 文件 | 用途 | 是否通常需要修改 |
| --- | --- | --- |
| `BleProvisioningProtocol.h/.cpp` | BLE UUID、JSON 协议、接收组帧和请求队列 | 仅修改 UUID 时改 |
| `BleProvisioningService.h/.cpp` | BLE 广播、GATT 服务、收发和通知队列 | 修改设备名前缀时改 |
| `WifiCredentialStore.h/.cpp` | NVS 凭据校验、双槽保存和清空 | NVS 命名冲突时改 |
| `WifiProvisioningCoordinator.h/.cpp` | 新网络验证、超时、保存和失败回滚 | 修改超时时间时改 |
| `WiFiDataSource.h/.cpp` | Wi-Fi 连接状态、切换和自动重连 | 按新工程网络接口调整 |

以下文件根据新工程需要选择：

| 文件 | 用途 |
| --- | --- |
| `WifiCredentials.h` | 保存首次启动使用的编译期默认 Wi-Fi |
| `ConnectionStatusIndicators.h/.cpp` | Wi-Fi、MQTT、BLE 三路指示灯 |
| `PeripheralPins.h` | 集中保存指示灯和其他 GPIO |
| `DashboardTypes.h` | 当前 `WiFiDataSource::readNetwork()` 返回的 `NetworkStatus` 类型 |

### 2.2 手机端文件

最稳妥的复用方式是复制整个目录：

```text
uniapp-ble-provisioning/
```

其中与配网逻辑直接相关的文件是：

| 文件 | 用途 |
| --- | --- |
| `utils/protocol.js` | UUID、请求 ID、JSON 编解码和 20 字节分包 |
| `services/ble-service.js` | BLE 扫描、连接、特征发现、写入和通知 |
| `services/wifi-service.js` | 微信/Android 手机 Wi-Fi 扫描和列表整理 |
| `pages/index/index.vue` | 配网页面、状态机和用户交互 |
| `manifest.json` | 微信和 Android 权限配置 |
| `pages.json` | 页面路由 |
| `index.html` | HBuilderX Vue 3/Vite 入口 |

## 3. Arduino 端依赖

目标工程使用 ESP32 Arduino Core，并需要：

- `WiFi.h`：ESP32 Arduino Core 自带；
- `Preferences.h`：ESP32 Arduino Core 自带；
- `BLEDevice.h`、`BLEServer.h`、`BLE2902.h`：ESP32 Arduino Core 自带的 BLE 库；
- `ArduinoJson`：需要在 Arduino IDE 库管理器中安装。

当前实现使用 ESP32 Arduino Core 自带 BLE API，不要同时换成 `NimBLE-Arduino` 或 `ArduinoBLE`，除非同步重写 `BleProvisioningService`。

Arduino IDE 推荐设置：

```text
开发板：ESP32S3 Dev Module
Flash Size：按实际模组选取，当前板为 16MB
PSRAM：当前 N16R8 板使用 OPI PSRAM
USB CDC On Boot：按开发板下载/串口方式配置
```

## 4. Arduino 端移植步骤

### 4.1 复制核心源码

把第 2.1 节的五组核心 `.h/.cpp` 文件复制到新 Arduino 工程的 `.ino` 同级目录。

Arduino IDE 会自动编译 `.ino` 同级目录中的 `.cpp` 文件。不要把这些文件复制到一个 Arduino IDE 不会参与编译的任意子目录。

### 4.2 处理 `DashboardTypes.h` 依赖

当前 `WiFiDataSource.h` 包含 `DashboardTypes.h`，原因是它提供：

```cpp
NetworkStatus WiFiDataSource::readNetwork() const;
```

有两种处理方式：

1. 推荐快速移植：同时复制 `DashboardTypes.h`；
2. 精简移植：删除 `readNetwork()` 及 `DashboardTypes.h` 依赖，改用：

```cpp
const bool wifiConnected =
    wifiDataSource.attemptState() == WifiAttemptState::kConnected;
const char* ip = wifiDataSource.localIp();
```

如果新工程不需要 LCD 显示网络状态，第二种方式依赖更少。

### 4.3 配置默认 Wi-Fi

新建或修改 `WifiCredentials.h`：

```cpp
#pragma once

namespace WifiCredentials {
constexpr char kSsid[] = "YOUR_DEFAULT_2G_WIFI";
constexpr char kPassword[] = "YOUR_DEFAULT_WIFI_PASSWORD";
}  // namespace WifiCredentials
```

默认凭据只在 NVS 没有有效配网记录时使用。BLE 配网成功后，NVS 凭据的优先级更高。

如果产品不应内置默认网络，可以把程序改为 NVS 无记录时不调用默认凭据连接，仅保持 BLE 广播；这种修改需要同时调整启动逻辑，不能简单把 SSID 留空，因为当前凭据策略会把空 SSID 判为无效。

### 4.4 在主程序中创建对象

在 `.ino` 顶部添加：

```cpp
#include "BleProvisioningService.h"
#include "WiFiDataSource.h"
#include "WifiCredentialStore.h"
#include "WifiCredentials.h"
#include "WifiProvisioningCoordinator.h"
```

创建全局对象：

```cpp
WiFiDataSource wifiDataSource;
WifiCredentialStore wifiCredentialStore;
WifiProvisioningCoordinator wifiProvisioningCoordinator(
    &wifiDataSource, &wifiCredentialStore);
BleProvisioningService bleProvisioningService;
```

这些对象需要在程序整个生命周期内存在，不要把协调器或 BLE 服务创建为 `setup()` 中的局部变量。

### 4.5 在 `setup()` 中初始化

将以下逻辑加入 `setup()`。应先选择启动凭据，再启动 Wi-Fi 和 BLE：

```cpp
WifiCredentialsValue savedCredentials = {};
WifiCredentialsValue fallbackCredentials = {};

snprintf(fallbackCredentials.ssid,
         sizeof(fallbackCredentials.ssid),
         "%s", WifiCredentials::kSsid);
snprintf(fallbackCredentials.password,
         sizeof(fallbackCredentials.password),
         "%s", WifiCredentials::kPassword);

const bool savedCredentialsPresent =
    wifiCredentialStore.load(&savedCredentials);

const WifiCredentialsValue bootCredentials =
    WifiCredentialPolicy::selectBootCredentials(
        savedCredentialsPresent,
        savedCredentials,
        fallbackCredentials);

wifiDataSource.begin(bootCredentials);
bleProvisioningService.begin();
```

注意：

- 不要在串口中打印 `bootCredentials.password`；
- `BleProvisioningService::begin()` 会开始 BLE 广播；
- BLE 广播在 Wi-Fi 已连接时仍然保留，方便以后再次换网；
- 当前广播名称由芯片 MAC 尾部生成，例如 `IoT-Controller-A1B2`。

### 4.6 在 `loop()` 中轮询

以下代码是必须保留的核心调度逻辑：

```cpp
void loop() {
  const uint32_t nowMs = millis();

  wifiDataSource.poll(nowMs);
  bleProvisioningService.poll(nowMs);

  BleWifiRequest request = {};
  while (bleProvisioningService.takeRequest(&request)) {
    if (!wifiProvisioningCoordinator.start(request, nowMs)) {
      WifiProvisioningEvent busyEvent = {};
      busyEvent.kind = WifiProvisioningEventKind::kFailed;
      snprintf(busyEvent.id, sizeof(busyEvent.id), "%s", request.id);
      snprintf(busyEvent.reason, sizeof(busyEvent.reason), "%s", "busy");
      bleProvisioningService.enqueueEvent(busyEvent);
    }
  }

  wifiProvisioningCoordinator.poll(nowMs);

  WifiProvisioningEvent event = {};
  while (wifiProvisioningCoordinator.takeEvent(&event)) {
    bleProvisioningService.enqueueEvent(event);
  }

  const bool wifiConnected =
      wifiDataSource.attemptState() == WifiAttemptState::kConnected;

  // 在这里继续执行 MQTT、传感器、显示和执行器轮询。
}
```

不得用下面的阻塞方式等待连接：

```cpp
// 不推荐：会阻塞 BLE、传感器、显示和其他任务。
while (WiFi.status() != WL_CONNECTED) {
  delay(500);
}
```

配网模块依赖 `loop()` 高频调用 `poll()`。如果主循环中有长时间 `delay()`、阻塞串口读取或电机动作，BLE 通知和 Wi-Fi 验证都会变慢。

### 4.7 接入 MQTT

BLE 配网模块不依赖具体 MQTT 库。新项目只需把真实 Wi-Fi 状态传给 MQTT 层：

```cpp
const bool wifiConnected =
    wifiDataSource.attemptState() == WifiAttemptState::kConnected;

mqttService.poll(nowMs, wifiConnected);
```

Wi-Fi 切换时状态会变为未连接，MQTT 应暂停连接或主动断开；新 Wi-Fi 成功或旧 Wi-Fi 回滚完成后，再按原来的重连策略连接 MQTT。

不要把“MQTT 已连接”作为 Wi-Fi 配网成功条件。当前设计中，只要 ESP32 成功获取 Wi-Fi IP，就保存新凭据；MQTT 可以随后继续重试。

### 4.8 接入状态灯（可选）

如果新项目继续使用当前三路灯，可复制 `ConnectionStatusIndicators.h/.cpp` 并创建：

```cpp
ConnectionStatusIndicators indicators(
    WIFI_LED_PIN,
    MQTT_LED_PIN,
    BLE_LED_PIN);
```

`setup()`：

```cpp
indicators.begin();
```

`loop()`：

```cpp
indicators.update(
    nowMs,
    wifiConnected,
    wifiProvisioningCoordinator.active(),
    mqttConnected,
    bleProvisioningService.connected());
```

当前逻辑：

- Wi-Fi 普通断开：Wi-Fi 灯灭；
- Wi-Fi 验证期间：Wi-Fi 灯每 250ms 切换一次；
- Wi-Fi 已连接：Wi-Fi 灯常亮；
- MQTT 已连接：MQTT 灯常亮；
- BLE 客户端已连接：BLE 灯常亮；
- `begin()` 后三路默认都是低电平。

当前工程使用 GPIO3、GPIO43、GPIO44。移植到新硬件时必须根据原理图重新选引脚。GPIO43/44 与 ESP32-S3 UART0 复用，使用它们做 LED 时建议通过原生 USB CDC 下载和查看日志。

## 5. Arduino 端可配置位置

### 5.1 BLE UUID

固件位置：

```text
BleProvisioningProtocol.h
```

当前 UUID：

```text
Service  6E400001-B5A3-F393-E0A9-E50E24DCCA9E
RX       6E400002-B5A3-F393-E0A9-E50E24DCCA9E
TX       6E400003-B5A3-F393-E0A9-E50E24DCCA9E
```

RX 是手机写入，TX 是 ESP32 通知。如果修改 UUID，必须同步修改小程序的 `utils/protocol.js`，否则手机能看到设备，但找不到配网服务或特征值。

同一批使用同一小程序的设备应保持 UUID 不变。只有与其他 BLE 产品发生冲突，或需要区分不兼容协议版本时才建议更换。

### 5.2 BLE 广播名称

固件位置：

```text
BleProvisioningService.cpp
```

当前格式：

```cpp
"IoT-Controller-%02X%02X"
```

如果改成例如 `Greenhouse-XXXX`，还必须修改小程序：

```text
uniapp-ble-provisioning/utils/protocol.js
```

```js
export const BLE_DEVICE_NAME_PREFIX = 'Greenhouse-'
```

### 5.3 Wi-Fi 验证超时

固件位置：

```text
WifiProvisioningCoordinator.h
```

当前为：

```cpp
static constexpr uint32_t kValidationTimeoutMs = 15000;
```

路由器连接较慢时可以适当增大，但不要在 `poll()` 中加入阻塞等待。

### 5.4 NVS 命名空间

固件位置：

```text
WifiCredentialStore.cpp
```

当前命名空间为：

```cpp
constexpr char kNamespace[] = "iot_wifi";
```

如果新项目已经使用同名 Preferences 命名空间，需要修改该名称，避免键冲突。ESP32 NVS 键名长度有限，不要使用过长名称。

凭据采用两个交替槽位保存：先完整写入非活动槽，最后才切换活动槽。这样即使新凭据写入失败，上一个活动槽仍可继续用于下次启动。

### 5.5 清除已保存凭据

维护功能可以调用：

```cpp
wifiCredentialStore.clear();
```

清除后重启，程序会重新使用 `WifiCredentials.h` 的默认网络。也可以通过 Arduino IDE 的“擦除全部 Flash”清除 NVS，但这会同时清除其他持久化数据。

## 6. UniApp 端移植步骤

### 6.1 复制并打开正确目录

复制整个：

```text
uniapp-ble-provisioning/
```

在 HBuilderX 中必须把这个目录本身作为项目根目录打开，而不是打开包含它的 Arduino 仓库根目录。项目根目录应直接看到：

```text
App.vue
main.js
manifest.json
pages.json
index.html
pages/
services/
utils/
```

如果 HBuilderX 报“根目录缺少 index.html”，优先检查是不是打开了错误的上级目录。

### 6.2 配置小程序/App身份

修改 `manifest.json`：

- `name`：应用显示名称；
- `appid`：UniApp 项目标识；
- `versionName` / `versionCode`：发行版本；
- `mp-weixin.appid`：自己的微信小程序 AppID。

不要把微信 AppID 误填到 ESP32 固件中；它只属于小程序工程。

### 6.3 安装 Wi-Fi 扫描能力

在 HBuilderX/DCloud 插件市场安装官方 `uni-WiFi` 扩展 API。当前代码通过以下接口调用手机扫描：

```js
uni.startWifi()
uni.onGetWifiList()
uni.getWifiList()
uni.stopWifi()
```

微信开发者工具模拟器不能完整替代真机 BLE/Wi-Fi 扫描，最终必须使用真机调试。

### 6.4 权限配置

`manifest.json` 已声明：

- Android Bluetooth；
- Android Bluetooth Admin；
- Android Bluetooth Scan；
- Android Bluetooth Connect；
- Android Access Wi-Fi State；
- Android Change Wi-Fi State；
- Android Fine Location；
- 微信 `scope.userLocation` 用途说明；
- 微信 `getWifiList`、`startWifi` 隐私接口声明。

如果新项目合并到已有 UniApp 应用，不要覆盖整个 `manifest.json`，应把上述权限合并进去。

### 6.5 同步 UUID 和设备名前缀

手机端位置：

```text
uniapp-ble-provisioning/utils/protocol.js
```

以下四项必须与固件一致：

```js
BLE_SERVICE_UUID
BLE_RX_UUID
BLE_TX_UUID
BLE_DEVICE_NAME_PREFIX
```

通常复用当前小程序时不修改这些常量，这样同一个小程序可以连接所有使用相同协议的项目。

### 6.6 修改页面品牌和业务入口

页面位置：

```text
pages/index/index.vue
```

可以安全修改：

- 页面标题；
- 说明文字；
- 颜色和样式；
- 配网成功后跳转的页面；
- 成功后是否继续保持 BLE 连接。

不建议随意修改：

- `sending`、`validating` 等状态流转；
- 密码只保存在组件内存中的做法；
- 成功后清空密码；
- 配网期间禁止重复提交；
- 页面退出时调用 `wifiService.close()` 和 `bleService.close()`；
- 使用请求 ID 过滤其他通知。

不要调用 `uni.setStorage` 保存 Wi-Fi 密码，也不要在 `console.log` 中输出完整请求对象。

### 6.7 合并到已有 UniApp 项目

如果不是复制独立配网 App，而是把功能加入已有应用：

1. 复制 `utils/protocol.js`；
2. 复制 `services/ble-service.js`；
3. 复制 `services/wifi-service.js`；
4. 复制或改造 `pages/index/index.vue`；
5. 把页面路径加入原项目 `pages.json`；
6. 把 Android/微信权限合并进原项目 `manifest.json`；
7. 确认原项目仍为 Vue 3，或者按其 Vue 版本调整页面写法；
8. 页面退出时清理 BLE/Wi-Fi 监听；
9. 运行 Node 测试并用微信/Android 真机验证。

## 7. BLE 协议约定

### 7.1 消息边界

- 编码：UTF-8；
- 格式：JSON；
- 每条消息结尾：换行符 `\n`；
- 每个 BLE 写入/通知分包：最多 20 字节；
- 单条完整消息：最多 256 字节；
- 分包必须按顺序串行写入。

不要按 JavaScript 字符数切包。中文字符在 UTF-8 中通常占多个字节，必须使用 `TextEncoder` 后按字节切分。

### 7.2 配网请求

```json
{"id":"a1b2c3d4","cmd":"configure_wifi","ssid":"Selected-WiFi","password":"user-entered-password"}
```

字段限制：

| 字段 | 规则 |
| --- | --- |
| `id` | 8 位十六进制字符，用于关联本次请求和回复 |
| `cmd` | 固定为 `configure_wifi` |
| `ssid` | UTF-8 编码后 1–32 字节 |
| `password` | UTF-8 编码后 0–63 字节，开放网络可为空 |

### 7.3 回复事件

正在验证：

```json
{"id":"a1b2c3d4","event":"wifi_connecting"}
```

成功：

```json
{"id":"a1b2c3d4","event":"wifi_connected","ip":"192.168.1.100"}
```

失败：

```json
{"id":"a1b2c3d4","event":"wifi_failed","reason":"timeout"}
```

协议错误：

```json
{"id":"a1b2c3d4","event":"error","reason":"invalid_request"}
```

常见 `reason`：

| 原因 | 含义 |
| --- | --- |
| `invalid_json` | JSON 无法解析 |
| `invalid_request` | 缺字段、命令错误或 ID 错误 |
| `invalid_ssid` | SSID 为空或超过 32 字节 |
| `invalid_password` | 密码超过 63 字节 |
| `message_too_long` | 完整 BLE 消息超过 256 字节 |
| `busy` | 已有配网事务正在处理 |
| `timeout` | 15 秒内未连接成功 |
| `connection_failed` | Wi-Fi 返回明确连接失败 |
| `storage_failed` | 已连接，但保存 NVS 失败并执行回滚 |

手机端只处理 `id` 与当前请求一致的回复。不要删除这个关联判断，否则多条历史通知可能改变当前页面状态。

## 8. 哪些地方必须成对修改

| 修改内容 | Arduino 端 | UniApp 端 |
| --- | --- | --- |
| 服务 UUID | `BleProvisioningProtocol.h` | `utils/protocol.js` |
| RX/TX UUID | `BleProvisioningProtocol.h` | `utils/protocol.js` |
| 设备名前缀 | `BleProvisioningService.cpp` | `utils/protocol.js` |
| JSON 命令字段 | `BleProvisioningProtocol.cpp` | `utils/protocol.js` |
| 回复事件名称 | `BleProvisioningService.cpp` / 协调器 | `pages/index/index.vue` / `utils/protocol.js` |
| 最大分包长度 | `kBleProvisioningMaxChunkBytes` | `MAX_CHUNK_BYTES` |
| 最大消息长度 | `kBleProvisioningMaxMessageBytes` | `MAX_MESSAGE_BYTES` |

如果只是更换开发板 GPIO、默认 Wi-Fi、应用标题或微信 AppID，不需要修改 BLE 协议。

## 9. 推荐的复用策略

### 9.1 多个项目共用同一个小程序

推荐保持下面内容完全一致：

- BLE 服务/RX/TX UUID；
- `IoT-Controller-` 设备名前缀；
- JSON 字段和事件名称；
- 20 字节分包和换行结尾；
- 8 位请求 ID。

每块板通过 MAC 尾部四位区分，因此可以共用同一个小程序。

### 9.2 协议升级

如果未来协议发生不兼容修改，建议增加协议版本，而不是直接复用旧事件名表达新含义。例如：

```json
{"id":"a1b2c3d4","cmd":"configure_wifi","version":2,"ssid":"...","password":"..."}
```

真正不兼容时也可以更换 UUID，让旧小程序不会误连接新协议设备。

### 9.3 安全边界

当前 BLE 配网是近距离明文应用层传输，适合测试开发阶段。正式产品建议增加至少一种保护：

- BLE 配对/绑定；
- 设备外壳上的一次性配网码；
- 应用层挑战应答；
- 应用层加密和重放保护；
- 配网窗口超时，而不是永久允许任意附近手机配网。

不要把“设备名称难猜”当作安全措施。

## 10. 新项目移植清单

### 10.1 Arduino

- [ ] 目标是 ESP32/ESP32-S3 Arduino 工程；
- [ ] 安装 ArduinoJson；
- [ ] 复制五组配网核心 `.h/.cpp`；
- [ ] 复制或解除 `DashboardTypes.h` 依赖；
- [ ] 创建默认 `WifiCredentials.h`；
- [ ] 在 `.ino` 中创建 Wi-Fi、NVS、协调器和 BLE 全局对象；
- [ ] `setup()` 中完成凭据优先级选择；
- [ ] `setup()` 中调用 Wi-Fi 和 BLE `begin()`；
- [ ] `loop()` 中持续调用 Wi-Fi、BLE、协调器 `poll()`；
- [ ] 从 BLE 队列取请求并交给协调器；
- [ ] 从协调器取事件并交给 BLE；
- [ ] MQTT 使用真实 Wi-Fi 状态；
- [ ] 没有打印 Wi-Fi 密码；
- [ ] GPIO 与新硬件原理图不冲突；
- [ ] UUID 和设备名前缀与小程序一致。

### 10.2 UniApp

- [ ] HBuilderX 打开的是 `uniapp-ble-provisioning` 本身；
- [ ] 根目录存在 `index.html`；
- [ ] `manifest.json` 配置自己的微信 AppID；
- [ ] 安装官方 `uni-WiFi` 扩展 API；
- [ ] Android 权限已合并；
- [ ] 微信隐私接口与定位用途已声明；
- [ ] UUID 与 Arduino 一致；
- [ ] 设备名前缀与 Arduino 一致；
- [ ] 没有持久化或打印 Wi-Fi 密码；
- [ ] 页面退出时移除监听；
- [ ] 微信和 Android 都使用真机测试。

## 11. 验证步骤

### 11.1 自动测试

UniApp：

```powershell
npm test --prefix uniapp-ble-provisioning
```

固件关键测试：

```powershell
arduino-cli compile --config-file tmp/arduino-cli-codex.yaml `
  --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi" `
  tests/ble_provisioning_protocol_test

arduino-cli compile --config-file tmp/arduino-cli-codex.yaml `
  --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi" `
  tests/wifi_credential_policy_test

arduino-cli compile --config-file tmp/arduino-cli-codex.yaml `
  --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi" `
  tests/wifi_provisioning_coordinator_test
```

`tmp/arduino-cli-codex.yaml` 中是本机 Arduino CLI 路径；换电脑后需要修改，使用 Arduino IDE 时不需要该文件。

### 11.2 实机验证

1. 清除配网 NVS 后启动，确认连接默认 Wi-Fi；
2. 手机能发现 `IoT-Controller-XXXX`；
3. BLE 连接成功后，确认 GPIO44（若使用）常亮；
4. 手机能扫描并显示附近 2.4GHz Wi-Fi；
5. 输入正确凭据，确认页面依次显示发送、验证和成功；
6. 确认页面显示 ESP32 获取的新 IP；
7. 断电重启，确认自动连接刚刚保存的网络；
8. 输入错误密码，确认旧凭据未覆盖且旧网络恢复；
9. 确认 Wi-Fi 恢复后 MQTT 自动重连；
10. 断开 BLE 后仍能再次搜索并重新连接设备；
11. 确认传感器、屏幕和执行器在配网期间没有因阻塞而停止工作。

## 12. 常见问题

### 12.1 手机能看到设备，但连接后提示没有配网服务

检查固件和 `utils/protocol.js` 的 Service UUID 是否一致，并确认连接的是当前项目的 `IoT-Controller-XXXX`。

### 12.2 能连接 BLE，但发送后没有回复

检查：

- RX/TX UUID 是否对应正确；
- TX Notify 是否成功启用；
- 每条 JSON 是否以 `\n` 结束；
- 是否按最多 20 字节串行写入；
- Arduino `loop()` 是否持续调用 `bleProvisioningService.poll()`；
- 主循环是否存在长时间 `delay()`。

### 12.3 新 Wi-Fi 成功后，重启又回到默认网络

检查：

- `WifiCredentialStore::save()` 是否返回成功；
- NVS 分区是否存在；
- 是否每次烧录都选择了擦除全部 Flash；
- 是否在其他模块中清空了 `iot_wifi` 命名空间；
- 启动时是否真的调用 `wifiCredentialStore.load()` 和 `selectBootCredentials()`。

### 12.4 错误密码后无法恢复旧网络

检查协调器创建时是否同时传入了 `WiFiDataSource` 和 `WifiCredentialStore`，并确认没有在配网流程之外直接覆盖 `currentCredentials()` 对应状态。

### 12.5 HBuilderX 提示根目录缺少 `index.html`

检查打开的项目目录是否为：

```text
...\iot-controller\uniapp-ble-provisioning
```

而不是上一级 Arduino 仓库目录，并确认该目录下存在 `index.html`。

### 12.6 手机扫描不到 Wi-Fi

检查手机 Wi-Fi、蓝牙和定位服务是否开启；微信/Android 权限是否授权；Android 是否安装官方 `uni-WiFi` 扩展；并使用真机测试。

### 12.7 找不到 5GHz Wi-Fi

这是当前设计的预期行为。ESP32-S3 只连接 2.4GHz Wi-Fi，手机端会过滤已知频率为 5GHz 的网络。双频同名路由器建议临时区分 2.4GHz 和 5GHz SSID，便于测试。

### 12.8 更换 GPIO 后状态灯异常

检查新 GPIO 是否为启动绑带、Flash/PSRAM、USB/JTAG、UART 或其他外设占用引脚；同时确认 LED 是高电平还是低电平点亮。当前 `ConnectionStatusIndicators` 按高电平点亮设计。

## 13. 最小复用原则

如果希望以后继续使用当前小程序，最重要的是保持以下契约不变：

```text
设备名前缀一致
Service/RX/TX UUID 一致
JSON 字段和事件名称一致
UTF-8 + 换行消息边界一致
20 字节 BLE 分包一致
8 位请求 ID 关联一致
```

Arduino 工程的传感器、屏幕、MQTT 服务、继电器和电机都可以更换；只要上述 BLE 协议契约保持一致，当前小程序仍可复用。
