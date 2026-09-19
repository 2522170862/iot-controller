# ESP32-S3 BLE Wi-Fi 配网设计

日期：2026-09-19

## 1. 目标与范围

为现有 ESP32-S3 Arduino IDE `.ino` 工程增加 BLE Wi-Fi 配网能力，并在同一仓库中提供一个 UniApp Vue 3 配网端。

本阶段支持：

- 微信小程序；
- Android App；
- 手机扫描附近 Wi-Fi，用户选择 SSID 后通过 BLE 将 SSID 和密码发送给 ESP32-S3；
- 板子首次烧录时继续连接 `WifiCredentials.h` 中的现有默认 Wi-Fi；
- BLE 配网成功后保存新凭据，此后启动时优先连接最后一次配网成功的 Wi-Fi；
- BLE 始终保持广播，允许设备联网后再次配网；
- GPIO3 显示 Wi-Fi 连接状态，GPIO44 显示 BLE 客户端连接状态；
- Wi-Fi 切换后 MQTT 自动恢复连接。

本阶段不支持：

- iOS App；
- Wi-Fi 密码的应用层加密；
- ESP32 扫描附近 Wi-Fi；
- 云端账户、设备绑定和远程配网。

Wi-Fi 密码通过 BLE GATT 明文传输。UniApp 不持久化密码，ESP32 只在验证新网络连接成功后保存凭据。串口日志不得输出密码。

## 2. 平台与硬件约束

- 主控：ESP32-S3 DevKitC-v1 N16R8；
- 固件：Arduino IDE `.ino` 工程结构；
- ESP32-S3 仅连接 2.4 GHz Wi-Fi；
- Wi-Fi 指示灯：GPIO3；
- BLE 连接指示灯：GPIO44；
- GPIO44 被配置为普通输出后不再承担 UART0 RX 功能；现有串口 TX 调试输出可以继续使用；
- MQTT 指示灯及现有外设逻辑保持原有行为，不在本功能中重构；
- UniApp 工程使用 Vue 3，并可由 HBuilderX 发布为微信小程序和 Android App。

## 3. 总体架构

### 3.1 固件组件

新增以下独立组件：

1. `WifiCredentialStore`
   - 使用 ESP32 `Preferences`/NVS 保存 Wi-Fi SSID、密码和有效标记；
   - 读取已保存凭据；
   - 只有新网络验证成功后才能覆盖旧凭据；
   - 不负责连接 Wi-Fi。

2. `BleProvisioningService`
   - 初始化 BLE GATT Server；
   - 始终广播设备；
   - 接收、分片重组和解析配网 JSON；
   - 通过通知特征值分片发送状态回复；
   - 提供 BLE 客户端连接状态；
   - 不直接保存凭据。

3. `WifiProvisioningCoordinator`
   - 管理一次配网事务；
   - 保存事务开始前的可用凭据；
   - 暂停正常 Wi-Fi/MQTT 重连，验证新凭据；
   - 成功后写入 NVS，并恢复 MQTT；
   - 失败后恢复原凭据和原网络；
   - 拒绝并行配网事务。

4. 扩展 `WiFiDataSource`
   - 启动时优先读取 NVS 中最后一次成功凭据；
   - NVS 无有效记录时使用 `WifiCredentials.h` 的默认凭据；
   - 支持非阻塞连接、验证连接和恢复旧连接；
   - 暴露连接阶段和失败原因，不在其中实现 BLE 协议。

5. 扩展 `ConnectionStatusIndicators`
   - Wi-Fi 未连接：GPIO3 低电平；
   - 正在验证新 Wi-Fi：GPIO3 周期闪烁；
   - Wi-Fi 已连接：GPIO3 高电平；
   - BLE 无客户端：GPIO44 低电平；
   - BLE 客户端已连接：GPIO44 高电平。

### 3.2 UniApp 组件

在仓库中新增独立目录 `uniapp-ble-provisioning/`：

```text
uniapp-ble-provisioning/
├─ pages/index/index.vue
├─ services/ble-service.js
├─ services/wifi-service.js
├─ utils/protocol.js
├─ App.vue
├─ main.js
├─ pages.json
└─ manifest.json
```

- `index.vue`：单页配网流程和状态展示；
- `ble-service.js`：BLE 初始化、扫描、连接、服务发现、通知订阅、分包写入和断开清理；
- `wifi-service.js`：手机 Wi-Fi 扫描及微信/Android 的平台差异；
- `protocol.js`：UTF-8 编解码、请求 ID、JSON 行消息和分片重组；
- `manifest.json`：声明 Android 蓝牙、Wi-Fi 和定位权限。

不引入额外全局状态管理框架。

## 4. 启动与凭据优先级

板子启动后按以下顺序选择 Wi-Fi：

1. 如果 NVS 存在有效的已验证凭据，连接该网络；
2. 如果 NVS 无有效凭据，连接 `WifiCredentials.h` 中的默认网络；
3. 连接失败时保持正常的非阻塞重试，同时 BLE 继续广播；
4. BLE 配网成功后，NVS 凭据成为下次启动的首选网络。

烧录新固件不应因 NVS 中没有记录而失去当前默认联网能力。

## 5. BLE GATT 设计

设备广播名格式：

```text
IoT-Controller-XXXX
```

`XXXX` 取设备 MAC 地址的末尾四位，用于区分多块板子。

GATT Server 使用一个固定服务和两个固定特征值：

- RX：支持 Write 和 Write Without Response，接收手机命令；
- TX：支持 Notify，发送处理状态和结果。

具体 UUID 在实现计划中固定为项目常量，并由固件与 UniApp 共用同一份协议说明。

### 5.1 消息边界与分片

- 应用层消息是 UTF-8 JSON；
- 每条 JSON 后追加换行符 `\n`，作为消息结束标记；
- UniApp 将输出按最多 20 字节顺序分包，避免依赖协商后的 MTU；
- ESP32 按收到顺序缓存字节，遇到 `\n` 后解析一条完整消息；
- ESP32 通知回复也使用相同的分片和换行规则；
- 单条消息最大 256 字节；超过限制立即清空缓存并返回 `message_too_long`；
- 分包必须串行发送；某一包失败后停止本次发送。

### 5.2 配网命令

请求示例：

```json
{"id":"a1b2c3d4","cmd":"configure_wifi","ssid":"Selected-WiFi","password":"user-entered-password"}
```

字段要求：

- `id`：8 位十六进制请求标识；
- `cmd`：固定为 `configure_wifi`；
- `ssid`：1 至 32 字节；
- `password`：0 至 63 字节，开放网络允许空字符串。

### 5.3 回复事件

接受请求并开始验证：

```json
{"id":"a1b2c3d4","event":"wifi_connecting"}
```

连接成功：

```json
{"id":"a1b2c3d4","event":"wifi_connected","ip":"192.168.1.100"}
```

连接失败：

```json
{"id":"a1b2c3d4","event":"wifi_failed","reason":"timeout"}
```

协议错误：

```json
{"id":"a1b2c3d4","event":"error","reason":"invalid_request"}
```

支持的 `reason` 至少包括：

- `invalid_json`；
- `invalid_request`；
- `invalid_ssid`；
- `invalid_password`；
- `message_too_long`；
- `busy`；
- `timeout`；
- `connection_failed`。

所有回复都携带原请求 `id`。UniApp 只处理当前配网事务对应的回复。

## 6. UniApp 用户流程

1. 页面初始化手机 BLE 和 Wi-Fi 模块；
2. 申请微信小程序或 Android 所需权限；
3. 扫描名称前缀为 `IoT-Controller-` 的 BLE 设备；
4. 用户选择目标设备；
5. 建立 BLE 连接、发现服务和特征值，并启用 TX 通知；
6. GPIO44 常亮；
7. 手机扫描附近 Wi-Fi；
8. 页面展示 SSID、信号强度和是否加密，优先显示 2.4 GHz 网络；
9. 用户选择 SSID，输入密码；隐藏网络可手动输入 SSID；
10. UniApp 生成请求 ID，将 JSON 分片写入 RX；
11. 页面依次展示“正在发送”“正在验证”“配网成功”或具体错误；
12. 成功时显示 ESP32 的新 IP；
13. BLE 断开时 GPIO44 熄灭，板子继续广播。

## 7. Wi-Fi 验证事务

收到合法配网命令后：

1. 如果已有验证事务，返回 `busy`；
2. 保存当前可恢复的凭据；
3. 回复 `wifi_connecting`；
4. GPIO3 开始闪烁；
5. 暂停 MQTT 连接尝试；
6. 断开当前 Wi-Fi，使用新凭据发起连接；
7. 在限定时间内轮询连接状态，主循环不能被阻塞；
8. 成功时写入 NVS、GPIO3 常亮、回复 IP，并允许 MQTT 重连；
9. 失败时不写 NVS，回复失败原因，恢复原凭据和原网络；
10. 恢复原网络期间 GPIO3 按普通 Wi-Fi 状态显示。

Wi-Fi 连接成功是配网成功的判定条件。MQTT 暂时连接失败不会回滚已验证的 Wi-Fi，而是在页面提示 Wi-Fi 已成功、MQTT 正在重连。

## 8. 并发与生命周期

- BLE 广播在正常联网后仍保持开启；
- 同一时间只允许一个 BLE 客户端执行配网；
- 同一时间只允许一个 Wi-Fi 验证事务；
- BLE 断开不取消已经开始的 Wi-Fi 验证；板子仍完成验证并保存成功凭据；
- BLE 断开后无法投递最终通知，但结果仍体现在 Wi-Fi/NVS 状态中；
- MQTT 发布、订阅和外设轮询继续使用现有主循环，配网流程不得使用长时间阻塞等待；
- BLE 回调只做收包和入队，Wi-Fi 切换在主循环状态机中执行，避免回调与网络逻辑冲突。

## 9. 异常与安全处理

- BLE、Wi-Fi 或定位未开启时，UniApp 显示明确提示和重试入口；
- 权限被拒绝时不循环弹窗，提供前往系统设置的说明；
- BLE 断开后停止页面配网计时并允许重新连接；
- BLE 分包写入失败后终止剩余分包；
- 输入在 UniApp 和 ESP32 两端同时校验；
- 固件日志不得打印 Wi-Fi 密码；
- UniApp 不把密码写入本地存储、日志或页面历史；
- 无效新凭据不得覆盖旧凭据；
- NVS 写入失败时返回错误并继续保留旧凭据；
- 本阶段接受 BLE 明文配网的风险，正式产品版本需要另行设计配对码或应用层加密。

## 10. 测试与验收

### 10.1 固件单元/主机测试

- 无 NVS 凭据时选择默认凭据；
- 有有效 NVS 凭据时优先选择已保存凭据；
- 新网络成功后保存，失败时不覆盖；
- 失败后恢复原网络；
- Wi-Fi 验证期间指示灯闪烁，成功后常亮；
- BLE 连接/断开正确控制 GPIO44；
- JSON 分片可跨任意边界重组；
- 超长消息、非法 JSON、缺失字段和并发请求返回预期错误；
- MQTT 在 Wi-Fi 切换期间暂停，在网络恢复后继续现有重连流程。

### 10.2 UniApp 逻辑测试

- UTF-8 字符串正确编码、分片和重组；
- 请求 ID 生成和回复关联正确；
- 微信小程序与 Android 条件编译分支均可构建；
- 权限拒绝、蓝牙关闭、Wi-Fi 关闭和 BLE 断开均显示正确状态；
- 密码不会写入持久化存储或普通日志。

### 10.3 硬件验收

1. 清空配网 NVS 后启动，板子连接当前默认 Wi-Fi；
2. 微信小程序能扫描手机附近 Wi-Fi，并完成一次新网络配网；
3. Android App 能完成相同流程；
4. 验证期间 GPIO3 闪烁，成功后常亮；
5. BLE 连接期间 GPIO44 常亮，断开后熄灭；
6. 输入错误密码时不覆盖旧凭据，并恢复旧网络；
7. 正确配网后断电重启，板子自动连接最后一次成功网络；
8. Wi-Fi 成功后 MQTT 能重新连接，现有传感器和控制功能继续工作；
9. BLE 保持可发现，已联网状态仍可再次更换 Wi-Fi。

## 11. 完成标准

- 固件保持 Arduino IDE `.ino` 工程结构并成功编译；
- 当前默认 Wi-Fi 首次启动行为不变；
- NVS、BLE、指示灯和 MQTT 重连状态符合本文档；
- UniApp 微信小程序与 Android App 均可构建；
- 两个平台均使用手机扫描附近 Wi-Fi；
- 关键协议、构建方法、权限配置和测试步骤有项目文档；
- 不实现未在本设计范围内的 iOS 支持。
