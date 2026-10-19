# 蓝牙 Wi-Fi / MQTT 配置说明

## 操作方式

在 HBuilderX 打开 `uniapp-ble-provisioning`，编译为微信小程序或 Android App。首页有三个入口，配置内容在弹窗内填写：

1. **蓝牙连接**：搜索附近有名称的 BLE 设备，选择开发板。连接后首页和蓝牙弹窗都有断开按钮。
2. **Wi-Fi 配置**：使用手机扫描附近 2.4GHz 网络，也可手动输入 SSID。输入密码后点击“保存并连接”。
3. **MQTT 配置**：填写服务器、端口、用户名、密码；需要改变主题时打开“高级设置”。点击“保存并连接”，等待板子验证结果。

手机不保存密码，配置成功或蓝牙断开后清空对应密码输入。主动断开蓝牙不代表撤销已经发送给开发板的配置；开发板可能继续验证并保存。

## BLE 传输

沿用现有服务与特征值，不需要新增小程序权限：

| 用途 | UUID |
| --- | --- |
| 服务 | `6E400001-B5A3-F393-E0A9-E50E24DCCA9E` |
| RX，手机写入 | `6E400002-B5A3-F393-E0A9-E50E24DCCA9E` |
| TX，开发板通知 | `6E400003-B5A3-F393-E0A9-E50E24DCCA9E` |

消息是紧凑 UTF-8 JSON，以换行符 `\n` 结尾，每次写入最多 20 字节；手机发送端把末尾换行计算在 1024 字节总长度以内。回复也采用相同分包方式。请求 `id` 为 8 位十六进制字符，手机生成小写，用于关联回复，和 MQTT 控制消息的 `hash` 字段相互独立。

手机每次只发送并等待一个配置请求；开发板也对 Wi-Fi 和 MQTT 配置互斥，重复请求返回 `busy`。手机最多等待 45 秒；等待超时代表结果未确认，不能认为板子必然没有应用配置。

## 当前 Wi-Fi 请求

```json
{"id":"a1b2c3d4","cmd":"configure_wifi","ssid":"Lab-WiFi","password":"example-password"}
```

原有格式保持兼容。SSID 为 1～32 字节，密码为 0～63 字节；开放网络密码传空字符串。连接验证成功后保存，失败恢复之前的 Wi-Fi。Wi-Fi 验证不要求 MQTT 服务器同时可用。

## MQTT 请求

仅更换服务器连接参数：

```json
{"id":"a1b2c3d5","cmd":"configure_mqtt","server":"mqtt.example.com","port":1883,"username":"example-user","password":"example-password"}
```

连同主题一起修改：

```json
{"id":"a1b2c3d6","cmd":"configure_mqtt","server":"mqtt.example.com","port":1883,"username":"example-user","password":"example-password","subTopic":"my/device/command","pubTopic":"my/device/data","subQos":0}
```

| 字段 | 必填 | 限制与含义 |
| --- | --- | --- |
| `id` | 是 | 8 位十六进制请求编号 |
| `cmd` | 是 | 固定 `configure_mqtt` |
| `server` | 是 | 1～127 字节，IPv4 地址或域名，允许英文字母、数字、点、短横线；不带协议前缀、路径或端口 |
| `port` | 是 | JSON 整数，1～65535，不加引号 |
| `username` | 是 | 0～63 字节；无认证时传空字符串 |
| `password` | 是 | 0～127 字节；无认证时传空字符串 |
| `subTopic` | 否 | 1～39 字节；订阅主题，支持完整层级的 `+` 和位于最后一层的 `#` |
| `pubTopic` | 否 | 1～39 字节；发布主题，不能包含 `+` 或 `#` |
| `subQos` | 否 | JSON 整数 0 或 1；表示订阅 QoS，发布仍为 QoS 0 |

长度均按 UTF-8 字节数计算。三个可选字段独立判断：省略保留当前值，`null` 和空主题不等于省略，会被拒绝。`subQos: 0` 是明确修改，不能当作未传。弹窗中主题留空、QoS 选择“保持当前值”时不发送对应字段。

当前沿用普通 MQTT TCP 连接，不新增 TLS；填写 8883 并不会自动启用 TLS。Client ID 继续使用 `MqttCredentials.h` 中的 `kClientId`。

## 开发板回复

开始连接：

```json
{"id":"a1b2c3d5","event":"mqtt_connecting"}
```

连接验证并保存成功：

```json
{"id":"a1b2c3d5","event":"mqtt_connected"}
```

失败并恢复旧配置：

```json
{"id":"a1b2c3d5","event":"mqtt_failed","reason":"authentication_failed"}
```

| `reason` | 含义 |
| --- | --- |
| `wifi_offline` | 板子没有连接 Wi-Fi，需要先配网 |
| `busy` | Wi-Fi 或 MQTT 配置事务正在进行 |
| `authentication_failed` | 服务器拒绝用户名、密码或授权 |
| `connection_failed` | TCP、DNS 或 MQTT 连接失败 |
| `subscribe_failed` | MQTT 订阅请求发送失败 |
| `timeout` | 15 秒配置验证窗口内没有连接成功 |
| `storage_failed` | 已连接，但保存 NVS 失败，恢复旧配置 |
| `invalid_mqtt` | 字段类型、长度、端口、主题或 QoS 不合法 |

协议解析错误使用 `event: "error"`。可解析的合法请求编号会随错误返回；无法恢复编号时使用 `00000000`。手机把配置发送异常、回复解析错误和 45 秒未响应也显示为可重试的错误。

当前 PubSubClient 的 `subscribe()` 返回值仅确认订阅请求发送成功，不能证明服务端 ACL 允许订阅或发布。实机验收还应在服务器发送一条控制命令，确认板子能收到，并检查上报主题。

## 保存、回退与运行时使用

- Wi-Fi 和 MQTT 分别保存在 `iot_wifi`、`iot_mqtt` NVS 命名空间。已有 Wi-Fi 保存内容不受影响。
- 第一次启动没有有效 MQTT 记录时使用 `MqttCredentials.h`；以后优先使用上次通过验证并保存的配置。
- MQTT 使用两个配置槽，写完并验证新槽后才提交活动槽标记。保存失败不会替换当前有效配置；读取发现活动记录损坏时尝试另一个有效槽。
- 切换服务器或主题时断开旧 MQTT 会话，清空收发队列。之后所有环境上报、输入/RFID 数据和指令回复使用新的发布主题，LCD 显示当前服务器。
- 配置失败会恢复切换前的服务器、认证和主题，原有重连逻辑继续工作。恢复连接仍取决于旧服务器和网络是否可用。
- 重新烧录通常保留 NVS，因此改动编译期默认值不会覆盖已经保存的 MQTT 设置。

## 自动化验证与实机验收

小程序逻辑测试：在 `uniapp-ble-provisioning` 中运行 `node --test`。

固件主机测试使用 C++17 编译器和 ArduinoJson，网络及 NVS 为底层测试替身，协议、协调器、持久化与服务代码使用真实实现：

```powershell
.\tools\verify-mqtt-configuration.ps1 -Compiler g++
```

Arduino IDE 仍打开根目录 `iot-controller.ino`。使用你的 ESP32-S3 N16R8 板型设置编译、烧录；小程序或 App 需要在 HBuilderX 重新构建，旧安装包不会自动更新。

真机依次检查：蓝牙连接/主动断开/再次连接；手机 Wi-Fi 扫描与错误密码回退；仅改服务器时三个可选字段不变；单独更改一个主题或 QoS；错误 MQTT 认证回退；配置成功后断电重启；新服务器控制继电器等模块及新主题收到环境数据；蓝牙在验证中断开后重新连接恢复操作。微信和 Android 需要分别验证授权、弹窗滚动及软键盘遮挡情况。
