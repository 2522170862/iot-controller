import test from 'node:test'
import assert from 'node:assert/strict'

import { BleService } from '../services/ble-service.js'

function successful(options, value = {}) {
  options.success(value)
}

test('starting a new scan stops an existing discovery session', async () => {
  let stopCalls = 0
  globalThis.uni = {
    openBluetoothAdapter: (options) => successful(options),
    startBluetoothDevicesDiscovery: (options) => successful(options),
    stopBluetoothDevicesDiscovery: (options) => {
      stopCalls += 1
      successful(options)
    },
    onBluetoothDeviceFound() {},
    offBluetoothDeviceFound() {},
  }
  const service = new BleService()

  await service.scan(() => {})
  await service.scan(() => {})

  assert.equal(stopCalls, 1)
})

test('failed service discovery closes and clears the partial connection', async () => {
  let closeCalls = 0
  globalThis.uni = {
    openBluetoothAdapter: (options) => successful(options),
    createBLEConnection: (options) => successful(options),
    getBLEDeviceServices: (options) => successful(options, { services: [] }),
    closeBLEConnection: (options) => {
      closeCalls += 1
      successful(options)
    },
  }
  const service = new BleService()

  await assert.rejects(
    service.connect('controller-1', () => {}, () => {}),
    /配网服务/,
  )
  assert.equal(closeCalls, 1)
  assert.equal(service.deviceId, '')
})

test('scan reports named devices without a project prefix and hides unnamed devices', async () => {
  let deviceFoundHandler = null
  let reportedDevices = []
  globalThis.uni = {
    openBluetoothAdapter: (options) => successful(options),
    startBluetoothDevicesDiscovery: (options) => successful(options),
    onBluetoothDeviceFound: (handler) => {
      deviceFoundHandler = handler
    },
    offBluetoothDeviceFound() {},
  }
  const service = new BleService()

  await service.scan((devices) => {
    reportedDevices = devices
  })
  deviceFoundHandler({
    devices: [
      {
        deviceId: 'other-sensor',
        name: 'Other Sensor',
        localName: '',
        RSSI: -55,
      },
      {
        deviceId: 'unnamed-device',
        name: '',
        localName: '',
        RSSI: -30,
      },
    ],
  })

  assert.deepEqual(
    reportedDevices.map(({ deviceId, displayName }) => ({
      deviceId,
      displayName,
    })),
    [
      { deviceId: 'other-sensor', displayName: 'Other Sensor' },
    ],
  )
})
