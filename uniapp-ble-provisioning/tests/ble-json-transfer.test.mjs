import test from 'node:test'
import assert from 'node:assert/strict'
import { createRequire } from 'node:module'
import { BleService } from '../services/ble-service.js'
import pageOptions from '../services/configuration-page.js'
import { BLE_SERVICE_UUID, BLE_RX_UUID, BLE_TX_UUID, JsonLineDecoder } from '../utils/protocol.js'

function device({ mtu = 247, setResult, getMtu = true } = {}) {
  const state = { writes: [], handlers: {}, pendingWrites: [], requestMtu: 0 }
  const ok = (o, result = {}) => o.success(result)
  globalThis.uni = {
    openBluetoothAdapter: ok, createBLEConnection: ok, closeBLEConnection: ok,
    getBLEDeviceServices: o => ok(o, { services: [{ uuid: BLE_SERVICE_UUID }] }),
    getBLEDeviceCharacteristics: o => ok(o, { characteristics: [
      { uuid: BLE_RX_UUID, properties: { write: true, writeNoResponse: true } },
      { uuid: BLE_TX_UUID, properties: { notify: true } },
    ] }),
    notifyBLECharacteristicValueChange: ok,
    setBLEMTU: o => { state.requestMtu = o.mtu; ok(o, setResult ?? { mtu }) },
    onBLECharacteristicValueChange: fn => { state.handlers.message = fn },
    onBLEConnectionStateChange: fn => { state.handlers.connection = fn },
    offBLECharacteristicValueChange() {}, offBLEConnectionStateChange() {},
    writeBLECharacteristicValue: o => { state.writes.push(new Uint8Array(o.value)); ok(o) },
  }
  if (getMtu) uni.getBLEMTU = o => ok(o, { mtu })
  state.notify = text => state.handlers.message({ deviceId: 'board', characteristicId: BLE_TX_UUID,
    value: new TextEncoder().encode(text).buffer })
  state.reply = message => state.notify(JSON.stringify(message) + '\n')
  return state
}

const config = { server: 'broker.example.com', port: 1883, username: 'user', password: 'dummy&pass' }
const flush = () => new Promise(resolve => setImmediate(resolve))
async function drive(t, operation) {
  let finished = false, result, error
  operation.then(value => { result = value; finished = true }, value => { error = value; finished = true })
  for (let step = 0; step < 100 && !finished; step++) { await flush(); t.mock.timers.tick(120) }
  await flush()
  assert.ok(finished, 'transfer must finish without application-level chunk ACKs')
  if (error) throw error
  return result
}

test('confirmed large MTU sends MQTT as one raw JSON write with a system write response', async () => {
  const board = device(), messages = [], service = new BleService()
  await service.connect('board', m => messages.push(m), () => {})
  try {
    const write = uni.writeBLECharacteristicValue
    uni.writeBLECharacteristicValue = o => { assert.equal(o.writeType, 'write'); write(o) }
    const id = await service.sendMqttConfiguration(config)
    assert.equal(board.requestMtu, 247)
    assert.equal(board.writes.length, 1)
    const text = new TextDecoder().decode(board.writes[0])
    assert.equal(text[0], '{'); assert.ok(text.endsWith('\n'))
    assert.deepEqual(JSON.parse(text), { id, cmd: 'configure_mqtt', ...config })
    assert.deepEqual(messages, []) // Writing bytes is not MQTT connection success.
    board.reply({ id, event: 'mqtt_connected' })
    assert.equal(messages[0].event, 'mqtt_connected')
  } finally { await service.disconnect() }
})

test('small actual MTU splits UTF-8 JSON and paces writes past the deferred firmware callback', async t => {
  t.mock.timers.enable({ apis: ['setTimeout'] })
  const board = device({ mtu: 23 }), service = new BleService(), received = []
  await service.connect('board', () => {}, () => {})
  let sharedValue
  uni.writeBLECharacteristicValue = o => {
    sharedValue = new Uint8Array(o.value); board.writes.push(sharedValue); o.success({})
    setTimeout(() => received.push(sharedValue), 50) // Mirrors the SDK's delayed shared-value read.
  }
  try {
    const id = await drive(t, service.sendWifiCredentials('实验室', 'pa"ss\\word'))
    t.mock.timers.tick(120)
    assert.ok(board.writes.length > 1 && board.writes.every(bytes => bytes.length <= 20))
    assert.equal(received.length, board.writes.length)
    assert.deepEqual(JSON.parse(Buffer.concat(received).toString()), {
      id, cmd: 'configure_wifi', ssid: '实验室', password: 'pa"ss\\word',
    })
    board.reply({ id, event: 'wifi_connected' })
  } finally { await service.disconnect() }
})

test('set-MTU success without an actual value never assumes the requested size', async t => {
  t.mock.timers.enable({ apis: ['setTimeout'] })
  const board = device({ setResult: { errMsg: 'setBLEMTU:ok' }, getMtu: false }), service = new BleService()
  await service.connect('board', () => {}, () => {})
  try {
    const id = await drive(t, service.sendMqttConfiguration(config))
    assert.ok(board.writes.length > 1 && board.writes.every(bytes => bytes.length <= 20))
    assert.equal(JSON.parse(Buffer.concat(board.writes).toString()).id, id)
  } finally { await service.disconnect() }
})

test('actual MTU readback overrides a larger set-MTU result', async t => {
  t.mock.timers.enable({ apis: ['setTimeout'] })
  const board = device({ mtu: 23, setResult: { mtu: 247 } }), service = new BleService()
  await service.connect('board', () => {}, () => {})
  try {
    await drive(t, service.sendMqttConfiguration(config))
    assert.ok(board.writes.every(bytes => bytes.length <= 20))
  } finally { await service.disconnect() }
})

test('large configurations split only when exceeding the confirmed payload and preserve optional fields', async t => {
  t.mock.timers.enable({ apis: ['setTimeout'] })
  const board = device(), service = new BleService()
  const large = { server: 'a'.repeat(127), port: 1884, username: 'u'.repeat(63), password: 'p'.repeat(127),
    subTopic: 'in/' + 'x'.repeat(36), pubTopic: 'out/' + 'y'.repeat(35), subQos: 0 }
  await service.connect('board', () => {}, () => {})
  try {
    const id = await drive(t, service.sendMqttConfiguration(large))
    assert.ok(board.writes.length > 1 && board.writes.every(bytes => bytes.length <= 244))
    assert.deepEqual(JSON.parse(Buffer.concat(board.writes).toString()), { id, cmd: 'configure_mqtt', ...large })
  } finally { await service.disconnect() }
})

test('MTU API failure or missing callback falls back without hanging the BLE connection', async t => {
  t.mock.timers.enable({ apis: ['setTimeout'] })
  for (const mode of ['failure', 'no-callback']) {
    const board = device({ getMtu: false }), service = new BleService({ mtuTimeoutMs: 30 })
    uni.setBLEMTU = o => { if (mode === 'failure') o.fail({ errMsg: 'unsupported' }) }
    const connection = service.connect('board', () => {}, () => {})
    await drive(t, connection)
    try {
      await drive(t, service.sendMqttConfiguration(config))
      assert.ok(board.writes.every(bytes => bytes.length <= 20))
    } finally { await service.disconnect() }
  }
})

test('disconnect invalidates an in-flight MTU result before a new connection', async () => {
  const board = device({ getMtu: false }), service = new BleService()
  let negotiation
  uni.setBLEMTU = o => { negotiation = o }
  const connecting = service.connect('board', () => {}, () => {})
  const rejected = assert.rejects(connecting, /断开|取消/)
  await flush(); await service.disconnect()
  negotiation.success({ mtu: 247 }); await rejected
  assert.equal(service.deviceId, '')
  // A new peer reporting only the baseline MTU cannot inherit the previous size.
  uni.setBLEMTU = o => o.success({ mtu: 23 })
  await service.connect('board', () => {}, () => {})
  assert.equal(service.writePayloadBytes, 20)
  await service.disconnect()
})

test('missing native write callback times out and forbids retrying into a partial firmware frame', async t => {
  t.mock.timers.enable({ apis: ['setTimeout'] })
  const board = device(), service = new BleService({ writeTimeoutMs: 30 })
  await service.connect('board', () => {}, () => {})
  uni.writeBLECharacteristicValue = o => { board.writes.push(new Uint8Array(o.value)) }
  try {
    const sending = service.sendMqttConfiguration(config)
    const rejected = assert.rejects(sending, /写入.*超时/)
    await flush(); t.mock.timers.tick(30); await rejected
    await assert.rejects(service.sendMqttConfiguration(config), /重新连接|重连/)
    assert.equal(board.writes.length, 1)
  } finally { await service.disconnect() }
})

test('disconnect cancels a pending system write without waiting for its timeout', async () => {
  const board = device(), service = new BleService()
  await service.connect('board', () => {}, () => {})
  uni.writeBLECharacteristicValue = o => { board.writes.push(new Uint8Array(o.value)) }
  const sending = service.sendMqttConfiguration(config)
  const rejected = assert.rejects(sending, /断开|结束|取消/)
  await flush(); await service.disconnect(); await rejected
  assert.equal(board.writes.length, 1)
})

test('a verified final reply remains authoritative if the native write callback never arrives', async () => {
  const board = device(), messages = [], service = new BleService()
  await service.connect('board', m => messages.push(m), () => {})
  uni.writeBLECharacteristicValue = () => board.reply({ id: service.currentRequestId, event: 'mqtt_connected' })
  try {
    const id = await service.sendMqttConfiguration(config)
    assert.deepEqual(messages, [{ id, event: 'mqtt_connected' }])
    assert.equal(service.currentRequestId, '')
  } finally { await service.disconnect() }
})

test('a native write failure is not silently resent as another configuration', async () => {
  const board = device(), service = new BleService()
  await service.connect('board', () => {}, () => {})
  uni.writeBLECharacteristicValue = o => { board.writes.push(new Uint8Array(o.value)); o.fail({ errCode: 10008 }) }
  try {
    await assert.rejects(service.sendMqttConfiguration(config), /异常/)
    await assert.rejects(service.sendMqttConfiguration(config), /重新连接|重连/)
    assert.equal(board.writes.length, 1)
  } finally { await service.disconnect() }
})

test('valid replies whose page handler throws never become invalid-JSON errors', async () => {
  const board = device(), messages = [], service = new BleService()
  await service.connect('board', message => {
    messages.push(message)
    if (message.event === 'mqtt_connected') throw new Error('simulated page error')
  }, () => {})
  try {
    const id = await service.sendMqttConfiguration(config)
    board.reply({ id, event: 'mqtt_connected' })
    assert.deepEqual(messages.map(m => m.event), ['mqtt_connected', 'error'])
    assert.equal(messages[1].reason, 'reply_processing_failed')
    assert.ok(messages.every(m => m.reason !== 'invalid_response'))
  } finally { await service.disconnect() }
})

test('an invalid JSON reply does not poison a subsequent request on the same connection', async () => {
  const board = device(), messages = [], service = new BleService()
  await service.connect('board', m => messages.push(m), () => {})
  try {
    await service.sendMqttConfiguration(config)
    board.notify('{invalid-json}\n')
    assert.equal(messages.at(-1).reason, 'invalid_response')
    const id = await service.sendMqttConfiguration(config)
    board.reply({ id, event: 'mqtt_connected' })
    assert.equal(messages.at(-1).event, 'mqtt_connected')
    assert.equal(messages.at(-1).id, id)
  } finally { await service.disconnect() }
})

test('oversized unterminated replies cannot poison a clean retry', async () => {
  const board = device(), messages = [], service = new BleService()
  await service.connect('board', m => messages.push(m), () => {})
  try {
    await service.sendMqttConfiguration(config)
    board.notify('x'.repeat(1025))
    assert.equal(messages.at(-1).reason, 'invalid_response')
    const id = await service.sendMqttConfiguration(config)
    board.reply({ id, event: 'mqtt_connected' })
    assert.equal(messages.at(-1).event, 'mqtt_connected')
  } finally { await service.disconnect() }
})

test('a listener retained by the native layer cannot inject errors into a reconnected session', async () => {
  const board = device(), messages = [], service = new BleService()
  await service.connect('board', m => messages.push(m), () => {})
  const staleListener = board.handlers.message
  await service.disconnect()
  await service.connect('board', m => messages.push(m), () => {})
  try {
    const id = await service.sendMqttConfiguration(config)
    staleListener({ deviceId: 'board', characteristicId: BLE_TX_UUID,
      value: new TextEncoder().encode('{bad}\n').buffer })
    board.reply({ id, event: 'mqtt_connected' })
    assert.deepEqual(messages, [{ id, event: 'mqtt_connected' }])
  } finally { await service.disconnect() }
})

test('line decoder consumes malformed frames and retains later valid frames in the same notification', () => {
  const decoder = new JsonLineDecoder()
  const messages = decoder.push(new TextEncoder().encode('{bad}\n{"id":"1234abcd","event":"mqtt_connected"}\n'))
  assert.equal(messages[0].reason, 'invalid_response')
  assert.deepEqual(messages[1], { id: '1234abcd', event: 'mqtt_connected' })
  assert.deepEqual(decoder.push(new Uint8Array()), [])
})

test('line decoder bounds a missing-newline frame and resynchronizes at the next newline', () => {
  const decoder = new JsonLineDecoder()
  const errors = decoder.push(new TextEncoder().encode('x'.repeat(1025)))
  assert.equal(errors[0].reason, 'invalid_response')
  assert.deepEqual(decoder.push(new TextEncoder().encode('tail\n{"id":"1234abcd","event":"mqtt_connected"}\n')),
    [{ id: '1234abcd', event: 'mqtt_connected' }])
})

let reactive
try { ({ reactive } = createRequire(import.meta.url)(process.env.UNIAPP_VUE_RUNTIME || 'vue')) } catch {}
test('real Vue reactive service receives fragmented replies without reporting a format error',
  { skip: !reactive }, async () => {
    const board = device(), messages = [], service = reactive(new BleService())
    await service.connect('board', m => messages.push(m), () => {})
    try {
      const id = await service.sendMqttConfiguration(config)
      const text = JSON.stringify({ id, event: 'mqtt_connected' }) + '\n'
      for (let offset = 0; offset < text.length; offset += 20) board.notify(text.slice(offset, offset + 20))
      assert.deepEqual(messages, [{ id, event: 'mqtt_connected' }])
    } finally { await service.disconnect() }
  })

test('real Vue page completes Wi-Fi and MQTT result dialogs over plain JSON',
  { skip: !reactive }, async () => {
    const board = device(), page = reactive(pageOptions.data())
    for (const [name, method] of Object.entries(pageOptions.methods)) page[name] = method.bind(page)
    for (const [name, getter] of Object.entries(pageOptions.computed)) {
      Object.defineProperty(page, name, { get: () => getter.call(page) })
    }
    await page.bleService.connect('board', m => page.handleProvisioningMessage(m), () => {})
    page.bleConnected = true; page.state = 'ready'; page.modal = 'wifi-list'
    try {
      page.selectWifi({ SSID: 'Lab', secure: true }); page.selectedWifiPassword = 'dummy-pass'
      await page.submitWifi()
      board.reply({ id: page.bleService.currentRequestId, event: 'wifi_connected', ip: '192.168.5.53' })
      assert.equal(page.wifiResult.success, true); assert.equal(page.errorMessage, '')
      page.confirmWifiResult()
      page.openModal('mqtt'); page.mqttServer = 'broker.example.com'
      await page.submitMqtt()
      board.reply({ id: page.bleService.currentRequestId, event: 'mqtt_connected' })
      assert.equal(page.mqttResult.success, true); assert.equal(page.errorMessage, '')
      page.confirmMqttResult(); assert.equal(page.modal, '')
    } finally {
      page.clearWifiResultTimer(); page.clearMqttResultTimer()
      await page.bleService.disconnect()
    }
  })
