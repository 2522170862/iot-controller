# ESP32-S3 蓝牙设备配置端

本目录是与 `iot-controller.ino` 配套的 UniApp Vue 3 工程，支持微信小程序和 Android App。首页提供蓝牙连接、Wi-Fi 配置和 MQTT 配置三个卡片，点击后在弹窗内操作；已连接蓝牙时可以主动断开。Wi-Fi 仍由手机扫描，MQTT 参数经 BLE 发送给 ESP32-S3，验证成功后保存到板子的 NVS。当前版本不支持 iOS。

MQTT 请求格式、字段长度、可选主题、回退机制和验收步骤见 [`../docs/BLE_MQTT配置说明.md`](../docs/BLE_MQTT配置说明.md)。

## 运行环境

- HBuilderX（选择支持 Vue 3 的较新版本）；
- 微信开发者工具和可用的微信小程序 AppID；
- Android 设备（蓝牙、Wi-Fi、定位功能均已开启）。

在 HBuilderX 中导入本目录。Android App 使用工程内的官方 `uni-wifi 1.0.9` 插件（`uni_modules/uni-wifi`），与用户已验证能扫描的 `wife1.0/ble` 工程相同；不再自行通过 Native.js 读取系统版本。微信小程序继续使用微信的 Wi-Fi API。微信小程序发布前，将 `manifest.json` 中 `mp-weixin.appid` 的空值改为自己的 AppID；Android App 仅作为 Android 目标构建。

## 构建

### 微信小程序

1. 在 HBuilderX 打开本目录；
2. 配置微信小程序 AppID；
3. 确认微信公众平台已声明“位置信息”用途；
4. 选择“发行 → 小程序-微信”；
5. 在微信开发者工具中检查 `getWifiList`、`startWifi`、蓝牙和定位授权；
6. 使用真机测试，开发者工具模拟器不能替代真实 BLE/Wi-Fi 扫描。

### Android App

1. 确认工程保留完整的 `uni_modules/uni-wifi`（已从用户提供的可用工程导入，不用再次下载）；基座调试时选择“运行 → 运行到手机或模拟器”。如果提示缺少 `uni-wifi` 原生模块，按 HBuilderX 提示编译 UTS 插件，或制作包含该插件的自定义基座后运行；
2. 正式发布选择“发行 → 原生 App-云打包”或本地打包，权限声明随安装包生效；
3. Android 8 手机允许基座/应用的“位置信息”，并打开手机 Wi-Fi、定位和蓝牙。Wi-Fi 初始化由官方插件申请定位权限，不混入蓝牙权限申请；Android 12+ 蓝牙功能仍需系统“附近设备”授权；
4. 若拒绝了“不再询问”，需要在系统应用设置中重新开启权限。基座调试时授权对象是 HBuilder 调试基座，而不是独立安装的 App 或微信；
5. 权限变更不能靠热更新修改已经安装的 APK。自定义基座缺少权限时要重新制作并安装基座；正式 App 修改权限后要重新打包安装。

`manifest.json` 已声明 Bluetooth、Bluetooth Admin、Bluetooth Scan、Bluetooth Connect、Access/Change Wi-Fi State、Fine/Coarse Location 权限。旧系统不会申请 Android 12 的蓝牙运行时权限。

扫描入口 `services/wifi-service.js` 使用 `uni.startWifi()` → 注册 `uni.onGetWifiList()` → `uni.getWifiList()`；授权/初始化回调和扫描列表分别有 10 秒超时保护。Android 插件的列表回调是单次的，因此每次扫描都会重新注册；结束、超时或退出会清理监听。结果过滤已知 5GHz 网络、同名 SSID 保留最强信号；错误保留方法名和错误码。扫描失败仍可手动填写 SSID 和密码，通过原有 BLE 协议配网。没有复制参考工程里不同的配网 JSON 格式，不会修改现有板端协议，也无需重新烧录 ESP32 固件。

调试时在 HBuilderX 控制台查看 `[Wi-Fi]` 日志：`startWifi: begin/success`、`getWifiList: begin/success`、`onGetWifiList: N networks`。日志不打印 Wi-Fi 密码或网络列表。若卡在初始化，超时信息会指明 `startWifi`，需检查基座是否包含插件；若初始化成功但无列表则显示扫描超时。

Vue 会代理页面 `data()` 内的服务对象；扫描事务使用数字 ID 判断是否仍有效，不能将 `this.pendingScan` 与原始请求对象做 `===` 比较，否则结果和超时都可能被忽略。回归测试覆盖真实 Vue `reactive()`：有 Node/Vue 依赖时直接 `npm test`；HBuilderX 工程可把环境变量 `UNIAPP_VUE_RUNTIME` 指向 HBuilderX 的 `plugins/uniapp-cli-vite/node_modules/vue` 后运行 `npm test`。未找到 Vue 运行库时，这两项集成测试会明确标记为跳过。

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

### 手机扫描的弹窗流程

Wi-Fi 配置窗口保留手动名称/密码输入。点击“手机扫描”后进入独立的网络选择窗口，可重新扫描或返回手动填写；点击网络后进入密码窗口，自动显示只读的所选 Wi-Fi 名称。该密码输入与手动表单独立，切换网络或返回列表时清空，避免沿用其他网络的密码。点击“保存并连接”仍使用原有 BLE 配网消息。取消扫描或蓝牙断开后，迟到的扫描结果不会重新打开窗口或覆盖新状态。

扫描选网流程在提交后等待开发板的 `wifi_connected` / `wifi_failed` 回包，而不是把 BLE 写入成功当作联网成功。收到成功结果后显示网络名、IP 和“确认”，3 秒后自动返回首页；收到失败结果后显示原因，3 秒后回到同一个密码窗口，保留所选网络与本次输入以便修改。点击“确认”立即执行相同返回动作并取消计时；页面退出、切换或 BLE 断开时清理计时，防止旧倒计时影响新操作。没有收到设备回复时只提示结果未确认，不显示成功。手动填写与 MQTT 配置仍保持原有结果显示方式。

## BLE 协议

| 用途 | UUID |
| --- | --- |
| 配网服务 | `6E400001-B5A3-F393-E0A9-E50E24DCCA9E` |
| RX（手机写入） | `6E400002-B5A3-F393-E0A9-E50E24DCCA9E` |
| TX（控制器通知） | `6E400003-B5A3-F393-E0A9-E50E24DCCA9E` |

消息使用 UTF-8 JSON，每条以换行符 `\n` 结束，每个 BLE 分包不超过 20 字节。单条 JSON 最大 1024 字节（手机发送端含末尾换行符计数）。请求 ID 为 8 位小写十六进制字符，用来关联同一次配置事务。

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

失败后修改密码重试时，固件会清空本地连接/IP 缓存，独立验证本次连接，忽略 ESP32 底层可能暂时保留的上次失败状态。只有实际连接的 SSID 与本次配置相符且取得非零 IP 才确认成功；15 秒验证窗口内不会被普通的 10 秒重连定时器打断。该修复涉及开发板固件，必须重新烧录；只更新手机端不能修复旧固件的误报。

## 自动化检查

在项目根目录执行：

```powershell
npm test --prefix uniapp-ble-provisioning
```

测试覆盖 Wi-Fi/MQTT 编码、可选主题独立省略、字段长度和端口校验、20 字节分包、UTF-8 跨包、请求互斥与超时、断开/重连、提前到达的回复、弹窗状态，以及手机 Wi-Fi 列表过滤。也可以在本目录直接运行 `node --test`。

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
