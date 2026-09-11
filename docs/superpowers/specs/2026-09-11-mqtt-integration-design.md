# MQTT 通信与模块控制设计

## 目标与范围

ESP32-S3 控制器通过现有 Wi-Fi 接入 MQTT 服务端，实现数据上报、远程模块控制和
命令执行回执。直流电机模块不在本次范围内。

本次支持的模块为：环境传感器、摇杆、旋转编码器、RC522 RFID、两路继电器、
8×8 RGB LED 矩阵、舵机和步进电机。

## MQTT 配置与主题

配置采用用户提供的服务器地址、端口、账号、密码、QoS 和主题前缀，写入
`MqttCredentials.h`。按照用户明确要求，该文件受 Git 版本控制并会提交到远程仓库。

只有一台设备，因此不增加设备 ID 或主题后缀：

| 方向 | Topic | QoS |
| --- | --- | --- |
| 服务端到设备 | `LoTS2C/` | 0 |
| 设备到服务端 | `LoTC2S/` | 0 |

## 连接状态机与队列

`WiFiDataSource` 继续只负责 Wi-Fi 连接。新的 `MqttService` 在每次主循环中接收
Wi-Fi 当前状态：

1. Wi-Fi 未连接时，不尝试 MQTT 连接；如果 MQTT 曾连接，标记为断开。
2. Wi-Fi 已连接且 MQTT 未连接时，每 5 秒尝试连接一次。
3. MQTT 成功连接后订阅 `LoTS2C/`，再执行 `client.loop()`。
4. Wi-Fi 或 MQTT 断开不会阻塞传感器、屏幕和电机的主循环。

PubSubClient 回调只把收到的 Topic 和负载复制进固定长度接收队列（8 条、每条 JSON
最多 384 字节），绝不直接执行硬件动作。主循环按 FIFO 顺序解析并执行一条消息。

设备上行的遥测与回执统一进入固定长度发送队列（16 条、每条 JSON 最多 384 字节），
由主循环按 FIFO 顺序发布到 `LoTC2S/`。接收队列满时丢弃最新到达的命令且不执行；
发送队列满时丢弃最新的遥测消息。控制命令的回执优先入队：空间不足时先移除队列中最早的
遥测消息；只有队列全部都是回执时才丢弃最新回执并打印串口诊断。

## JSON 与 hash 关联

所有设备发布的 JSON 都含有 8 位十六进制 `hash`。设备主动遥测用递增序列号和时间
生成 FNV-1a 32 位哈希；控制消息如果带有 `hash`，回复原样保留该值，缺失时由设备生成。
`hash` 用于对应请求与回执，不作为安全认证手段。

控制指令统一外层字段：

```json
{"hash":"8f29c104","module":"relay","channel":1,"action":"on"}
```

模块特有参数放在 `params` 对象：

```json
{"hash":"8f29c105","module":"servo","action":"set","params":{"angle":90}}
```

成功回执：

```json
{"hash":"8f29c104","type":"reply","ok":true,"module":"relay","channel":1,"state":"on"}
```

失败回执保留同一 `hash`，并包含稳定的 `error` 值，例如 `invalid_module`、
`invalid_action`、`invalid_params` 或 `queue_full`。

## 模块控制协议

| module | action | 字段 | 约束 |
| --- | --- | --- | --- |
| `relay` | `on` / `off` | `channel` | 通道只能为 1 或 2 |
| `rgb` | `set` / `clear` | `params.r/g/b/brightness` | 颜色 0–255；亮度 0–25，限制峰值功耗 |
| `servo` | `set` | `params.angle` | 角度 0–180 |
| `stepper` | `move` / `stop` | `params.turns/direction/speed` | 圈数为正数；方向为 `cw` 或 `ccw`；速度单位 RPM |
| `input` | `get` | 无 | 返回当前摇杆与编码器数据，不控制硬件 |

步进电机的 `move` 命令按圈数、方向和 RPM 换算为半步与步进间隔。新 `move` 命令在电机
仍忙碌时拒绝并回执 `busy`；`stop` 可立即停止并释放线圈。

直流电机不接受 MQTT 指令，也不会出现在控制文档中。

## 上报策略

所有上报发布到 `LoTC2S/`，格式为 `type: "telemetry"`：

- 环境传感器：每 20 秒一次，含温度、湿度、气压、光照、海拔、麦克风强度和传感器连接状态。
- 摇杆与旋转编码器：不主动上报。服务端使用 `module: "input"`、`action: "get"`
  发起查询后，设备以同一 `hash` 回传当前摇杆坐标、摇杆按键、编码器位置与编码器按键状态。
- RFID：读取到新卡号时立即上报。
- 继电器、RGB、舵机、步进电机：仅在命令执行后通过 `reply` 回报状态，不额外周期上报。

## 代码边界

- `MqttCredentials.h`：服务器配置。
- `MqttService`：Wi-Fi 依赖、连接/订阅、发送/接收队列、哈希和 JSON 封装。
- `ModuleCommandDispatcher`：JSON 校验并将命令分发给 Relay、RGB、Servo、Stepper。
- 现有模块类：补充所需的安全状态读取和控制方法；不耦合 MQTT 库。
- `iot-controller.ino`：按既有轮询模型调用 Wi-Fi、MQTT、外设和屏幕刷新。

## 验证

测试先验证 JSON 的合法/非法解析、回执 hash 保留、设备 hash 生成、接收/发送队列 FIFO
行为与队列满策略。再验证各模块的参数边界和控制分发。使用 ESP32-S3 FQBN 编译测试草图
和完整工程。

完成后新增 `docs/MQTT指令格式说明.md`，供服务端和客户端直接对接。
