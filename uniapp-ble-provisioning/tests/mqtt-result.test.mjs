import test from 'node:test'
import assert from 'node:assert/strict'
import { createRequire } from 'node:module'
import { readFileSync } from 'node:fs'
import options from '../services/configuration-page.js'

function page() {
  const state = options.data()
  for (const [name, fn] of Object.entries(options.methods)) state[name] = fn.bind(state)
  for (const [name, fn] of Object.entries(options.computed)) {
    Object.defineProperty(state, name, { get: () => fn.call(state) })
  }
  return state
}

async function mqttAttempt(t) {
  t.mock.timers.enable({ apis: ['setTimeout'] })
  const p = page()
  p.bleConnected = true
  p.modal = 'mqtt'
  p.mqttServer = 'mqtt.example.com'
  p.mqttUsername = 'test-user'
  p.mqttPassword = 'test-password'
  p.bleService = { sendMqttConfiguration: async () => {}, close: async () => {} }
  p.wifiService = { close() {} }
  t.after(() => p.clearMqttResultTimer?.())
  await p.submitMqtt()
  return p
}

test('MQTT BLE delivery and connecting events do not show a successful result', async t => {
  const p = await mqttAttempt(t)
  p.handleProvisioningMessage({ event: 'mqtt_connecting' })
  t.mock.timers.tick(4000)
  assert.equal(p.modal, 'mqtt')
  assert.equal(p.state, 'validating')
  assert.equal(p.mqttResult, null)
})

test('a device MQTT success shows the server and returns home after exactly three seconds', async t => {
  const p = await mqttAttempt(t)
  p.handleProvisioningMessage({ event: 'mqtt_connected' })
  assert.equal(p.modal, 'mqtt-result')
  assert.equal(p.mqttResult.success, true)
  assert.equal(p.mqttResult.server, 'mqtt.example.com:1883')
  assert.equal(p.mqttPassword, '')
  assert.equal(p.state, 'success')
  t.mock.timers.tick(2999)
  assert.equal(p.modal, 'mqtt-result')
  t.mock.timers.tick(1)
  assert.equal(p.modal, '')
  assert.equal(p.mqttResult, null)
})

test('confirming MQTT success immediately returns home without later closing another dialog', async t => {
  const p = await mqttAttempt(t)
  p.handleProvisioningMessage({ event: 'mqtt_connected' })
  assert.equal(p.modal, 'mqtt-result')
  p.confirmMqttResult()
  assert.equal(p.modal, '')
  p.openModal('wifi')
  t.mock.timers.tick(3000)
  assert.equal(p.modal, 'wifi')
})

test('MQTT failure shows its reason then returns to the unchanged configuration form', async t => {
  const p = await mqttAttempt(t)
  p.mqttSummary = 'previous.example.com:1883'
  p.handleProvisioningMessage({ event: 'mqtt_failed', reason: 'authentication_failed' })
  assert.equal(p.modal, 'mqtt-result')
  assert.equal(p.mqttResult.success, false)
  assert.match(p.mqttResult.message, /认证失败/)
  assert.equal(p.mqttSummary, 'previous.example.com:1883')
  t.mock.timers.tick(2999)
  assert.equal(p.modal, 'mqtt-result')
  t.mock.timers.tick(1)
  assert.equal(p.modal, 'mqtt')
  assert.equal(p.mqttServer, 'mqtt.example.com')
  assert.equal(p.mqttUsername, 'test-user')
  assert.equal(p.mqttPassword, 'test-password')
  assert.equal(p.canSubmitMqtt, true)
})

test('confirming a failed MQTT result permits retry without the old countdown closing the new result', async t => {
  const p = await mqttAttempt(t)
  p.handleProvisioningMessage({ event: 'mqtt_failed', reason: 'connection_failed' })
  assert.equal(p.modal, 'mqtt-result')
  t.mock.timers.tick(1000)
  p.confirmMqttResult()
  assert.equal(p.modal, 'mqtt')
  await p.submitMqtt()
  p.handleProvisioningMessage({ event: 'mqtt_connected' })
  t.mock.timers.tick(2000)
  assert.equal(p.modal, 'mqtt-result')
  t.mock.timers.tick(1000)
  assert.equal(p.modal, '')
})

test('an unanswered MQTT request has an unconfirmed failure result, never a success', async t => {
  const p = await mqttAttempt(t)
  p.handleProvisioningMessage({ event: 'error', reason: 'device_timeout' })
  assert.equal(p.modal, 'mqtt-result')
  assert.equal(p.mqttResult.success, false)
  assert.match(p.mqttResult.message, /尚未确认/)
  p.closeModal()
  assert.equal(p.modal, 'mqtt')
})

test('a local MQTT submission error shows the result dialog and preserves inputs for correction', async t => {
  const p = await mqttAttempt(t)
  p.activeOperation = ''
  p.state = 'ready'
  p.bleService.sendMqttConfiguration = async () => { throw new Error('invalid port') }
  await p.submitMqtt()
  assert.equal(p.modal, 'mqtt-result')
  assert.equal(p.mqttResult.success, false)
  assert.equal(p.mqttResult.message, 'invalid port')
  p.closeModal()
  assert.equal(p.modal, 'mqtt')
  assert.equal(p.mqttPassword, 'test-password')
})

test('disconnect clears the MQTT result countdown so it cannot reopen the configuration form', async t => {
  const p = await mqttAttempt(t)
  p.handleProvisioningMessage({ event: 'mqtt_failed', reason: 'connection_failed' })
  assert.equal(p.modal, 'mqtt-result')
  p.resetLink()
  t.mock.timers.tick(3000)
  assert.equal(p.modal, '')
  assert.equal(p.mqttResult, null)
  assert.equal(p.mqttResultTimer, null)
})

test('leaving the page cancels MQTT result navigation', async t => {
  const p = await mqttAttempt(t)
  p.handleProvisioningMessage({ event: 'mqtt_connected' })
  assert.equal(p.modal, 'mqtt-result')
  options.onUnload.call(p)
  assert.equal(p.mqttResult, null)
  assert.equal(p.mqttResultTimer, null)
  p.modal = ''
  t.mock.timers.tick(3000)
  assert.equal(p.modal, '')
})

let vue, renderToString
try {
  const require = createRequire(process.env.UNIAPP_VUE_RUNTIME ?
    `${process.env.UNIAPP_VUE_RUNTIME}/package.json` : import.meta.url)
  vue = require('vue')
  ;({ renderToString } = require('@vue/server-renderer'))
} catch {}

for (const success of [true, false]) {
  test(`the actual MQTT result template renders ${success ? 'success' : 'failure'} and a confirm button`,
    { skip: vue && renderToString ? false : 'Set UNIAPP_VUE_RUNTIME to exercise the Vue template' }, async () => {
      const source = readFileSync(new URL('../pages/index/index.vue', import.meta.url), 'utf8')
      const template = source.match(/<template>([\s\S]*?)<\/template>/)[1]
      const app = vue.createSSRApp({
        ...options,
        data: () => ({ ...options.data(), modal: 'mqtt-result', state: success ? 'success' : 'error',
          mqttResult: { success, server: 'mqtt.example.com:1883', message: success ? 'saved' : 'rejected' } }),
        template,
      })
      app.config.compilerOptions.isCustomElement = tag => ['view', 'text', 'scroll-view', 'switch', 'picker'].includes(tag)
      const html = await renderToString(app)
      assert.ok(html.includes(success ? 'MQTT 连接成功' : 'MQTT 配置未完成'))
      assert.ok(html.includes('mqtt.example.com:1883'))
      assert.ok(html.includes(success ? 'saved' : 'rejected'))
      assert.match(html, /<button[^>]*>确认<\/button>/)
      assert.ok(!html.includes('保存并连接'))
    })
}
