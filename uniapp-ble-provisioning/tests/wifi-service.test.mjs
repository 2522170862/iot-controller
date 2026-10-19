import test from 'node:test'
import assert from 'node:assert/strict'
import { createRequire } from 'node:module'
import { WifiService } from '../services/wifi-service.js'

// HBuilder supplies Vue in IDE projects. A CLI project can resolve its own
// Vue, or set UNIAPP_VUE_RUNTIME to HBuilder's node_modules/vue directory.
let reactive
try { ({ reactive } = createRequire(import.meta.url)(process.env.UNIAPP_VUE_RUNTIME || 'vue')) } catch (_) {}

// Only the Uni/UTS boundary is replaced. uni-wifi 1.0.9 clears its list
// callback after each broadcast, so the service must register on every scan.
function phone(t, overrides = {}) {
  const state = { platform: 'app', starts: 0, requests: 0, stops: 0, authorizations: 0,
    services: [], listener: null, autoReply: true, initError: null, scanError: null, ...overrides }
  globalThis.plus = { android: new Proxy({}, { get() { throw new Error('must not use Native.js') } }) }
  state.emit = (list = [
    { SSID: 'Lab', BSSID: 'weak', frequency: 2412, signalStrength: -80, secure: true },
    { SSID: 'Lab', BSSID: 'strong', frequency: 2437, signalStrength: -40, secure: true },
    { SSID: 'Guest', frequency: 2462, signalStrength: -65, secure: false },
    { SSID: 'Fast', frequency: 5180, signalStrength: -30, secure: true },
  ]) => {
    const listener = state.listener
    state.listener = null
    listener?.({ wifiList: list })
  }
  globalThis.uni = {
    getSystemInfoSync: () => ({ uniPlatform: state.platform, platform: 'android', system: 'Android 8.0.0' }),
    authorize: args => { state.authorizations++; args.success({}) },
    startWifi: args => {
      state.starts++
      if (state.holdInit) state.initReply = args
      else if (state.initError) args.fail(state.initError)
      else args.success({ errMsg: 'startWifi:ok' })
    },
    onGetWifiList: fn => { state.listener = fn },
    offGetWifiList: (...args) => { state.offArgs = args; state.listener = null },
    getWifiList: args => {
      state.requests++
      if (state.scanError) args.fail(state.scanError)
      else if (state.autoReply) { state.emit(); args.success({ errMsg: 'getWifiList:ok' }) }
    },
    stopWifi: args => { state.stops++; args.success({ errMsg: 'stopWifi:ok' }) },
  }
  state.service = options => {
    const service = new WifiService(options)
    state.services.push(service)
    return service
  }
  t.after(async () => {
    for (const service of state.services) await service.close()
    delete globalThis.uni; delete globalThis.plus
  })
  return state
}

test('Android 8 uses the working official Wi-Fi APIs without reading native SDK fields', async t => {
  const state = phone(t)
  const service = state.service()
  const networks = await service.scan()
  assert.equal(state.starts, 1)
  assert.equal(state.requests, 1)
  assert.equal(state.authorizations, 0)
  assert.deepEqual(networks.map(n => [n.SSID, n.BSSID, n.secure]), [
    ['Lab', 'strong', true], ['Guest', '', false],
  ])
})

test('repeated Android scans re-register the one-shot uni-wifi callback', async t => {
  const state = phone(t)
  const service = state.service({ scanTimeoutMs: 30 })
  assert.equal((await service.scan()).length, 2)
  assert.equal((await service.scan()).length, 2)
  assert.equal(state.requests, 2)
})

test('WeChat retains phone authorization and its built-in Wi-Fi APIs', async t => {
  const state = phone(t, { platform: 'mp-weixin' })
  const service = state.service()
  assert.equal((await service.scan())[0].SSID, 'Lab')
  assert.equal(state.authorizations, 1)
})

test('missing plugin API gives actionable installation and base instructions', async t => {
  const state = phone(t)
  delete uni.startWifi
  await assert.rejects(state.service().scan(), /uni-wifi.*基座|基座.*uni-wifi/)
})

test('disabled Wi-Fi reports a specific error code from the official plugin', async t => {
  const state = phone(t, { initError: { errCode: 12005, errMsg: 'startWifi:fail' } })
  await assert.rejects(state.service().scan(), /Wi-Fi.*开关|打开.*Wi-Fi/)
  assert.equal(state.requests, 0)
})

test('a scan failure keeps the plugin error detail and releases the scan lock', async t => {
  const state = phone(t, { scanError: { errCode: 12001, errMsg: 'location permission denied' } })
  const service = state.service()
  await assert.rejects(service.scan(), /12001.*location permission denied/)
  state.scanError = null
  assert.equal((await service.scan()).length, 2)
})

test('no result times out and removes the stale list listener', async t => {
  const state = phone(t, { autoReply: false })
  const service = state.service({ scanTimeoutMs: 25 })
  await assert.rejects(service.scan(), /超时/)
  assert.equal(state.listener, null)
})

test('closing cancels a scan and unregisters without passing a mismatched UTS callback type', async t => {
  const state = phone(t, { autoReply: false })
  const service = state.service()
  const scan = service.scan()
  const rejection = assert.rejects(scan, /取消/)
  await new Promise(resolve => setTimeout(resolve, 0))
  await service.close()
  await rejection
  assert.equal(state.stops, 1)
  assert.equal(state.listener, null)
  assert.deepEqual(state.offArgs, [])
})

test('simultaneous requests cannot start two scans', async t => {
  const state = phone(t, { autoReply: false })
  const service = state.service({ scanTimeoutMs: 25 })
  const results = await Promise.allSettled([service.scan(), service.scan()])
  assert.equal(state.requests, 1)
  assert.equal(results.filter(r => r.status === 'rejected' && /正在扫描/.test(r.reason.message)).length, 1)
})

test('an empty Wi-Fi list resolves immediately rather than waiting for a timeout', async t => {
  const state = phone(t, { autoReply: false })
  const service = state.service()
  const scan = service.scan()
  const rejectionHandled = scan.catch(() => {})
  await new Promise(resolve => setTimeout(resolve, 0))
  state.emit([])
  assert.deepEqual(await scan, [])
  await rejectionHandled
})

test('Vue reactive service resolves list callbacks instead of rejecting proxy identity', { skip: !reactive }, async t => {
  const state = phone(t)
  const service = reactive(state.service({ scanTimeoutMs: 15 }))
  const result = await Promise.race([
    service.scan(), new Promise(resolve => setTimeout(() => resolve('hung'), 60)),
  ])
  assert.notEqual(result, 'hung', 'Vue proxy caused the list callback and timeout to be ignored')
  assert.equal(result[0].SSID, 'Lab')
})

test('Vue reactive service times out when no scan broadcast arrives', { skip: !reactive }, async t => {
  const state = phone(t, { autoReply: false })
  const service = reactive(state.service({ scanTimeoutMs: 15 }))
  const result = await Promise.race([
    service.scan().catch(error => error.message),
    new Promise(resolve => setTimeout(() => resolve('hung'), 60)),
  ])
  assert.match(result, /超时/)
})

test('startWifi without any native callback times out before scanning', async t => {
  const state = phone(t, { holdInit: true })
  const service = state.service({ scanTimeoutMs: 15 })
  const result = await Promise.race([
    service.scan().catch(error => error.message),
    new Promise(resolve => setTimeout(() => resolve('hung'), 60)),
  ])
  assert.match(result, /startWifi.*超时/)
  assert.equal(state.requests, 0)
  // A late native success must not trigger a new scan after timeout.
  state.initReply.success({})
  await new Promise(resolve => setTimeout(resolve, 0))
  assert.equal(state.requests, 0)
})

test('cancelling a selection scan releases the wait without stopping the Wi-Fi module', async t => {
  const state = phone(t, { autoReply: false })
  const service = state.service()
  const scan = service.scan()
  const rejected = assert.rejects(scan, /取消/)
  await new Promise(resolve => setTimeout(resolve, 0))
  service.cancelScan()
  await rejected
  assert.equal(state.stops, 0)
  state.autoReply = true
  assert.equal((await service.scan())[0].SSID, 'Lab')
})
