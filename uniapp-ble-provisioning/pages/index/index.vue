<template>
  <view class="page">
    <view class="hero">
      <text class="title">控制器蓝牙配网</text>
      <text class="subtitle">用手机把 2.4GHz Wi‑Fi 安全发送给 ESP32-S3</text>
    </view>

    <view class="card prerequisites">
      <text class="card-title">开始前请确认</text>
      <text>1. 控制器已上电，GPIO44 蓝牙灯可用</text>
      <text>2. 手机蓝牙、Wi‑Fi 和定位服务已打开</text>
      <text>3. 请选择 2.4GHz Wi‑Fi，当前版本不支持 iOS</text>
    </view>

    <view class="card">
      <view class="section-heading">
        <text class="card-title">1. 连接控制器</text>
        <button class="minor-button" :disabled="busy" @click="searchDevices">
          {{ state === 'searching_device' ? '搜索中…' : '搜索蓝牙' }}
        </button>
      </view>
      <view v-if="devices.length" class="list">
        <button
          v-for="device in devices"
          :key="device.deviceId"
          class="list-item"
          :disabled="busy"
          @click="connectDevice(device)"
        >
          <view>
            <text class="item-title">{{ device.name || device.localName }}</text>
            <text class="item-caption">{{ device.deviceId }}</text>
          </view>
          <text>{{ device.RSSI ?? '--' }} dBm</text>
        </button>
      </view>
      <text v-else class="empty">尚未发现控制器</text>
      <text v-if="connectedDeviceName" class="success-text">
        已连接：{{ connectedDeviceName }}
      </text>
    </view>

    <view class="card" :class="{ disabled: !bleConnected }">
      <view class="section-heading">
        <text class="card-title">2. 选择附近 Wi‑Fi</text>
        <button
          class="minor-button"
          :disabled="!bleConnected || busy"
          @click="scanWifi"
        >
          {{ state === 'scanning_wifi' ? '扫描中…' : '扫描 Wi‑Fi' }}
        </button>
      </view>
      <view v-if="wifiList.length" class="list wifi-list">
        <button
          v-for="network in wifiList"
          :key="network.SSID"
          class="list-item"
          :class="{ selected: ssid === network.SSID }"
          :disabled="busy"
          @click="selectWifi(network)"
        >
          <view>
            <text class="item-title">{{ network.SSID }}</text>
            <text class="item-caption">
              {{ network.secure ? '已加密' : '开放网络' }}
            </text>
          </view>
          <text>{{ network.signalStrength }} dBm</text>
        </button>
      </view>
      <label class="field">
        <text>Wi‑Fi 名称（可手动输入）</text>
        <input
          v-model.trim="ssid"
          :disabled="busy"
          maxlength="32"
          placeholder="请输入 2.4GHz Wi‑Fi 名称"
        />
      </label>
      <label class="field">
        <text>Wi‑Fi 密码</text>
        <input
          v-model="password"
          :disabled="busy"
          :password="!showPassword"
          maxlength="63"
          placeholder="开放网络可留空"
        />
      </label>
      <label class="password-toggle">
        <checkbox :checked="showPassword" @click="showPassword = !showPassword" />
        <text>显示密码</text>
      </label>
    </view>

    <view class="card action-card">
      <button
        class="primary-button"
        :disabled="!canSubmit"
        @click="submitCredentials"
      >
        {{ submitLabel }}
      </button>
      <view class="status" :class="state">
        <text>{{ statusText }}</text>
        <text v-if="finalIp" class="ip">设备 IP：{{ finalIp }}</text>
        <text v-if="errorMessage" class="error-text">{{ errorMessage }}</text>
      </view>
      <button v-if="state === 'error'" class="retry-button" @click="retry">
        重试
      </button>
    </view>
  </view>
</template>

<script>
import { BleService } from '../../services/ble-service.js'
import { WifiService } from '../../services/wifi-service.js'

const reasonMessages = {
  busy: '控制器正在处理另一条配网请求',
  connection_failed: 'Wi‑Fi 连接失败，请检查名称和密码',
  timeout: 'Wi‑Fi 验证超时，请靠近路由器后重试',
  storage_failed: 'Wi‑Fi 已连接，但控制器保存凭据失败',
  invalid_response: '控制器回复格式错误',
}

export default {
  data() {
    return {
      state: 'checking',
      devices: [],
      wifiList: [],
      bleConnected: false,
      connectedDeviceName: '',
      ssid: '',
      password: '',
      showPassword: false,
      finalIp: '',
      errorMessage: '',
      bleService: new BleService(),
      wifiService: new WifiService(),
    }
  },
  computed: {
    busy() {
      return ['sending', 'validating'].includes(this.state)
    },
    canSubmit() {
      return this.bleConnected && this.ssid.length > 0 && !this.busy
    },
    submitLabel() {
      if (this.state === 'sending') return '正在发送…'
      if (this.state === 'validating') return '正在验证 Wi‑Fi…'
      if (this.state === 'success') return '配网成功'
      return '发送并连接 Wi‑Fi'
    },
    statusText() {
      const messages = {
        checking: '请先搜索并连接控制器',
        searching_device: '正在搜索附近控制器…',
        device_connected: '蓝牙已连接，可以扫描手机附近的 Wi‑Fi',
        scanning_wifi: '正在调用手机扫描附近 Wi‑Fi…',
        ready: '请选择网络并填写密码',
        sending: '正在通过蓝牙发送配网信息…',
        validating: '控制器正在连接并验证新 Wi‑Fi…',
        success: '配网成功；下次启动将优先连接此 Wi‑Fi',
        error: '操作未完成，请查看原因后重试',
      }
      return messages[this.state]
    },
  },
  onUnload() {
    this.wifiService.close()
    this.bleService.close()
  },
  methods: {
    async searchDevices() {
      this.errorMessage = ''
      this.state = 'searching_device'
      try {
        await this.bleService.scan((devices) => {
          this.devices = devices
        })
      } catch (error) {
        this.fail(error)
      }
    },
    async connectDevice(device) {
      this.errorMessage = ''
      try {
        await this.bleService.connect(
          device.deviceId,
          (message) => this.handleProvisioningMessage(message),
          () => {
            this.bleConnected = false
            this.connectedDeviceName = ''
            this.fail(new Error('控制器蓝牙连接已断开'))
          },
        )
        this.bleConnected = true
        this.connectedDeviceName = device.name || device.localName || device.deviceId
        this.state = 'device_connected'
      } catch (error) {
        this.fail(error)
      }
    },
    async scanWifi() {
      this.errorMessage = ''
      this.state = 'scanning_wifi'
      try {
        this.wifiList = await this.wifiService.scan()
        this.state = 'ready'
      } catch (error) {
        this.fail(error)
      }
    },
    selectWifi(network) {
      this.ssid = network.SSID
      if (!network.secure) {
        this.password = ''
      }
      this.state = 'ready'
    },
    async submitCredentials() {
      if (!this.canSubmit) return
      this.errorMessage = ''
      this.finalIp = ''
      this.state = 'sending'
      try {
        await this.bleService.sendWifiCredentials(this.ssid, this.password)
        this.state = 'validating'
      } catch (error) {
        this.fail(error)
      }
    },
    handleProvisioningMessage(message) {
      if (message.event === 'wifi_connecting') {
        this.state = 'validating'
        return
      }
      if (message.event === 'wifi_connected') {
        this.finalIp = message.ip || ''
        this.password = ''
        this.showPassword = false
        this.state = 'success'
        return
      }
      if (message.event === 'wifi_failed' || message.event === 'protocol_error') {
        this.fail(new Error(reasonMessages[message.reason] || message.reason || '配网失败'))
      }
    },
    retry() {
      this.errorMessage = ''
      this.finalIp = ''
      this.state = this.bleConnected ? 'ready' : 'checking'
    },
    fail(error) {
      this.errorMessage = error?.message || '操作失败'
      this.state = 'error'
    },
  },
}
</script>

<style lang="scss">
.page {
  min-height: 100vh;
  box-sizing: border-box;
  padding: 32rpx;
  background: #f4f7fb;
  color: #182230;
}

.hero {
  padding: 24rpx 8rpx 32rpx;
}

.title,
.subtitle,
.card-title,
.item-title,
.item-caption,
.status,
.ip {
  display: block;
}

.title {
  font-size: 48rpx;
  font-weight: 700;
}

.subtitle {
  margin-top: 12rpx;
  color: #667085;
}

.card {
  margin-bottom: 24rpx;
  padding: 28rpx;
  border-radius: 24rpx;
  background: #ffffff;
  box-shadow: 0 8rpx 28rpx rgba(16, 24, 40, 0.07);
}

.card-title {
  margin-bottom: 20rpx;
  font-size: 32rpx;
  font-weight: 650;
}

.prerequisites text:not(.card-title) {
  display: block;
  margin-top: 10rpx;
  color: #475467;
  line-height: 1.5;
}

.section-heading,
.list-item {
  display: flex;
  align-items: center;
  justify-content: space-between;
}

.minor-button,
.retry-button {
  margin: 0;
  padding: 0 24rpx;
  font-size: 26rpx;
  color: #175cd3;
  background: #eff8ff;
}

.list {
  margin-bottom: 20rpx;
}

.list-item {
  width: 100%;
  margin: 12rpx 0;
  padding: 20rpx;
  text-align: left;
  border: 2rpx solid #e4e7ec;
  border-radius: 16rpx;
  background: #ffffff;
}

.list-item::after {
  border: 0;
}

.list-item.selected {
  border-color: #2e90fa;
  background: #eff8ff;
}

.item-title {
  font-size: 28rpx;
  font-weight: 600;
}

.item-caption,
.empty {
  margin-top: 6rpx;
  font-size: 23rpx;
  color: #98a2b3;
}

.success-text {
  display: block;
  margin-top: 16rpx;
  color: #067647;
}

.field {
  display: block;
  margin-top: 24rpx;
  color: #344054;
}

.field input {
  margin-top: 12rpx;
  padding: 20rpx;
  border: 2rpx solid #d0d5dd;
  border-radius: 14rpx;
  background: #ffffff;
}

.password-toggle {
  display: flex;
  align-items: center;
  margin-top: 16rpx;
  color: #475467;
}

.primary-button {
  color: #ffffff;
  background: #1570ef;
}

.primary-button[disabled] {
  color: #98a2b3;
  background: #eaecf0;
}

.status {
  margin-top: 24rpx;
  line-height: 1.6;
  color: #475467;
}

.status.success {
  color: #067647;
}

.status.error,
.error-text {
  color: #b42318;
}

.ip {
  margin-top: 8rpx;
  font-weight: 650;
}

.retry-button {
  margin-top: 20rpx;
}

.disabled {
  opacity: 0.65;
}
</style>
