import {
  BLE_DEVICE_NAME_PREFIX,
  BLE_RX_UUID,
  BLE_SERVICE_UUID,
  BLE_TX_UUID,
  JsonLineDecoder,
  createRequestId,
  encodeWifiRequest,
  isReplyForRequest,
} from '../utils/protocol.js'

function callUni(method, options = {}) {
  return new Promise((resolve, reject) => {
    const api = uni[method]
    if (typeof api !== 'function') {
      reject(new Error(`当前平台不支持 ${method}`))
      return
    }
    api({
      ...options,
      success: resolve,
      fail: (error) => reject(toUserError(error)),
    })
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

function deviceName(device) {
  return device?.name || device?.localName || ''
}

export class BleService {
  constructor() {
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
  }

  async scan(onDevices) {
    await this.openAdapter()
    this.devices.clear()
    this.removeDeviceFoundListener()
    this.deviceFoundHandler = (result) => {
      for (const device of result?.devices || []) {
        if (!deviceName(device).startsWith(BLE_DEVICE_NAME_PREFIX)) {
          continue
        }
        const previous = this.devices.get(device.deviceId) || {}
        this.devices.set(device.deviceId, { ...previous, ...device })
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
    await this.openAdapter()
    await this.stopDiscovery()
    await callUni('createBLEConnection', { deviceId, timeout: 10000 })

    this.deviceId = deviceId
    this.messageHandler = onMessage
    this.disconnectHandler = onDisconnect
    this.decoder.reset()

    const serviceResult = await callUni('getBLEDeviceServices', { deviceId })
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
    const characteristics = characteristicResult.characteristics || []
    const rx = characteristics.find(
      (candidate) =>
        sameUuid(candidate.uuid, BLE_RX_UUID) &&
        (candidate.properties?.write || candidate.properties?.writeNoResponse),
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
  }

  async sendWifiCredentials(ssid, password) {
    if (!this.deviceId || !this.rxCharacteristicId) {
      throw new Error('请先连接控制器蓝牙')
    }
    const requestId = createRequestId()
    const chunks = encodeWifiRequest(requestId, ssid, password)
    this.currentRequestId = requestId

    for (const chunk of chunks) {
      const value = chunk.buffer.slice(
        chunk.byteOffset,
        chunk.byteOffset + chunk.byteLength,
      )
      await callUni('writeBLECharacteristicValue', {
        deviceId: this.deviceId,
        serviceId: this.serviceId,
        characteristicId: this.rxCharacteristicId,
        value,
      })
    }
    return requestId
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
    this.characteristicHandler = (result) => {
      if (
        result.deviceId !== this.deviceId ||
        !sameUuid(result.characteristicId, BLE_TX_UUID)
      ) {
        return
      }
      try {
        const messages = this.decoder.push(result.value)
        for (const message of messages) {
          if (isReplyForRequest(message, this.currentRequestId)) {
            this.messageHandler?.(message)
          }
        }
      } catch (_) {
        this.messageHandler?.({
          id: this.currentRequestId,
          event: 'protocol_error',
          reason: 'invalid_response',
        })
      }
    }
    this.connectionStateHandler = (result) => {
      if (result.deviceId === this.deviceId && !result.connected) {
        const callback = this.disconnectHandler
        this.resetConnection()
        callback?.()
      }
    }
    uni.onBLECharacteristicValueChange(this.characteristicHandler)
    uni.onBLEConnectionStateChange(this.connectionStateHandler)
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
    this.deviceId = ''
    this.serviceId = ''
    this.rxCharacteristicId = ''
    this.txCharacteristicId = ''
    this.currentRequestId = ''
    this.messageHandler = null
    this.disconnectHandler = null
    this.decoder.reset()
  }
}
