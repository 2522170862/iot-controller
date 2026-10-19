# BLE MQTT configuration and configuration dialogs

Goal: extend the existing BLE provisioning flow to configure MQTT on ESP32-S3 and provide Bluetooth, Wi-Fi and MQTT dialogs in UniApp.

The user approved `configure_mqtt` with required id/server/port/username/password and independently optional subTopic/pubTopic/subQos. Implementation is authorized in the current Arduino/UniApp project; no push or device flashing is requested.

## Decisions

- Preserve existing Wi-Fi JSON and BLE UUIDs. UTF-8 JSON lines, 20-byte writes; extend the maximum frame to 1024 bytes for MQTT.
- Validate every supplied MQTT field; omitted optional fields retain the board's current values. Topic limits match the existing 40-byte message topic buffer; subscription QoS supports 0 and 1.
- Use NVS for validated MQTT settings, with the compiled configuration as the initial fallback. Save only after connection and subscription request succeed. Roll back on connection, validation timeout or storage failure.
- Serialize configuration transactions across Wi-Fi and MQTT. BLE callbacks only enqueue requests. Drop queued MQTT messages when changing endpoints/topics. All outgoing messages and the LCD use the active runtime configuration.
- Keep the existing unencrypted MQTT TCP transport. Client ID remains the board's existing configured ID. No new cloud service, permissions or dependencies in the shipped app.
- Three home cards open dialogs. Named BLE scanning and phone Wi-Fi scanning remain. Explicit disconnect clears pending requests/listeners; connection and validation errors remain visible.
- Do not persist passwords on the phone. Clear password fields after success/disconnection and bound client waiting with a timeout.

## Task 1: firmware protocol and runtime configuration

- Add configuration validation, NVS storage and a testable MQTT configuration coordinator.
- Extend BLE parsing/queueing to distinguish Wi-Fi and MQTT, retaining the old Wi-Fi API alias for existing sketches.
- Update MQTT service, main loop and LCD status to use runtime settings and serialize provisioning.
- Verify valid/invalid MQTT fields, optional-field retention, rollback, timeout, storage failure and unchanged Wi-Fi frames.

## Task 2: client protocol and BLE lifecycle

- Add MQTT JSON encoding with the same limits as firmware.
- Serialize writes and requests, match replies, report firmware errors, time out missing replies, support disconnect/reconnect and clear listeners.
- Run Node tests RED then GREEN, including early replies, incomplete frames and disconnection while sending.

## Task 3: configuration dialogs

- Move page state into a testable page-options module; keep the Vue template focused on rendering.
- Provide the three dialogs, MQTT advanced optional topic fields, explicit disconnect and clear busy/result states.
- Test request construction, state transitions and errors, and compile the Vue SFC when tooling is available.

## Task 4: integration and documentation

- Document MQTT formats, limits, persisted settings, rollback and true-device acceptance steps.
- Run the entire UniApp test suite, firmware protocol/coordinator checks and ESP32-S3 compilation proportionate to available tooling.
- Review the diff for secret leakage, hard-coded topics, stale queues, cross-session replies and competing configuration requests. Leave changes local for the user to test.

## Progress

- Inspected existing BLE, Wi-Fi provisioning, MQTT queues/service and UniApp flow. Found hard-coded publish topic and compile-time broker configuration.
- Task 1: protocol, coordinator, NVS slots, runtime MQTT service and main-loop integration implemented. Four host C++ test executables pass, including the original BLE and Wi-Fi coordinator assertion sketches.
- Task 2: MQTT encoder, serialized requests, deadline, disconnect/reconnect and session isolation implemented; included in the Node suite.
- Task 3: three cards and native UniApp dialogs implemented; Vue template/script compile and WeChat/App production resource builds passed.
- Independent read-only review found two Important issues: stale send failures after reconnect and success after the validation deadline. Both reproduced with failing regression tests, then fixed; an additional notify-setup disconnect race was also reproduced and fixed. Node suite is now 28/28; C++ regression suite passes.
- Review limitations: real radio/device timing, actual broker permissions and physical NVS fault injection require hardware acceptance. PubSubClient subscription success is transmission success, not SUBACK/ACL validation; documented as an existing transport limitation.
- Ruling: use plain CSS for the existing CSS-only styles, removing an unnecessary Sass build dependency. Keep builds local and do not push or flash without a separate request.
- Task 4 complete: final Node suite 28/28; all four host C++ test executables pass. Final ESP32-S3 full build passed (1,235,971 bytes flash, 73,960 bytes RAM) using the existing Arduino board profile. Confirmed the Arduino compilation copy contains the final deadline fix after its generated line marker.
- Final WeChat production artifacts include app.json and the configuration page WXML; final App resources include manifest.json and app-service.js. Verified both output bundles contain MQTT configuration, request timeout and session-generation logic. Production platform builds used installed HBuilderX tooling without adding dependencies to the project.
- No hardware flashing, real BLE/broker connection or remote Git push was performed. Physical acceptance remains as documented in docs/BLE_MQTT配置说明.md.
