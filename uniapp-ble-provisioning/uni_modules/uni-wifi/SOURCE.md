# 来源

本目录的插件文件来自用户提供、已验证能扫描的本地工程：

`wife1.0/ble/uni_modules/uni-wifi/`

插件标识 `uni-wifi`，版本 **1.0.9**，原有 13 个文件完整导入，保留上游实现和元数据；仅统一文本换行。此说明为本工程补充文件。

官方插件页面：https://ext.dcloud.net.cn/plugin?id=10337

本工程使用 `startWifi`、`onGetWifiList`、`getWifiList`、`offGetWifiList`、`stopWifi`。Android 列表回调在收到结果后会清空，调用端每次扫描都需重新注册。微信小程序使用平台内置接口，不调用本插件的 Android 原生实现。未复制参考页面不同的 BLE 配网协议或打印密码的代码。
