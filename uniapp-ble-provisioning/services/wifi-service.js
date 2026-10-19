function callUni(method, options = {}, timeoutMs = 0) {
  return new Promise((resolve, reject) => {
    // Keep static uni.xxx references: HBuilderX injects uni-wifi UTS exports
    // at compile time and cannot discover dynamic uni[method] accesses.
    const api = {
      authorize: uni.authorize,
      startWifi: uni.startWifi,
      getWifiList: uni.getWifiList,
      stopWifi: uni.stopWifi,
    }[method]
    if (typeof api !== 'function') {
      reject(new Error(`${method} 不可用，请确认工程包含 uni_modules/uni-wifi，并使用包含 uni-wifi 的 Android 基座或微信真机`))
      return
    }
    let settled = false
    let timer = null
    const finish = (error, result) => {
      if (settled) return
      settled = true
      clearTimeout(timer)
      if (error) { console.warn(`[Wi-Fi] ${method}: ${error.message}`); reject(error) }
      else { console.info(`[Wi-Fi] ${method}: success`); resolve(result) }
    }
    if (timeoutMs > 0) timer = setTimeout(() => {
      finish(new Error(`${method} 回调超时，请检查 uni-wifi 插件是否已编译进运行基座，以及定位授权弹窗；可手动输入 Wi-Fi 名称`))
    }, timeoutMs)
    console.info(`[Wi-Fi] ${method}: begin`)
    try { api({
      ...options,
      success: result => finish(null, result),
      fail: (error) => {
        const code = error?.errCode
        const detail = Number(code) === 12005 ? '请打开手机 Wi-Fi 开关后重试' : (error?.errMsg || 'Wi-Fi 操作失败，请检查定位权限与系统定位开关')
        finish(new Error(`${method} 失败${code === undefined ? '' : `（${code}）`}：${detail}`))
      },
    }) } catch (error) { finish(error) }
  })
}

export function normalizeWifiList(rawList) {
  const strongestBySsid = new Map()
  for (const raw of rawList || []) {
    const SSID = String(raw?.SSID || '').trim()
    if (!SSID) {
      continue
    }
    const frequency = Number.isFinite(Number(raw.frequency))
      ? Number(raw.frequency)
      : undefined
    if (frequency !== undefined && frequency >= 4900) {
      continue
    }
    const signalStrength = Number.isFinite(Number(raw.signalStrength))
      ? Number(raw.signalStrength)
      : -100
    const network = {
      SSID,
      BSSID: String(raw?.BSSID || ''),
      secure: Boolean(raw?.secure),
      signalStrength,
      frequency,
    }
    const previous = strongestBySsid.get(SSID)
    if (!previous || network.signalStrength > previous.signalStrength) {
      strongestBySsid.set(SSID, network)
    }
  }
  return Array.from(strongestBySsid.values()).sort(
    (left, right) => right.signalStrength - left.signalStrength,
  )
}

function runtimePlatform() {
  const info = uni.getSystemInfoSync?.() || {}
  return {
    uniPlatform: info.uniPlatform || '',
    osName: String(info.osName || info.platform || '').toLowerCase(),
  }
}

export class WifiService {
  constructor(options = {}) {
    this.initialized = false
    this.wifiListHandler = null
    this.pendingScan = null
    this.scanning = false
    this.generation = 0
    this.scanTimeoutMs = options.scanTimeoutMs ?? 10000
    this.scanSequence = 0
  }

  async initialize() {
    const platform = runtimePlatform()
    const generation = this.generation
    if (platform.uniPlatform === 'mp-weixin') {
      await callUni('authorize', { scope: 'scope.userLocation' }, this.scanTimeoutMs)
    } else if (
      ['app', 'app-plus'].includes(platform.uniPlatform) &&
      platform.osName === 'android'
    ) {
      // Same path as the working reference project. The bundled official
      // uni-wifi plugin handles Android location permission and native calls.
    } else if (platform.osName === 'ios') {
      throw new Error('当前版本暂不支持 iOS 配网')
    } else {
      throw new Error('请使用微信真机或 Android App 扫描 Wi-Fi')
    }

    if (generation !== this.generation) throw new Error('Wi-Fi 扫描已取消')
    // Call on each attempt: the plugin is idempotent and rechecks permission
    // and the Wi-Fi switch, including after a failed scan or an OS change.
    try {
      await callUni('startWifi', {}, this.scanTimeoutMs)
    } catch (error) {
      if (!/already/i.test(error.message)) throw error
    }
    if (generation !== this.generation) throw new Error('Wi-Fi 扫描已取消')
    this.initialized = true
  }

  async scan() {
    if (this.scanning) {
      throw new Error('正在扫描附近 Wi-Fi，请稍候')
    }
    this.scanning = true
    const generation = this.generation
    try {
      await this.initialize()
      if (generation !== this.generation) throw new Error('Wi-Fi 扫描已取消')
      // Use the same promise for API failure, cancellation and scan timeout.
      return await new Promise((resolve, reject) => {
        const pending = { id: ++this.scanSequence, resolve, reject, timer: setTimeout(() => {
          if (this.pendingScan?.id !== pending.id) return
          this.pendingScan = null
          reject(new Error('Wi-Fi 扫描超时，请确认已开启定位和 Wi-Fi'))
        }, this.scanTimeoutMs) }
        this.pendingScan = pending
        this.wifiListHandler = result => {
          if (this.pendingScan?.id !== pending.id || generation !== this.generation) return
          this.pendingScan = null
          clearTimeout(pending.timer)
          console.info(`[Wi-Fi] onGetWifiList: ${result?.wifiList?.length || 0} networks`)
          resolve(normalizeWifiList(result?.wifiList || []))
        }
        // uni-wifi 1.0.9 uses a one-shot callback on Android. Register for
        // every scan, not just initialization, and before calling getWifiList.
        if (typeof uni.onGetWifiList !== 'function') {
          clearTimeout(pending.timer)
          this.pendingScan = null
          reject(new Error('onGetWifiList 不可用，请检查 uni-wifi 插件与运行基座'))
          return
        }
        uni.onGetWifiList(this.wifiListHandler)
        callUni('getWifiList').catch(error => {
          if (this.pendingScan?.id !== pending.id) return
          clearTimeout(pending.timer)
          this.pendingScan = null
          reject(error)
        })
      })
    } finally {
      if (generation === this.generation) {
        this.removeWifiListListener()
        this.scanning = false
      }
    }
  }

  removeWifiListListener() {
    if (!this.wifiListHandler) return
    this.wifiListHandler = null
    if (typeof uni.offGetWifiList === 'function') {
      // Do not pass the list callback to the plugin's zero-argument callback
      // type. This service exclusively owns Wi-Fi list subscription here.
      try { uni.offGetWifiList() } catch (_) { /* Cleanup must not mask results. */ }
    }
  }

  cancelScan() {
    this.generation++
    this.scanning = false
    if (this.pendingScan) {
      clearTimeout(this.pendingScan.timer)
      this.pendingScan.reject(new Error('Wi-Fi 扫描已取消'))
      this.pendingScan = null
    }
    this.removeWifiListListener()
  }

  async close() {
    this.cancelScan()
    if (this.initialized && typeof uni.stopWifi === 'function') {
      try {
        await callUni('stopWifi', {}, 2000)
      } catch (_) {
        // Page cleanup should not block navigation.
      }
    }
    this.initialized = false
  }
}
