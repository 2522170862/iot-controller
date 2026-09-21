# ESP32-S3 蓝牙 Wi-Fi 配网端

本目录是与 `iot-controller.ino` 配套的 UniApp Vue 3 工程。它支持微信小程序和 Android App，通过手机扫描附近的 2.4GHz Wi-Fi，再经 BLE 把用户选择的 SSID 和密码发送给 ESP32-S3。当前版本不支持 iOS。

## 运行环境

- HBuilderX（选择支持 Vue 3 的较新版本）；
- DCloud 官方 `uni-WiFi` 扩展 API；
- 微信开发者工具和可用的微信小程序 AppID；
- Android 设备（蓝牙、Wi-Fi、定位功能均已开启）。

在 HBuilderX 中导入本目录后，从插件市场安装官方 `uni-WiFi` 扩展 API。不要安装名字相近的第三方 Wi-Fi 插件。微信小程序发布前，将 `manifest.json` 中 `mp-weixin.appid` 的空值改为自己的 AppID；Android App 仅作为 Android 目标构建。

## 构建

### 微信小程序

1. 在 HBuilderX 打开本目录；
2. 配置微信小程序 AppID；
3. 确认微信公众平台已声明“位置信息”用途；
4. 选择“发行 → 小程序-微信”；
5. 在微信开发者工具中检查 `getWifiList`、`startWifi`、蓝牙和定位授权；
6. 使用真机测试，开发者工具模拟器不能替代真实 BLE/Wi-Fi 扫描。

### Android App

1. 安装并勾选官方 `uni-WiFi` 扩展 API；
2. 选择“发行 → 原生 App-云打包”或本地打包；
3. 安装后允许“附近设备”和“位置信息”权限；
4. 若拒绝了“不再询问”，需要在系统应用设置中重新开启权限。

`manifest.json` 已声明 Bluetooth、Bluetooth Admin、Bluetooth Scan、Bluetooth Connect、Access/Change Wi-Fi State 和 Fine Location 权限。

## 配网流程

1. 板子上电后先读取 NVS 中已验证的凭据；没有有效记录时使用 `WifiCredentials.h` 中的默认 Wi-Fi；
2. BLE 始终广播 `IoT-Controller-XXXX`，`XXXX` 是芯片 MAC 末尾四位；
3. 手机搜索并连接控制器，GPIO44 在 BLE 已连接时常亮；
4. 手机调用系统能力扫描附近 Wi-Fi，页面过滤已知 5GHz 网络；
5. 用户选择或手动输入 SSID，填写密码后发送；
6. GPIO3 在新 Wi-Fi 验证期间每 250ms 改变一次状态；
7. 成功后凭据写入 NVS，页面显示新 IP，GPIO3 常亮，MQTT 自动重连；
8. 失败时不覆盖 NVS，并恢复切换前使用的 Wi-Fi；
9. BLE 断开后 GPIO44 熄灭，控制器重新开始广播。

页面不会把密码写入本地存储，也不会打印完整配网请求；成功后会立即清空内存中的密码输入值。

## BLE 协议

| 用途 | UUID |
| --- | --- |
| 配网服务 | `6E400001-B5A3-F393-E0A9-E50E24DCCA9E` |
| RX（手机写入） | `6E400002-B5A3-F393-E0A9-E50E24DCCA9E` |
| TX（控制器通知） | `6E400003-B5A3-F393-E0A9-E50E24DCCA9E` |

消息使用 UTF-8 JSON，每条以换行符 `\n` 结束，每个 BLE 分包不超过 20 字节。单条消息最大 256 字节。请求 ID 为 8 位小写十六进制字符，用来关联同一次配网事务。

请求：

```json
{"id":"a1b2c3d4","cmd":"configure_wifi","ssid":"Selected-WiFi","password":"user-entered-password"}
```

开始验证：

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

协议错误使用 `event: "error"`。常见 `reason` 包括 `invalid_json`、`invalid_request`、`invalid_ssid`、`invalid_password`、`message_too_long`、`busy`、`timeout`、`connection_failed` 和 `storage_failed`。

## 固件烧录与串口检查

Arduino IDE 继续直接打开根目录的 `iot-controller.ino`。ESP32-S3 设置使用 16MB Flash 和 OPI PSRAM。烧录后串口可能显示以下信息：

```text
BLE provisioning advertising: IoT-Controller-XXXX
Connecting to Wi-Fi: <SSID>
Wi-Fi connected, IP: <IPv4>
MQTT connected: <server>:<port>
```

固件只打印正在连接的 SSID，不打印 Wi-Fi 密码。程序存储空间当前约占 92%，后续添加大型库前应继续关注 Flash 余量。

## 凭据重置和回退

配网凭据保存在 ESP32 Preferences 的 `iot_wifi` 命名空间。普通重新烧录通常不会清空 NVS；需要恢复默认网络时，可在 Arduino IDE 擦除全部 Flash 后重新烧录，或在维护代码中调用 `WifiCredentialStore::clear()`。没有有效 NVS 记录时，启动会回退到 `WifiCredentials.h`。

错误密码、连接超时或保存失败不会替换上一次有效凭据。失败后固件会重新连接切换前的网络。Wi-Fi 配网成功不要求 MQTT 同时成功；MQTT 按原有重连策略继续尝试。

## 自动化检查

在项目根目录执行：

```powershell
node --test uniapp-ble-provisioning/tests/protocol.test.mjs
```

测试覆盖 8 位请求 ID、20 字节分包、中文 UTF-8 跨包、多个通知帧、请求关联，以及 Wi-Fi 列表去重/排序/2.4GHz 过滤。

## 九项实机验收

1. 擦除配网 NVS 后启动，板子仍连接 `WifiCredentials.h` 中的默认 Wi-Fi；
2. 微信小程序能扫描手机附近 Wi-Fi，并成功切换一次网络；
3. Android App 能完成相同流程；
4. 验证期间 GPIO3 闪烁，连接成功后常亮；
5. BLE 连接期间 GPIO44 常亮，断开后熄灭；
6. 输入错误密码时不覆盖旧凭据，并自动恢复旧网络；
7. 正确配网后断电重启，板子自动连接上一次成功网络；
8. Wi-Fi 成功后 MQTT 能重新连接，传感器显示和模块控制继续工作；
9. 板子联网后 BLE 仍可发现，并能再次更换 Wi-Fi。

自动化测试不能代替以上真机步骤。尤其要分别使用微信真机和 Android 真机验证权限、系统 Wi-Fi 扫描与 BLE 生命周期。
