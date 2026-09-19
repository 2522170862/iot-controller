# BLE Wi-Fi Provisioning Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Add always-available BLE Wi-Fi provisioning to the ESP32-S3 firmware and a UniApp Vue 3 client for WeChat Mini Program and Android App.

**Architecture:** The firmware separates newline-framed JSON parsing, NVS credential storage, Wi-Fi validation/rollback, and BLE transport into focused units driven by the existing non-blocking Arduino loop. The UniApp client scans Wi-Fi with the phone, connects to a Nordic-UART-style custom GATT service, and sends/receives 20-byte UTF-8 chunks while presenting one continuous provisioning workflow.

**Tech Stack:** Arduino ESP32 Core 3.3.3, ArduinoJson, ESP32 BLE library, Preferences/NVS, existing PubSubClient integration, UniApp Vue 3, JavaScript, Node built-in test runner/assertions.

**Spec:** `docs/superpowers/specs/2026-09-19-ble-wifi-provisioning-design.md`

## Global Constraints

- Keep the existing Arduino IDE `.ino` project structure.
- Target ESP32-S3 DevKitC-v1 N16R8 and the existing `esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi` configuration.
- Use GPIO3 for Wi-Fi status and GPIO44 for BLE-client status.
- GPIO44 is accepted as a normal output; UART0 RX is unavailable while it is used this way.
- On first boot with no saved NVS credentials, connect with `WifiCredentials.h`.
- Save new credentials only after `WL_CONNECTED`; failed validation must restore the previous network.
- Keep BLE advertising available after Wi-Fi connects.
- The phone, not ESP32, scans nearby Wi-Fi.
- Support WeChat Mini Program and Android App; do not implement iOS App support.
- Accept plaintext BLE GATT provisioning for this development version, but never log or persist the password in the UniApp client.
- Preserve all unrelated dirty-worktree changes and the current MQTT, sensor, motor, RFID, joystick, encoder, relay, LCD, and RGB behavior.
- Use the fixed GATT UUIDs `6E400001-B5A3-F393-E0A9-E50E24DCCA9E` (service), `6E400002-B5A3-F393-E0A9-E50E24DCCA9E` (RX), and `6E400003-B5A3-F393-E0A9-E50E24DCCA9E` (TX).
- Use newline-terminated UTF-8 JSON and chunks no larger than 20 bytes in both directions.

---

## File Structure

### Firmware files to create

- `BleProvisioningProtocol.h/.cpp`: request model, newline frame assembler, JSON validation, and reply serialization.
- `WifiCredentialStore.h/.cpp`: fixed-size credential model and Preferences/NVS adapter.
- `WifiProvisioningCoordinator.h/.cpp`: non-blocking validation, timeout, save, rollback, and result events.
- `BleProvisioningService.h/.cpp`: ESP32 BLE server, RX ingestion queue, TX notification queue, advertising, and connection state.
- `tests/ble_provisioning_protocol_test/ble_provisioning_protocol_test.ino`: protocol and framing tests.
- `tests/wifi_credential_policy_test/wifi_credential_policy_test.ino`: credential validity and boot-selection tests.
- `tests/wifi_provisioning_coordinator_test/wifi_provisioning_coordinator_test.ino`: state-machine success, failure, timeout, and busy tests.

### Firmware files to modify

- `PeripheralPins.h`: add `kBleIndicator = 44`.
- `ConnectionStatusIndicators.h/.cpp`: three indicators and timed Wi-Fi-validation blinking.
- `WiFiDataSource.h/.cpp`: runtime credentials, non-blocking reconnect, candidate connection, and connection-result reporting.
- `iot-controller.ino`: construct, initialize, and poll the provisioning components.
- `tests/connection_status_indicators_test/connection_status_indicators_test.ino`: GPIO44 and Wi-Fi blink assertions.

### UniApp files to create

- `uniapp-ble-provisioning/package.json`: Vue 3 project metadata and protocol test command.
- `uniapp-ble-provisioning/App.vue`, `main.js`, `pages.json`, `manifest.json`, `uni.scss`: HBuilderX-compatible application shell.
- `uniapp-ble-provisioning/pages/index/index.vue`: single-page provisioning UI.
- `uniapp-ble-provisioning/services/ble-service.js`: BLE discovery, connection, GATT discovery, notification handling, and sequential writes.
- `uniapp-ble-provisioning/services/wifi-service.js`: WeChat/Android Wi-Fi initialization, permissions, scan, filtering, and cleanup.
- `uniapp-ble-provisioning/utils/protocol.js`: UTF-8 framing, chunking, request IDs, and reply parsing.
- `uniapp-ble-provisioning/tests/protocol.test.mjs`: Node tests for Unicode framing, chunks, and reply correlation.
- `uniapp-ble-provisioning/README.md`: HBuilderX, WeChat, Android permission, and build instructions.

### Documentation files to modify

- `docs/项目代码文件说明.md`: new firmware and UniApp file responsibilities/functions.
- `README.md`: provisioning entry point and links.

---

### Task 1: BLE provisioning protocol and frame assembler

**Files:**
- Create: `BleProvisioningProtocol.h`
- Create: `BleProvisioningProtocol.cpp`
- Create: `tests/ble_provisioning_protocol_test/ble_provisioning_protocol_test.ino`

**Interfaces:**
- Produces: `BleWifiRequest { char id[9]; char ssid[33]; char password[64]; }`.
- Produces: `BleProtocolResult` values `kOk`, `kNeedMore`, `kInvalidJson`, `kInvalidRequest`, `kInvalidSsid`, `kInvalidPassword`, and `kMessageTooLong`.
- Produces: `BleProvisioningFrameAssembler::append(const uint8_t*, size_t, BleWifiRequest*)`.
- Produces: `BleProvisioningProtocol::makeEvent(const char* id, const char* event, const char* reason, const char* ip, char* output, size_t capacity)`.
- Produces: the three UUID constants and `kBleProvisioningMaxChunkBytes = 20`.

- [ ] **Step 1: Write framing and validation tests**

Test a valid request split at arbitrary byte offsets, two messages split across writes, Unicode SSID byte limits, missing/invalid IDs, wrong command, oversized SSID/password, and a 257-byte unterminated frame. Assert the assembler resets after terminal success or error.

```cpp
BleProvisioningFrameAssembler assembler;
BleWifiRequest request = {};
assert(assembler.append(first, firstLength, &request) == BleProtocolResult::kNeedMore);
assert(assembler.append(second, secondLength, &request) == BleProtocolResult::kOk);
assert(strcmp(request.id, "a1b2c3d4") == 0);
assert(strcmp(request.ssid, "Lab-WiFi") == 0);
```

- [ ] **Step 2: Run the new Arduino test and verify failure**

Run:

```powershell
arduino-cli compile --config-file tmp/arduino-cli-codex.yaml --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi" tests/ble_provisioning_protocol_test
```

Expected: FAIL because `BleProvisioningProtocol.h` does not exist.

- [ ] **Step 3: Implement the protocol**

Use a fixed 257-byte buffer, terminate frames on `\n`, ignore `\r`, and parse with ArduinoJson. Validate `id` as exactly eight hexadecimal characters, `cmd == "configure_wifi"`, SSID as 1-32 UTF-8 bytes, and password as 0-63 UTF-8 bytes. Serialize replies without the password.

- [ ] **Step 4: Run the protocol test**

Run the command from Step 2. Expected: PASS and a successful sketch build.

- [ ] **Step 5: Commit the protocol unit**

```powershell
git add BleProvisioningProtocol.h BleProvisioningProtocol.cpp tests/ble_provisioning_protocol_test
git commit -m "feat: add BLE provisioning protocol"
```

### Task 2: Credential policy and Preferences storage

**Files:**
- Create: `WifiCredentialStore.h`
- Create: `WifiCredentialStore.cpp`
- Create: `tests/wifi_credential_policy_test/wifi_credential_policy_test.ino`

**Interfaces:**
- Produces: `WifiCredentialsValue { char ssid[33]; char password[64]; }`.
- Produces: `WifiCredentialPolicy::isValid(const WifiCredentialsValue&)` and `selectBootCredentials(bool savedPresent, const WifiCredentialsValue& saved, const WifiCredentialsValue& fallback)`.
- Produces: `IWifiCredentialStore` with virtual `load(WifiCredentialsValue*)`, `save(const WifiCredentialsValue&)`, and `clear()` methods.
- Produces: `WifiCredentialStore`, the `IWifiCredentialStore` implementation using namespace `iot_wifi` and keys `valid`, `ssid`, `password`.

- [ ] **Step 1: Write credential-policy tests**

Assert that an empty SSID is rejected, 32-byte SSID and 63-byte password are accepted, larger values are rejected, saved credentials win when valid, and fallback credentials win when saved credentials are absent or invalid.

```cpp
const WifiCredentialsValue selected =
    WifiCredentialPolicy::selectBootCredentials(true, saved, fallback);
assert(strcmp(selected.ssid, saved.ssid) == 0);
```

- [ ] **Step 2: Run the credential test and verify failure**

```powershell
arduino-cli compile --config-file tmp/arduino-cli-codex.yaml --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi" tests/wifi_credential_policy_test
```

Expected: FAIL because `WifiCredentialStore.h` does not exist.

- [ ] **Step 3: Implement policy and NVS adapter**

Keep policy methods independent of `Preferences`. In `save`, write SSID/password first and set `valid=true` last; if either string write fails, leave `valid=false`. `load` returns false unless all fields pass policy validation. Never print the password.

- [ ] **Step 4: Run tests and compile the storage source with the full core**

Run the test from Step 2, then compile the main sketch. Expected: both compile successfully.

- [ ] **Step 5: Commit credential storage**

```powershell
git add WifiCredentialStore.h WifiCredentialStore.cpp tests/wifi_credential_policy_test
git commit -m "feat: persist validated Wi-Fi credentials"
```

### Task 3: Runtime Wi-Fi source and provisioning coordinator

**Files:**
- Modify: `WiFiDataSource.h`
- Modify: `WiFiDataSource.cpp`
- Create: `WifiProvisioningCoordinator.h`
- Create: `WifiProvisioningCoordinator.cpp`
- Create: `tests/wifi_provisioning_coordinator_test/wifi_provisioning_coordinator_test.ino`

**Interfaces:**
- Produces: `WifiAttemptState { kIdle, kConnecting, kConnected, kFailed }`.
- Produces: `IWifiConnection` with `connect(const WifiCredentialsValue&, uint32_t)`, `currentCredentials() const`, `attemptState() const`, and `localIp() const`.
- `WiFiDataSource` implements `IWifiConnection`.
- `WiFiDataSource::begin(const WifiCredentialsValue& credentials)` starts the boot network.
- Keep `WiFiDataSource::begin()` as a compatibility overload that constructs the compile-time fallback credentials; Task 5 replaces the main-sketch call with explicit boot selection.
- `WiFiDataSource::connect(const WifiCredentialsValue& credentials, uint32_t nowMs)` replaces the active runtime target and starts an attempt.
- `WiFiDataSource::currentCredentials() const`, `attemptState() const`, `localIp() const`, and existing `readNetwork() const` expose state.
- `WifiProvisioningCoordinator::start(const BleWifiRequest&, uint32_t nowMs)` returns false while busy.
- `WifiProvisioningCoordinator::poll(uint32_t nowMs)` advances without blocking.
- `WifiProvisioningCoordinator::active() const` drives the Wi-Fi blink state.
- `WifiProvisioningCoordinator::takeEvent(WifiProvisioningEvent*)` returns `kConnecting`, `kConnected`, or `kFailed` with request ID, reason, and optional IP.

- [ ] **Step 1: Write state-machine tests with fakes**

Inject interfaces `IWifiConnection` and `IWifiCredentialStore` into the coordinator. Test success saves exactly once; connection failure and 15-second timeout never save and reconnect the original credentials; a second start while active returns false; MQTT-independent success is emitted as soon as Wi-Fi connects.

```cpp
assert(coordinator.start(request, 100));
coordinator.poll(200);
fakeWifi.connectedValue = true;
coordinator.poll(300);
assert(fakeStore.saveCount == 1);
assert(coordinator.takeEvent(&event));
assert(event.kind == WifiProvisioningEventKind::kConnected);
```

- [ ] **Step 2: Run the coordinator test and verify failure**

```powershell
arduino-cli compile --config-file tmp/arduino-cli-codex.yaml --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi" tests/wifi_provisioning_coordinator_test
```

Expected: FAIL because the coordinator interfaces do not exist.

- [ ] **Step 3: Refactor `WiFiDataSource` to runtime credentials**

Replace direct reads of `WifiCredentials::kSsid/kPassword` inside reconnect attempts with a fixed-size `WifiCredentialsValue currentCredentials_`. Preserve the 10-second normal reconnect interval, `WiFi.persistent(false)`, station mode, auto reconnect, serial state transitions, dashboard server address, and `0.0.0.0` disconnected IP.

- [ ] **Step 4: Implement the coordinator**

Use a 15-second deadline and a small event queue that preserves `kConnecting` followed by a terminal event. Capture the previous credentials before replacing them. On failure, call `connect(previous, nowMs)` after queuing the error. On success, save the candidate before queuing success; if NVS save fails, report `storage_failed`, restore the previous credentials, and do not claim success.

- [ ] **Step 5: Run coordinator tests and compile the root sketch**

Run the coordinator test, `tests/mqtt_service_state_test`, and compile the root sketch. The compatibility `begin()` overload keeps the current main sketch buildable until Task 5. Expected: all commands exit 0.

- [ ] **Step 6: Commit the Wi-Fi transaction unit**

```powershell
git add WiFiDataSource.h WiFiDataSource.cpp WifiProvisioningCoordinator.h WifiProvisioningCoordinator.cpp tests/wifi_provisioning_coordinator_test
git commit -m "feat: validate and roll back provisioned Wi-Fi"
```

### Task 4: ESP32 BLE transport service

**Files:**
- Create: `BleProvisioningService.h`
- Create: `BleProvisioningService.cpp`
- Modify: `BleProvisioningProtocol.h`

**Interfaces:**
- Consumes: UUIDs, assembler, `BleWifiRequest`, and reply serialization from Task 1.
- Produces: `begin()`, `poll(uint32_t nowMs)`, `connected() const`, `takeRequest(BleWifiRequest*)`, and `enqueueEvent(const WifiProvisioningEvent&)`.

- [ ] **Step 1: Add a transport-queue contract test to the protocol sketch**

Extract a fixed-capacity `BleProvisioningRequestQueue` into the protocol unit. Assert FIFO behavior, capacity rejection, and that callback ingestion copies request data rather than retaining BLE-owned pointers.

- [ ] **Step 2: Run the protocol test and verify the new assertions fail**

Use the Task 1 compile command. Expected: FAIL because the queue does not exist.

- [ ] **Step 3: Implement the BLE server**

Use the ESP32 Core 3.3.3 APIs from `<BLEDevice.h>`, `<BLEServer.h>`, `<BLEUtils.h>`, and `<BLE2902.h>`. Build the device name from the last two MAC bytes, create RX with `PROPERTY_WRITE | PROPERTY_WRITE_NR`, create TX with `PROPERTY_NOTIFY`, and restart advertising from `poll()` after disconnect. BLE callbacks only update atomic/volatile connection flags and feed copied bytes into the assembler/queue; they do not change Wi-Fi.

- [ ] **Step 4: Implement sequential TX notifications**

Serialize each coordinator event as one newline-terminated JSON frame and notify at most one 20-byte chunk per `poll()` iteration. Retain offset until the whole frame is sent. Drop pending notifications when no client is connected without exposing credentials.

- [ ] **Step 5: Compile the full Arduino sketch**

```powershell
arduino-cli compile --config-file tmp/arduino-cli-codex.yaml --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi" .
```

Expected: the BLE headers and APIs resolve on ESP32 Core 3.3.3.

- [ ] **Step 6: Commit BLE transport**

```powershell
git add BleProvisioningService.h BleProvisioningService.cpp BleProvisioningProtocol.h tests/ble_provisioning_protocol_test
git commit -m "feat: add BLE provisioning transport"
```

### Task 5: Indicators and firmware integration

**Files:**
- Modify: `PeripheralPins.h`
- Modify: `ConnectionStatusIndicators.h`
- Modify: `ConnectionStatusIndicators.cpp`
- Modify: `tests/connection_status_indicators_test/connection_status_indicators_test.ino`
- Modify: `iot-controller.ino`

**Interfaces:**
- `ConnectionStatusIndicators(uint8_t wifiPin, uint8_t mqttPin, uint8_t blePin, ...)`.
- `update(uint32_t nowMs, bool wifiConnected, bool wifiValidating, bool mqttConnected, bool bleConnected)`.
- Wi-Fi validation blink interval: 250 ms.

- [ ] **Step 1: Extend the indicator test**

Assert all three pins are outputs and LOW immediately after `begin()`. Assert disconnected LOW, connected HIGH, validation toggles GPIO3 at 250 ms boundaries, MQTT follows its connection flag, and BLE GPIO44 is HIGH only while connected.

- [ ] **Step 2: Run the indicator test and verify failure**

```powershell
arduino-cli compile --config-file tmp/arduino-cli-codex.yaml --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi" tests/connection_status_indicators_test
```

Expected: FAIL because the constructor and update signature still accept two indicators.

- [ ] **Step 3: Implement the three-indicator state logic**

Set all pins LOW in `begin()`. During validation, compute GPIO3 from `(nowMs / 250) % 2`; outside validation, use the connected flag. Do not use delays.

- [ ] **Step 4: Integrate boot credentials, BLE, and coordinator in `iot-controller.ino`**

In `setup()`, load NVS credentials, choose saved or compile-time fallback, start Wi-Fi, start BLE, then start MQTT. In `loop()`, poll BLE, transfer every complete BLE request to the coordinator, poll the coordinator and Wi-Fi, forward coordinator events to BLE, then poll MQTT with the actual Wi-Fi status. Update all three indicators every loop. Do not alter the existing 500 ms dashboard cadence or telemetry schedule.

- [ ] **Step 5: Run all firmware tests and full compile**

Compile every directory under `tests/` that contains an `.ino`, run existing native C++ tests using their current build commands, then compile the root sketch. Expected: all existing tests and the full sketch pass.

- [ ] **Step 6: Commit firmware integration**

```powershell
git add PeripheralPins.h ConnectionStatusIndicators.h ConnectionStatusIndicators.cpp tests/connection_status_indicators_test iot-controller.ino
git commit -m "feat: integrate BLE Wi-Fi provisioning"
```

### Task 6: UniApp protocol utility and BLE service

**Files:**
- Create: `uniapp-ble-provisioning/package.json`
- Create: `uniapp-ble-provisioning/utils/protocol.js`
- Create: `uniapp-ble-provisioning/services/ble-service.js`
- Create: `uniapp-ble-provisioning/tests/protocol.test.mjs`

**Interfaces:**
- `createRequestId(): string` returns eight lowercase hex characters.
- `encodeWifiRequest(id, ssid, password): Uint8Array[]` returns newline-terminated chunks of at most 20 bytes.
- `JsonLineDecoder.push(ArrayBuffer): object[]` returns every complete notification and retains partial UTF-8 bytes safely.
- `BleService.scan(onDevices)`, `connect(deviceId, onMessage, onDisconnect)`, `sendWifiCredentials(ssid, password)`, and `close()`.

- [ ] **Step 1: Write Node protocol tests**

Use `node:test` and `node:assert/strict`. Test 8-character IDs, 20-byte limits, newline termination, Chinese SSID round-trip, split multibyte UTF-8 notification decoding, multiple frames in one notification, and ignoring a reply with a different ID.

```js
const chunks = encodeWifiRequest('a1b2c3d4', '实验室WiFi', '12345678')
assert.ok(chunks.every((chunk) => chunk.byteLength <= 20))
assert.equal(new TextDecoder().decode(concat(chunks)).endsWith('\n'), true)
```

- [ ] **Step 2: Run tests and verify failure**

```powershell
node --test uniapp-ble-provisioning/tests/protocol.test.mjs
```

Expected: FAIL because `utils/protocol.js` does not exist.

- [ ] **Step 3: Implement the protocol utility**

Use `TextEncoder`/`TextDecoder` with streaming decode, validate SSID/password by encoded byte count, and expose the fixed UUID constants. Do not store passwords or print request objects.

- [ ] **Step 4: Implement `BleService`**

Wrap `uni.openBluetoothAdapter`, `uni.startBluetoothDevicesDiscovery`, `uni.onBluetoothDeviceFound`, `uni.createBLEConnection`, `uni.getBLEDeviceServices`, `uni.getBLEDeviceCharacteristics`, `uni.notifyBLECharacteristicValueChange`, and `uni.writeBLECharacteristicValue` as Promises. Filter names by `IoT-Controller-`, write chunks sequentially, stop discovery after connection, remove listeners on `close()`, and convert platform error codes into Chinese user-facing messages.

- [ ] **Step 5: Run Node tests**

Run the Step 2 command. Expected: PASS.

- [ ] **Step 6: Commit UniApp transport**

```powershell
git add uniapp-ble-provisioning/package.json uniapp-ble-provisioning/utils uniapp-ble-provisioning/services/ble-service.js uniapp-ble-provisioning/tests
git commit -m "feat: add UniApp BLE provisioning client"
```

### Task 7: Phone Wi-Fi scan and provisioning page

**Files:**
- Create: `uniapp-ble-provisioning/services/wifi-service.js`
- Create: `uniapp-ble-provisioning/pages/index/index.vue`
- Create: `uniapp-ble-provisioning/App.vue`
- Create: `uniapp-ble-provisioning/main.js`
- Create: `uniapp-ble-provisioning/pages.json`
- Create: `uniapp-ble-provisioning/manifest.json`
- Create: `uniapp-ble-provisioning/uni.scss`

**Interfaces:**
- `WifiService.initialize()`, `scan()`, and `close()`.
- `scan()` resolves to unique networks sorted by signal strength with `{ SSID, BSSID, secure, signalStrength, frequency }`.
- Page state values: `checking`, `searching_device`, `device_connected`, `scanning_wifi`, `ready`, `sending`, `validating`, `success`, and `error`.

- [ ] **Step 1: Add pure Wi-Fi result tests**

Add `normalizeWifiList(rawList)` tests to `protocol.test.mjs`: remove blank SSIDs, deduplicate by SSID using the strongest entry, sort descending by signal, exclude entries known to be 5 GHz, and retain entries whose frequency is absent.

- [ ] **Step 2: Run tests and verify failure**

Run the Task 6 Node command. Expected: FAIL because `normalizeWifiList` does not exist.

- [ ] **Step 3: Implement `WifiService` with platform branches**

For WeChat, call `uni.authorize({scope:'scope.userLocation'})`, `uni.startWifi`, register `uni.onGetWifiList`, and call `uni.getWifiList`. For Android App, use the same Uni Wi-Fi API and document/install the official `uni-WiFi` ext API; request Android runtime permissions before scanning. Do not add an iOS branch. Always unregister Wi-Fi listeners in `close()`.

- [ ] **Step 4: Build the single-page UI**

Create sections for phone prerequisites, BLE device list, nearby Wi-Fi list, manual SSID entry, password input, start button, progress, final IP, and retry. Keep the password only in component memory, clear it after terminal success, mask it by default, disable duplicate submission during `sending/validating`, and never call `uni.setStorage` with credentials.

- [ ] **Step 5: Configure targets and permissions**

Set `vueVersion: "3"`, one `pages/index/index` route, and Android permissions for Bluetooth, Bluetooth Admin, Bluetooth Scan, Bluetooth Connect, Access/Change Wi-Fi State, and Fine Location. Add clear Chinese permission descriptions for WeChat Mini Program review.

- [ ] **Step 6: Run Node tests and HBuilderX static checks**

Run Node tests. Import the directory in HBuilderX and run “发行 → 小程序-微信” plus “发行 → 原生App-云打包/本地打包” static validation. Expected: no missing page, manifest, UUID, or JavaScript import errors.

- [ ] **Step 7: Commit the UniApp workflow**

```powershell
git add uniapp-ble-provisioning
git commit -m "feat: add phone Wi-Fi provisioning UI"
```

### Task 8: Documentation and end-to-end verification

**Files:**
- Create: `uniapp-ble-provisioning/README.md`
- Modify: `docs/项目代码文件说明.md`
- Modify: `README.md`

**Interfaces:**
- Documents the shared UUIDs, JSON request/replies, phone permission requirements, HBuilderX builds, GPIO behavior, reset/fallback behavior, and manual acceptance procedure.

- [ ] **Step 1: Write the deployment and test guide**

Document installing the official UniApp `uni-WiFi` ext API, selecting a WeChat AppID, Android permission prompts, building each target, flashing the ESP32, expected serial messages without passwords, and the nine hardware acceptance checks from the spec.

- [ ] **Step 2: Update file/function responsibility documentation**

Add every new firmware and UniApp file plus its public functions to `docs/项目代码文件说明.md`. Link the design, implementation plan, and UniApp README from the root README.

- [ ] **Step 3: Run automated verification**

Run:

```powershell
node --test uniapp-ble-provisioning/tests/protocol.test.mjs
arduino-cli compile --config-file tmp/arduino-cli-codex.yaml --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi" tests/ble_provisioning_protocol_test
arduino-cli compile --config-file tmp/arduino-cli-codex.yaml --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi" tests/wifi_credential_policy_test
arduino-cli compile --config-file tmp/arduino-cli-codex.yaml --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi" tests/wifi_provisioning_coordinator_test
arduino-cli compile --config-file tmp/arduino-cli-codex.yaml --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi" tests/connection_status_indicators_test
arduino-cli compile --config-file tmp/arduino-cli-codex.yaml --fqbn "esp32:esp32:esp32s3:FlashSize=16M,PSRAM=opi" .
```

Expected: all commands exit 0.

- [ ] **Step 4: Perform hardware acceptance**

Verify default-network first boot, WeChat and Android phone Wi-Fi scanning, GPIO3 validation blink/connected steady state, GPIO44 BLE state, wrong-password rollback, successful NVS reboot persistence, MQTT reconnection, and always-available BLE advertising. Record any hardware-only gaps explicitly rather than claiming them automated.

- [ ] **Step 5: Review the final diff for secrets and unrelated changes**

Run:

```powershell
git diff --check
git status --short
git diff --stat
```

Inspect new logs and docs to ensure no Wi-Fi password is printed or copied. Preserve pre-existing dirty files and stage only provisioning-related changes.

- [ ] **Step 6: Commit documentation and verified integration**

```powershell
git add README.md "docs/项目代码文件说明.md" uniapp-ble-provisioning/README.md
git commit -m "docs: document BLE Wi-Fi provisioning"
```
