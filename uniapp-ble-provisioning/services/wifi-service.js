function callUni(method, options = {}) {
  return new Promise((resolve, reject) => {
    const api = uni[method]
    if (typeof api !== 'function') {
      reject(new Error(`当前平台不支持 ${method}，请确认已安装 uni-WiFi`))
      return
    }
    api({
      ...options,
      success: resolve,
      fail: (error) => reject(new Error(error?.errMsg || 'Wi-Fi 操作失败')),
    })
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

function requestAndroidPermissions() {
  return new Promise((resolve, reject) => {
    if (typeof plus === 'undefined' || !plus.android?.requestPermissions) {
      reject(new Error('Android 权限接口不可用，请使用 App 端运行'))
      return
    }
    plus.android.requestPermissions(
      [
        'android.permission.ACCESS_FINE_LOCATION',
        'android.permission.BLUETOOTH_SCAN',
        'android.permission.BLUETOOTH_CONNECT',
      ],
      (result) => {
        if ((result.deniedAlways || []).length || (result.deniedPresent || []).length) {
          reject(new Error('需要定位和蓝牙权限才能扫描附近 Wi-Fi'))
          return
        }
        resolve()
      },
      () => reject(new Error('请求 Android 权限失败')),
    )
  })
}

export class WifiService {
  constructor() {
    this.initialized = false
    this.wifiListHandler = null
    this.pendingScan = null
  }

  async initialize() {
    if (this.initialized) {
      return
    }
    const platform = runtimePlatform()
    if (platform.uniPlatform === 'mp-weixin') {
      await callUni('authorize', { scope: 'scope.userLocation' })
    } else if (
      platform.uniPlatform === 'app' &&
      platform.osName === 'android'
    ) {
      await requestAndroidPermissions()
    } else if (platform.osName === 'ios') {
      throw new Error('当前版本暂不支持 iOS 配网')
    }

    await callUni('startWifi')
    this.wifiListHandler = (result) => {
      if (!this.pendingScan) {
        return
      }
      const pending = this.pendingScan
      this.pendingScan = null
      clearTimeout(pending.timer)
      pending.resolve(normalizeWifiList(result?.wifiList || []))
    }
    uni.onGetWifiList(this.wifiListHandler)
    this.initialized = true
  }

  async scan() {
    await this.initialize()
    if (this.pendingScan) {
      throw new Error('正在扫描附近 Wi-Fi，请稍候')
    }

    const resultPromise = new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        this.pendingScan = null
        reject(new Error('Wi-Fi 扫描超时，请确认已开启定位和 Wi-Fi'))
      }, 10000)
      this.pendingScan = { resolve, reject, timer }
    })
    try {
      await callUni('getWifiList')
    } catch (error) {
      clearTimeout(this.pendingScan?.timer)
      this.pendingScan = null
      throw error
    }
    return resultPromise
  }

  async close() {
    if (this.pendingScan) {
      clearTimeout(this.pendingScan.timer)
      this.pendingScan.reject(new Error('Wi-Fi 扫描已取消'))
      this.pendingScan = null
    }
    if (this.wifiListHandler && typeof uni.offGetWifiList === 'function') {
      uni.offGetWifiList(this.wifiListHandler)
    }
    this.wifiListHandler = null
    if (this.initialized && typeof uni.stopWifi === 'function') {
      try {
        await callUni('stopWifi')
      } catch (_) {
        // Page cleanup should not block navigation.
      }
    }
    this.initialized = false
  }
}
