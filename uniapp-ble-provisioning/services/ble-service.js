import {
  BLE_RX_UUID,
  BLE_SERVICE_UUID,
  BLE_TX_UUID,
  JsonLineDecoder,
  createRequestId,
  encodeWifiRequest,
  encodeMqttRequest,
  isReplyForRequest,
} from '../utils/protocol.js'

const REQUESTED_MTU = 247
// Arduino-ESP32 3.3.3 defers onWrite() by a connection interval plus 5 ms.
// Native write completion alone must not trigger immediate back-to-back writes.
const WRITE_SETTLE_MS = 120

function callUni(method, options = {}, timeoutMs = 0) {
  return new Promise((resolve, reject) => {
    const api = uni[method]
    if (typeof api !== 'function') {
      reject(new Error(`当前平台不支持 ${method}`))
      return
    }
    const timer = timeoutMs ? setTimeout(() => reject(new Error(`${method} 回调超时`)), timeoutMs) : null
    try {
      api({
        ...options,
        success: value => { clearTimeout(timer); resolve(value) },
        fail: error => { clearTimeout(timer); reject(toUserError(error)) },
      })
    } catch (error) { clearTimeout(timer); reject(error) }
  })
}

function toUserError(error) {
  const code = Number(error?.errCode)
  const messages = {
    10000: '蓝牙适配器未初始化',
    10001: '请打开手机蓝牙后重试',
    10002: '未找到指定蓝牙设备',
    10003: '蓝牙连接失败，请靠近设备后重试',
    10004: '未找到设备的配网服务',
    10005: '未找到设备的配网特征值',
    10006: '蓝牙连接已断开',
    10007: '当前特征值不支持此操作',
    10008: '手机蓝牙状态异常，请重新打开蓝牙',
    10009: '当前系统版本不支持所需蓝牙能力',
    10012: '蓝牙连接超时，请重试',
    10013: '蓝牙参数无效',
  }
  return new Error(messages[code] || error?.errMsg || '蓝牙操作失败')
}

function sameUuid(left, right) {
  return String(left || '').toUpperCase() === right.toUpperCase()
}

function deviceDisplayName(device) {
  const name = String(device?.name || '').trim()
  if (name) {
    return name
  }
  return String(device?.localName || '').trim()
}

export class BleService {
  constructor({ requestTimeoutMs = 45000, writeTimeoutMs = 6000, mtuTimeoutMs = 1500 } = {}) {
    this.deviceId = ''
    this.serviceId = ''
    this.rxCharacteristicId = ''
    this.txCharacteristicId = ''
    this.currentRequestId = ''
    this.decoder = new JsonLineDecoder()
    this.devices = new Map()
    this.deviceFoundHandler = null
    this.characteristicHandler = null
    this.connectionStateHandler = null
    this.messageHandler = null
    this.disconnectHandler = null
    this.adapterOpen = false
    this.scanning = false
    this.requestTimeoutMs = requestTimeoutMs
    this.requestTimer = null
    this.sending = false
    this.connectionGeneration = 0
    this.connecting = false
    this.writeTimeoutMs = writeTimeoutMs
    this.mtuTimeoutMs = mtuTimeoutMs
    this.writePayloadBytes = 20
    this.pendingWrite = null
    this.writeToken = 0
    this.requiresReconnect = false
    this.sendingLastChunk = false
  }

  async scan(onDevices) {
    await this.openAdapter()
    await this.stopDiscovery()
    this.devices.clear()
    this.removeDeviceFoundListener()
    this.deviceFoundHandler = (result) => {
      for (const device of result?.devices || []) {
        const previous = this.devices.get(device.deviceId) || {}
        const merged = { ...previous, ...device }
        const displayName = deviceDisplayName(merged)
        if (!displayName) {
          continue
        }
        this.devices.set(device.deviceId, {
          ...merged,
          displayName,
        })
      }
      onDevices(
        Array.from(this.devices.values()).sort(
          (left, right) =>
            Number(right.RSSI ?? -Infinity) - Number(left.RSSI ?? -Infinity),
        ),
      )
    }
    uni.onBluetoothDeviceFound(this.deviceFoundHandler)
    await callUni('startBluetoothDevicesDiscovery', {
      allowDuplicatesKey: true,
      interval: 1000,
    })
    this.scanning = true
  }

  async connect(deviceId, onMessage, onDisconnect) {
    if (this.connecting) throw new Error('正在连接设备，请稍候')
    this.connecting = true
    try { await this.establishConnection(deviceId, onMessage, onDisconnect) }
    finally { this.connecting = false }
  }

  async establishConnection(deviceId, onMessage, onDisconnect) {
    if (this.deviceId) await this.disconnect()
    const generation = ++this.connectionGeneration
    await this.openAdapter()
    await this.stopDiscovery()
    if (generation !== this.connectionGeneration) throw new Error('蓝牙连接已取消')
    await callUni('createBLEConnection', { deviceId, timeout: 10000 })
    if (generation !== this.connectionGeneration) {
      try { await callUni('closeBLEConnection', { deviceId }) } catch (_) {}
      throw new Error('蓝牙连接已取消')
    }

    this.deviceId = deviceId
    this.messageHandler = onMessage
    this.disconnectHandler = onDisconnect
    this.writePayloadBytes = 20
    this.decoder.reset()
    try {
      const serviceResult = await callUni('getBLEDeviceServices', { deviceId })
      if (generation !== this.connectionGeneration) throw new Error('蓝牙连接已断开')
      const service = (serviceResult.services || []).find((candidate) =>
        sameUuid(candidate.uuid, BLE_SERVICE_UUID),
      )
      if (!service) {
        throw new Error('该设备没有发现 Wi-Fi 配网服务')
      }
      this.serviceId = service.uuid

      const characteristicResult = await callUni('getBLEDeviceCharacteristics', {
        deviceId,
        serviceId: this.serviceId,
      })
      if (generation !== this.connectionGeneration) throw new Error('蓝牙连接已断开')
      const characteristics = characteristicResult.characteristics || []
      const rx = characteristics.find(
        (candidate) =>
          sameUuid(candidate.uuid, BLE_RX_UUID) &&
          candidate.properties?.write,
      )
      const tx = characteristics.find(
        (candidate) =>
          sameUuid(candidate.uuid, BLE_TX_UUID) && candidate.properties?.notify,
      )
      if (!rx || !tx) {
        throw new Error('该设备的配网收发特征值不完整')
      }
      this.rxCharacteristicId = rx.uuid
      this.txCharacteristicId = tx.uuid

      this.installConnectionListeners()
      await callUni('notifyBLECharacteristicValueChange', {
        state: true,
        deviceId,
        serviceId: this.serviceId,
        characteristicId: this.txCharacteristicId,
      })
      if (generation !== this.connectionGeneration || !this.deviceId) throw new Error('蓝牙连接已断开')
      await this.negotiateMtu(generation)
      if (generation !== this.connectionGeneration || !this.deviceId) throw new Error('蓝牙连接已断开')
    } catch (error) {
      this.removeConnectionListeners()
      try {
        await callUni('closeBLEConnection', { deviceId })
      } catch (_) {
        // Preserve the discovery/notification error for the page.
      }
      this.resetConnection()
      throw error
    }
  }

  async sendWifiCredentials(ssid, password) {
    const requestId = createRequestId()
    return this.sendRequest(requestId, encodeWifiRequest(requestId, ssid, password, this.writePayloadBytes))
  }

  async sendMqttConfiguration(configuration) {
    const requestId = createRequestId()
    return this.sendRequest(requestId, encodeMqttRequest(requestId, configuration, this.writePayloadBytes))
  }

  async sendRequest(requestId, chunks) {
    if (!this.deviceId || !this.rxCharacteristicId) {
      throw new Error('请先连接控制器蓝牙')
    }
    if (this.requiresReconnect) throw new Error('上次蓝牙发送未完成，请断开蓝牙后重新连接')
    if (this.currentRequestId || this.sending) throw new Error('正在等待控制器处理上一条配置')
    this.decoder.reset()
    this.currentRequestId = requestId
    this.sending = true
    const generation = this.connectionGeneration
    this.sendingLastChunk = false
    console.info(`[BLE TX] begin id=${requestId} packets=${chunks.length} payload=${this.writePayloadBytes}`)
    this.requestTimer = setTimeout(() => {
      if (this.currentRequestId !== requestId) return
      this.requiresReconnect = this.sending
      this.clearRequest(new Error('等待控制器回复超时'))
      this.deliverMessage({ id: requestId, event: 'error', reason: 'device_timeout' })
    }, this.requestTimeoutMs)
    try {
      for (let sequence = 0; sequence < chunks.length; sequence++) {
        if (!this.deviceId || generation !== this.connectionGeneration || this.currentRequestId !== requestId) throw new Error('蓝牙配置已取消或连接已断开')
        const chunk = chunks[sequence]
        const value = chunk.buffer.slice(chunk.byteOffset, chunk.byteOffset + chunk.byteLength)
        this.sendingLastChunk = sequence === chunks.length - 1
        console.info(`[BLE TX] id=${requestId} seq=${sequence} bytes=${chunk.byteLength}`)
        await this.writeChunk(requestId, value, this.sendingLastChunk)
        if (!this.sendingLastChunk) await new Promise(resolve => setTimeout(resolve, WRITE_SETTLE_MS))
      }
      if (generation !== this.connectionGeneration) throw new Error('蓝牙连接已断开')
      return requestId
    } catch (error) {
      if (this.currentRequestId === requestId) {
        this.requiresReconnect = true // A partial JSON frame may still be on the device.
        this.clearRequest(error)
      }
      throw error
    } finally {
      if (generation === this.connectionGeneration) this.sending = false
    }
  }

  async negotiateMtu(generation) {
    let actualMtu
    if (typeof uni.setBLEMTU === 'function') {
      try {
        const result = await callUni('setBLEMTU', { deviceId: this.deviceId, mtu: REQUESTED_MTU }, this.mtuTimeoutMs)
        actualMtu = result?.mtu
      } catch (_) { /* Keep the conservative payload if negotiation is unsupported. */ }
    }
    if (generation !== this.connectionGeneration) return
    if (typeof uni.getBLEMTU === 'function') {
      try {
        const result = await callUni('getBLEMTU', { deviceId: this.deviceId, writeType: 'write' }, this.mtuTimeoutMs)
        actualMtu = result?.mtu
      } catch (_) { actualMtu = undefined }
    }
    if (generation !== this.connectionGeneration) return
    this.writePayloadBytes = Number.isInteger(actualMtu) && actualMtu >= 23 && actualMtu <= 517
      ? Math.min(actualMtu - 3, REQUESTED_MTU - 3) : 20
    console.info(`[BLE MTU] payload=${this.writePayloadBytes} bytes`)
  }

  writeChunk(requestId, value, last) {
    return new Promise((resolve, reject) => {
      const token = ++this.writeToken
      let settled = false
      const finish = error => {
        if (settled) return
        settled = true
        clearTimeout(timer)
        // Compare a scalar, not object identity: Vue wraps service state in proxies.
        if (this.pendingWrite?.token === token) this.pendingWrite = null
        if (error) reject(error); else resolve()
      }
      const timer = setTimeout(() => finish(new Error('蓝牙写入超时，请重新连接后重试；配置结果尚未确认')), this.writeTimeoutMs)
      this.pendingWrite = { id: requestId, token, last, resolve: () => finish(), reject: finish }
      try {
        uni.writeBLECharacteristicValue({
          deviceId: this.deviceId, serviceId: this.serviceId,
          characteristicId: this.rxCharacteristicId, value, writeType: 'write',
          success: () => finish(), fail: error => finish(toUserError(error)),
        })
      } catch (error) { finish(error) }
    })
  }

  clearRequest(error = new Error('蓝牙配置请求已结束')) {
    this.pendingWrite?.reject(error)
    clearTimeout(this.requestTimer)
    this.requestTimer = null
    this.currentRequestId = ''
  }

  async disconnect() {
    await this.stopDiscovery()
    this.removeDeviceFoundListener()
    const deviceId = this.deviceId
    this.removeConnectionListeners()
    this.resetConnection()
    if (deviceId) await callUni('closeBLEConnection', { deviceId })
  }

  async close() {
    await this.stopDiscovery()
    this.removeDeviceFoundListener()
    this.removeConnectionListeners()
    if (this.deviceId) {
      try {
        await callUni('closeBLEConnection', { deviceId: this.deviceId })
      } catch (_) {
        // The platform may already have closed a disconnected link.
      }
    }
    if (this.adapterOpen) {
      try {
        await callUni('closeBluetoothAdapter')
      } catch (_) {
        // Adapter cleanup is best effort when the page is leaving.
      }
    }
    this.resetConnection()
    this.adapterOpen = false
  }

  async openAdapter() {
    if (this.adapterOpen) {
      return
    }
    await callUni('openBluetoothAdapter')
    this.adapterOpen = true
  }

  async stopDiscovery() {
    if (!this.scanning) {
      return
    }
    try {
      await callUni('stopBluetoothDevicesDiscovery')
    } finally {
      this.scanning = false
    }
  }

  installConnectionListeners() {
    this.removeConnectionListeners()
    const generation = this.connectionGeneration
    this.characteristicHandler = (result) => {
      if (
        generation !== this.connectionGeneration ||
        result.deviceId !== this.deviceId ||
        !sameUuid(result.characteristicId, BLE_TX_UUID)
      ) {
        return
      }
      let messages
      try { messages = this.decoder.push(result.value) }
      catch (_) {
        this.decoder.reset()
        messages = [{ id: '00000000', event: 'protocol_error', reason: 'invalid_response' }]
      }
      for (const message of messages) {
        if (message.event === 'chunk_ack') continue // Ignore legacy firmware receipts.
        if (this.currentRequestId && (isReplyForRequest(message, this.currentRequestId) ||
            (message.id === '00000000' && ['error', 'protocol_error'].includes(message.event)))) {
          console.info(`[BLE RX] id=${message.id} event=${message.event} reason=${message.reason || '-'}`)
          if (['wifi_connected', 'mqtt_connected'].includes(message.event) && this.pendingWrite?.last) {
            this.pendingWrite.resolve()
          }
          if (['wifi_connected', 'wifi_failed', 'mqtt_connected', 'mqtt_failed', 'error', 'protocol_error'].includes(message.event)) {
            if (this.sending && !this.sendingLastChunk) this.requiresReconnect = true
            this.clearRequest()
          }
          this.deliverMessage(message)
        }
      }
    }
    this.connectionStateHandler = (result) => {
      if (generation === this.connectionGeneration && result.deviceId === this.deviceId && !result.connected) {
        const callback = this.disconnectHandler
        this.removeConnectionListeners()
        this.resetConnection()
        callback?.()
      }
    }
    uni.onBLECharacteristicValueChange(this.characteristicHandler)
    uni.onBLEConnectionStateChange(this.connectionStateHandler)
  }

  deliverMessage(message) {
    try { this.messageHandler?.(message) }
    catch (error) {
      console.error('[BLE RX] page handler failed', error)
      if (this.sending && !this.sendingLastChunk) this.requiresReconnect = true
      this.clearRequest()
      try { this.messageHandler?.({ id: message.id, event: 'error', reason: 'reply_processing_failed' }) }
      catch (handlerError) { console.error('[BLE RX] error handler failed', handlerError) }
    }
  }

  removeDeviceFoundListener() {
    if (this.deviceFoundHandler && typeof uni.offBluetoothDeviceFound === 'function') {
      uni.offBluetoothDeviceFound(this.deviceFoundHandler)
    }
    this.deviceFoundHandler = null
  }

  removeConnectionListeners() {
    if (
      this.characteristicHandler &&
      typeof uni.offBLECharacteristicValueChange === 'function'
    ) {
      uni.offBLECharacteristicValueChange(this.characteristicHandler)
    }
    if (
      this.connectionStateHandler &&
      typeof uni.offBLEConnectionStateChange === 'function'
    ) {
      uni.offBLEConnectionStateChange(this.connectionStateHandler)
    }
    this.characteristicHandler = null
    this.connectionStateHandler = null
  }

  resetConnection() {
    this.connectionGeneration += 1
    this.clearRequest()
    this.sending = false
    this.deviceId = ''
    this.serviceId = ''
    this.rxCharacteristicId = ''
    this.txCharacteristicId = ''
    this.currentRequestId = ''
    this.writePayloadBytes = 20
    this.requiresReconnect = false
    this.sendingLastChunk = false
    this.messageHandler = null
    this.disconnectHandler = null
    this.decoder.reset()
  }
}
