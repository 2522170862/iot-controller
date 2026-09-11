# MQTT 指令格式说明

本说明对应当前 ESP32-S3 控制器工程的 MQTT 实现。直流电机模块暂未接入，
不支持 MQTT 指令。

## 1. 主题与基本规则

| 方向 | Topic | QoS | 用途 |
| --- | --- | --- | --- |
| 服务端 → 控制器 | `LoTS2C/` | 0 | 下发控制或查询指令 |
| 控制器 → 服务端 | `LoTC2S/` | 0 | 发布执行回执和主动遥测 |

- 负载使用 UTF-8 编码的单行 JSON；不要添加额外文本、换行或十六进制空格。
- 当前只有一个设备，主题后**不要**附加设备编号、`module` 或 `channel`。
- MQTT 服务端地址、端口和认证信息由固件中的 `MqttCredentials.h` 配置；认证信息不应写入业务消息或本说明。
- 所有由控制器发布的消息都带有 8 位十六进制字符串 `hash`。服务端下发命令时应携带自己的 `hash`，控制器会在回执中原样返回它，以便请求和回执一一对应。

## 2. 通用命令结构

```json
{
  "hash": "8f29c104",
  "module": "relay",
  "channel": 1,
  "action": "on"
}
```

| 字段 | 是否必填 | 说明 |
| --- | --- | --- |
| `hash` | 建议必填 | 8 位十六进制字符串，例如 `8f29c104`。缺失时控制器会为回执生成一个；格式错误的 `hash` 不可用于关联。 |
| `module` | 是 | 要操作或查询的模块名称。 |
| `channel` | 仅继电器必填 | 继电器通道，只能是整数 `1` 或 `2`。 |
| `action` | 是 | 模块动作名称。 |
| `params` | 视模块而定 | 模块专用参数对象。 |

字段名、模块名和动作名均区分大小写。参数范围以本说明为准；超出范围或缺少必填字段时，命令不会执行。

## 3. 模块控制指令

### 3.1 两路继电器

`module` 固定为 `relay`，`channel` 为 `1` 或 `2`，`action` 为 `on` 或 `off`。

开启第 1 路继电器：

```json
{"hash":"10000001","module":"relay","channel":1,"action":"on"}
```

关闭第 2 路继电器：

```json
{"hash":"10000002","module":"relay","channel":2,"action":"off"}
```

继电器模块采用低电平触发时，固件会在内部处理电平反转；MQTT 侧只需使用 `on` 和 `off`，不需要关心 GPIO 电平。

### 3.2 8×8 RGB LED 矩阵

`module` 固定为 `rgb`。

设置整块矩阵的颜色和亮度：

```json
{"hash":"10000003","module":"rgb","action":"set","params":{"r":255,"g":80,"b":0,"brightness":10}}
```

| 参数 | 类型与范围 | 说明 |
| --- | --- | --- |
| `r` | 整数，0–255 | 红色分量 |
| `g` | 整数，0–255 | 绿色分量 |
| `b` | 整数，0–255 | 蓝色分量 |
| `brightness` | 整数，0–25 | 矩阵亮度。上限被限制为 25，以控制峰值电流。 |

清空/熄灭矩阵：

```json
{"hash":"10000004","module":"rgb","action":"clear"}
```

### 3.3 舵机

`module` 固定为 `servo`，仅支持 `set`。`angle` 是 0–180 的整数角度。

```json
{"hash":"10000005","module":"servo","action":"set","params":{"angle":90}}
```

### 3.4 步进电机

`module` 固定为 `stepper`。

按圈数、方向和速度运动：

```json
{"hash":"10000006","module":"stepper","action":"move","params":{"turns":1.5,"direction":"cw","speed":8}}
```

| 参数 | 类型与范围 | 说明 |
| --- | --- | --- |
| `turns` | 数值，大于 0 且不大于 100 | 目标转动圈数，可使用小数。 |
| `direction` | `cw` 或 `ccw` | `cw` 为顺时针，`ccw` 为逆时针。 |
| `speed` | 整数，1–15 | 转速，单位 RPM。 |

立即停止当前动作：

```json
{"hash":"10000007","module":"stepper","action":"stop"}
```

若步进电机仍在执行上一次 `move`，新的 `move` 会被拒绝，并返回 `state: "busy"`。此时可等待完成，或先发送 `stop`。

### 3.5 操控输入查询（摇杆和旋转编码器）

`module` 固定为 `input`，只支持 `get`，不需要 `channel` 和 `params`。

```json
{"hash":"10000008","module":"input","action":"get"}
```

该指令不会控制硬件，而是返回当前摇杆的 X/Y 值、摇杆按键状态、编码器位置和编码器按键状态。摇杆数据不会周期性主动上报，应通过此指令查询。

## 4. 执行回执

控制器接到指令后会向 `LoTC2S/` 发布一条 `type: "reply"` 消息。除 `input/get` 外，普通成功回执格式如下：

```json
{"hash":"10000001","type":"reply","ok":true,"state":"on"}
```

| 字段 | 说明 |
| --- | --- |
| `hash` | 与有效请求相同；请求未带 `hash` 时由控制器生成。 |
| `type` | 固定为 `reply`。 |
| `ok` | `true` 表示已执行；`false` 表示未执行。 |
| `state` | 结果状态，见下表。 |

`state` 的常见值：

| `state` | 含义 |
| --- | --- |
| `on` / `off` | 继电器已开启/关闭。 |
| `set` | RGB 或舵机设置完成。 |
| `clear` | RGB 矩阵已清空。 |
| `moving` | 步进电机已接受运动任务。 |
| `stopped` | 步进电机已停止。 |
| `busy` | 步进电机正在执行先前的运动任务。 |
| `unavailable` | 对应硬件模块不可用。 |
| `invalid_command` | JSON、模块名、动作名、字段或参数不符合协议。 |

`input/get` 的成功回执包含实时输入数据：

```json
{
  "hash": "10000008",
  "type": "reply",
  "ok": true,
  "module": "input",
  "x": 2048,
  "y": 1980,
  "joystickPressed": false,
  "encoderPosition": 12,
  "encoderPressed": false
}
```

- `x`、`y`：摇杆 ADC 原始读数，通常为 0–4095。
- `joystickPressed`、`encoderPressed`：按键状态，`true` 表示按下。
- `encoderPosition`：编码器累计位置，可为负数。

## 5. 控制器主动遥测

主动遥测同样发布到 `LoTC2S/`，并带有 `type: "telemetry"` 和控制器生成的 `hash`。

### 环境数据：每 20 秒一条

```json
{
  "hash": "a1b2c3d4",
  "type": "telemetry",
  "module": "environment",
  "temperature": 24.75,
  "humidity": 62.79,
  "pressure": 973.02,
  "light": 600.5,
  "altitude": 340.0,
  "microphone": 18,
  "online": true
}
```

| 字段 | 单位/说明 |
| --- | --- |
| `temperature` | 摄氏度（°C） |
| `humidity` | 相对湿度（%） |
| `pressure` | 气压（hPa） |
| `light` | 光照强度（lux） |
| `altitude` | 海拔（m） |
| `microphone` | MAX4466 声音强度百分比（0–100） |
| `online` | RS485 环境传感器本次数据是否有效 |

### 旋转编码器：状态变化时上报

当编码器位置变化或按键状态变化时，控制器立即上报：

```json
{"hash":"a1b2c3d5","type":"telemetry","module":"encoder","position":13,"pressed":false}
```

### RFID：读到新卡时上报

```json
{"hash":"a1b2c3d6","type":"telemetry","module":"rfid","uid":"DEADBEEF"}
```

`uid` 为 RC522 读出的卡片 UID 十六进制文本。服务端应按 `hash` 去重或记录事件，避免重复消费。

## 6. 对接建议

1. 订阅 `LoTC2S/` 后再向 `LoTS2C/` 发布命令。
2. 每条命令生成不同的 8 位十六进制 `hash`，收到相同 `hash` 的 `reply` 才视为该命令完成。
3. QoS 为 0，网络抖动时服务端应自行设置超时与重试；重试时建议使用新的 `hash`，避免将旧回执误判为新请求。
4. 对步进电机的 `move` 不要高频重复下发；收到 `moving` 后等待完成或需要时发送 `stop`。
