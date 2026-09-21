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
