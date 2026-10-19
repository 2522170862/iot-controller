import { BleService } from './ble-service.js'
import { WifiService } from './wifi-service.js'
import { provisioningFailureReason } from '../utils/protocol.js'

const reasonMessages = {
  busy: '控制器正在处理另一条配置，请稍后重试',
  authentication_failed: 'MQTT 认证失败，请检查用户名和密码',
  wifi_offline: '开发板尚未连接 Wi-Fi，请先完成 Wi-Fi 配置',
  subscribe_failed: 'MQTT 订阅请求发送失败，已恢复之前的配置',
  invalid_mqtt: 'MQTT 参数无效，请检查服务器、端口和主题',
  invalid_ssid: 'Wi-Fi 名称长度不正确',
  invalid_password: 'Wi-Fi 密码长度不正确',
  invalid_request: '控制器不支持此请求，请确认已烧录新版固件',
  invalid_json: '配置消息解析失败，请重新连接后重试',
  invalid_response: '控制器回复格式错误，请重新连接后重试',
  message_too_long: '配置消息太长，请缩短参数或更新开发板固件',
  storage_failed: '控制器保存配置失败，已恢复之前的配置',
  device_timeout: '等待控制器回复超时，请检查设备；结果尚未确认',
}

export default {
  data() {
    return {
      state: 'checking', modal: '', devices: [], wifiList: [],
      bleConnected: false, connectedDeviceName: '',
      ssid: '', password: '', showPassword: false, finalIp: '',
      selectedWifi: null, selectedWifiPassword: '', showSelectedWifiPassword: false,
      submittedWifiSsid: '', wifiScanGeneration: 0,
      wifiSubmissionMode: '', wifiResult: null, wifiResultTimer: null, wifiResultGeneration: 0,
      mqttServer: '', mqttPort: '1883', mqttUsername: '', mqttPassword: '',
      showMqttPassword: false, mqttAdvanced: false,
      mqttSubTopic: '', mqttPubTopic: '', mqttQosIndex: 0,
      mqttQosOptions: ['保持当前值', '0', '1'],
      wifiSummary: '', mqttSummary: '', errorMessage: '', resultMessage: '',
      activeOperation: '', scanTimer: null, linkGeneration: 0, requestGeneration: 0,
      bleService: new BleService(), wifiService: new WifiService(),
    }
  },
  computed: {
    busy() { return ['device_connecting', 'disconnecting', 'scanning_wifi', 'sending', 'validating'].includes(this.state) },
    canSubmitWifi() {
      const ssid = this.modal === 'wifi-password' ? this.selectedWifi?.SSID : this.ssid
      return this.bleConnected && !!ssid && !this.busy
    },
    canSubmitMqtt() { return this.bleConnected && this.mqttServer.trim().length > 0 && !this.busy },
    modalTitle() { return { bluetooth: '蓝牙连接', wifi: 'Wi-Fi 配置', 'wifi-list': '选择附近 Wi-Fi', 'wifi-password': '输入 Wi-Fi 密码', 'wifi-result': 'Wi-Fi 配置结果', mqtt: 'MQTT 配置' }[this.modal] || '' },
    statusText() {
      const module = this.activeOperation === 'mqtt' ? 'MQTT 服务器' : 'Wi-Fi'
      if (this.state === 'sending') return `正在通过蓝牙发送${module}配置…`
      if (this.state === 'validating') return `开发板正在连接并验证${module}…`
      return {
        checking: '先连接开发板蓝牙，再配置网络和消息服务',
        searching_device: '正在搜索附近有名称的蓝牙设备…',
        device_connecting: '正在连接开发板并检查配置服务…',
        disconnecting: '正在断开蓝牙…', device_connected: '蓝牙已连接，可以开始配置',
        scanning_wifi: '正在调用手机扫描附近 Wi-Fi…', ready: '填写配置后发送至开发板',
        success: this.resultMessage, error: '操作未完成，请根据提示重试',
      }[this.state] || ''
    },
  },
  onUnload() {
    this.clearWifiResultTimer()
    this.wifiResult = null
    this.wifiScanGeneration++
    clearTimeout(this.scanTimer)
    this.wifiService.close()
    this.bleService.close().catch(() => {})
  },
  methods: {
    openModal(name) {
      if (this.busy) return
      this.clearWifiResultTimer()
      this.wifiResult = null
      this.errorMessage = ''
      this.modal = !this.bleConnected && name !== 'bluetooth' ? 'bluetooth' : name
    },
    closeModal() {
      if (this.modal === 'wifi-result') {
        this.confirmWifiResult()
        return
      }
      if (['wifi-list', 'wifi-password'].includes(this.modal) && !this.activeOperation) {
        this.returnToWifiForm()
        return
      }
      this.modal = ''
      clearTimeout(this.scanTimer)
      this.bleService.stopDiscovery().catch(() => {})
      if (this.state === 'searching_device') this.state = this.bleConnected ? 'device_connected' : 'checking'
    },
    async searchDevices() {
      if (this.busy || this.bleConnected) return
      this.errorMessage = ''
      this.state = 'searching_device'
      clearTimeout(this.scanTimer)
      try {
        await this.bleService.scan(devices => { this.devices = devices })
        this.scanTimer = setTimeout(() => {
          this.bleService.stopDiscovery().catch(() => {})
          if (this.state === 'searching_device') this.state = 'checking'
        }, 15000)
      } catch (error) { this.fail(error) }
    },
    async connectDevice(device) {
      if (this.busy || this.bleConnected) return
      clearTimeout(this.scanTimer)
      this.errorMessage = ''
      this.state = 'device_connecting'
      const generation = ++this.linkGeneration
      try {
        await this.bleService.connect(device.deviceId,
          message => this.handleProvisioningMessage(message),
          () => {
            if (generation !== this.linkGeneration) return
            this.resetLink(); this.fail(new Error('蓝牙已断开，请重新连接；正在进行的配置结果尚未确认'))
          })
        if (generation !== this.linkGeneration) return
        this.bleConnected = true
        this.connectedDeviceName = device.displayName || device.deviceId
        this.wifiSummary = ''; this.mqttSummary = ''; this.finalIp = ''
        this.state = 'device_connected'
        this.modal = ''
      } catch (error) { if (generation === this.linkGeneration) this.fail(error) }
    },
    async disconnectDevice() {
      this.state = 'disconnecting'
      this.errorMessage = ''
      try {
        await this.bleService.disconnect()
        this.resetLink()
        this.state = 'checking'
      } catch (error) { this.resetLink(); this.fail(error) }
    },
    resetLink() {
      this.clearWifiResultTimer()
      this.wifiResult = null; this.wifiSubmissionMode = ''
      this.linkGeneration += 1
      this.requestGeneration += 1
      this.wifiScanGeneration += 1
      this.wifiService.cancelScan?.()
      this.wifiList = []; this.selectedWifi = null; this.selectedWifiPassword = ''
      this.showSelectedWifiPassword = false; this.submittedWifiSsid = ''
      clearTimeout(this.scanTimer)
      this.bleConnected = false; this.connectedDeviceName = ''
      this.activeOperation = ''; this.password = ''; this.mqttPassword = ''
      this.showPassword = false; this.showMqttPassword = false
      this.modal = ''; this.resultMessage = ''
      this.wifiSummary = ''; this.mqttSummary = ''; this.finalIp = ''
    },
    async scanWifi() {
      if (!this.bleConnected || this.busy) return
      const generation = ++this.wifiScanGeneration
      this.errorMessage = ''; this.finalIp = ''; this.resultMessage = ''
      this.selectedWifi = null; this.selectedWifiPassword = ''; this.showSelectedWifiPassword = false
      this.wifiList = []; this.modal = 'wifi-list'; this.state = 'scanning_wifi'
      try {
        const networks = await this.wifiService.scan()
        if (generation !== this.wifiScanGeneration || !this.bleConnected || this.modal !== 'wifi-list') return
        this.wifiList = networks; this.state = 'ready'
      } catch (error) {
        if (generation === this.wifiScanGeneration && this.modal === 'wifi-list') this.fail(error)
      }
    },
    selectWifi(network) {
      if (this.busy || !this.bleConnected || !network?.SSID) return
      this.selectedWifi = { ...network }
      this.selectedWifiPassword = ''; this.showSelectedWifiPassword = false
      this.errorMessage = ''; this.state = 'ready'; this.modal = 'wifi-password'
    },
    backToWifiList() {
      if (this.busy) return
      this.selectedWifi = null; this.selectedWifiPassword = ''; this.showSelectedWifiPassword = false
      this.errorMessage = ''; this.state = 'ready'; this.modal = 'wifi-list'
    },
    returnToWifiForm() {
      if (this.activeOperation && this.busy) return
      this.wifiScanGeneration++
      this.wifiService.cancelScan?.()
      this.selectedWifi = null; this.selectedWifiPassword = ''; this.showSelectedWifiPassword = false
      this.errorMessage = ''; this.state = 'ready'; this.modal = 'wifi'
    },
    mqttConfiguration() {
      const value = { server: this.mqttServer.trim(), port: Number(this.mqttPort),
        username: this.mqttUsername, password: this.mqttPassword }
      if (this.mqttAdvanced) {
        if (this.mqttSubTopic) value.subTopic = this.mqttSubTopic
        if (this.mqttPubTopic) value.pubTopic = this.mqttPubTopic
        if (this.mqttQosIndex > 0) value.subQos = this.mqttQosIndex - 1
      }
      return value
    },
    async submitWifi() {
      if (!this.canSubmitWifi) return
      const selected = this.modal === 'wifi-password'
      this.clearWifiResultTimer()
      this.wifiResult = null; this.wifiSubmissionMode = selected ? 'selected' : 'manual'
      const ssid = selected ? this.selectedWifi.SSID : this.ssid
      const password = selected ? this.selectedWifiPassword : this.password
      this.submittedWifiSsid = ssid
      this.errorMessage = ''; this.finalIp = ''; this.activeOperation = 'wifi'; this.state = 'sending'
      const generation = ++this.requestGeneration
      try {
        await this.bleService.sendWifiCredentials(ssid, password)
        if (generation === this.requestGeneration && this.state === 'sending') this.state = 'validating'
      } catch (error) {
        if (generation === this.requestGeneration && this.activeOperation === 'wifi') this.fail(error)
      }
    },
    async submitMqtt() {
      if (!this.canSubmitMqtt) return
      this.errorMessage = ''; this.activeOperation = 'mqtt'; this.state = 'sending'
      const generation = ++this.requestGeneration
      try {
        await this.bleService.sendMqttConfiguration(this.mqttConfiguration())
        if (generation === this.requestGeneration && this.state === 'sending') this.state = 'validating'
      } catch (error) { if (generation === this.requestGeneration) this.fail(error) }
    },
    handleProvisioningMessage(message) {
      if (message.event === `${this.activeOperation}_connecting`) { this.state = 'validating'; return }
      if (message.event === 'wifi_connected' && this.activeOperation === 'wifi') {
        this.finalIp = message.ip || ''; this.wifiSummary = this.submittedWifiSsid || this.ssid
        this.password = ''; this.showPassword = false
        this.selectedWifiPassword = ''; this.showSelectedWifiPassword = false
        this.resultMessage = 'Wi-Fi 配置成功，开发板下次启动将自动连接此网络'
        this.state = 'success'; this.activeOperation = ''
        if (this.wifiSubmissionMode === 'selected') this.showWifiResult(true, this.resultMessage)
        return
      }
      if (message.event === 'mqtt_connected' && this.activeOperation === 'mqtt') {
        this.mqttSummary = `${this.mqttServer.trim()}:${Number(this.mqttPort)}`
        this.mqttPassword = ''; this.showMqttPassword = false
        this.resultMessage = 'MQTT 配置已验证并保存，下次启动自动使用此服务器'
        this.state = 'success'; this.activeOperation = ''; return
      }
      const reason = provisioningFailureReason(message)
      if (reason !== null) {
        const name = this.activeOperation === 'mqtt' ? 'MQTT 服务器' : 'Wi-Fi'
        this.fail(new Error(reasonMessages[reason] ||
          (reason === 'timeout' ? `${name}连接验证超时，已恢复之前的配置` :
            reason === 'connection_failed' ? `${name}连接失败，请检查地址、网络及凭据` : reason)))
      }
    },
    clearWifiResultTimer() {
      clearTimeout(this.wifiResultTimer)
      this.wifiResultTimer = null
      this.wifiResultGeneration++
    },
    showWifiResult(success, message) {
      this.clearWifiResultTimer()
      this.wifiResult = {
        success, message, ssid: this.submittedWifiSsid,
        ip: success ? this.finalIp : '',
      }
      this.modal = 'wifi-result'
      const generation = this.wifiResultGeneration
      this.wifiResultTimer = setTimeout(() => {
        if (generation === this.wifiResultGeneration && this.modal === 'wifi-result') this.confirmWifiResult()
      }, 3000)
    },
    confirmWifiResult() {
      if (!this.wifiResult) return
      const success = this.wifiResult.success
      this.clearWifiResultTimer()
      this.wifiResult = null
      this.showSelectedWifiPassword = false
      this.modal = !success && this.bleConnected && this.selectedWifi ? 'wifi-password' : ''
    },
    fail(error) {
      const showResult = this.activeOperation === 'wifi' && this.wifiSubmissionMode === 'selected'
      this.errorMessage = error?.message || '操作失败'; this.state = 'error'; this.activeOperation = ''
      if (showResult) this.showWifiResult(false, this.errorMessage)
    },
  },
}
