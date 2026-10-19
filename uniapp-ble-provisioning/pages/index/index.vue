<template>
  <view class="page">
    <view class="hero">
      <text class="eyebrow">智联终端</text>
      <text class="title">设备配置</text>
      <text class="subtitle">连接蓝牙，轻松设置网络与消息服务</text>
    </view>

    <view class="connection-strip">
      <view class="connection-copy">
        <view class="dot" :class="{ online: bleConnected }"></view>
        <text>{{ bleConnected ? connectedDeviceName : '尚未连接开发板' }}</text>
      </view>
      <button v-if="bleConnected" class="disconnect-link" :disabled="state === 'disconnecting'" @click="disconnectDevice">断开连接</button>
      <text v-else class="strip-caption">请靠近设备</text>
    </view>

    <button class="config-card" :disabled="busy" @click="openModal('bluetooth')">
      <view class="module-icon bluetooth-icon"><text>BT</text></view>
      <view class="module-copy">
        <text class="module-title">蓝牙连接</text>
        <text class="module-description">{{ bleConnected ? '查看连接设备或断开连接' : '搜索并连接附近的开发板' }}</text>
      </view>
      <text class="badge" :class="{ connected: bleConnected }">{{ bleConnected ? '已连接' : '未连接' }}</text>
      <text class="chevron">›</text>
    </button>

    <button class="config-card" :disabled="busy" @click="openModal('wifi')">
      <view class="module-icon wifi-icon"><text>Wi-Fi</text></view>
      <view class="module-copy">
        <text class="module-title">Wi-Fi 配置</text>
        <text class="module-description">{{ wifiSummary || '选择附近网络，配置设备联网' }}</text>
      </view>
      <text class="chevron">›</text>
    </button>

    <button class="config-card" :disabled="busy" @click="openModal('mqtt')">
      <view class="module-icon mqtt-icon"><text>MQ</text></view>
      <view class="module-copy">
        <text class="module-title">MQTT 配置</text>
        <text class="module-description">{{ mqttSummary || '更换消息服务器与连接参数' }}</text>
      </view>
      <text class="chevron">›</text>
    </button>

    <view class="status-card" :class="{ success: state === 'success', error: state === 'error' }">
      <text class="status-heading">{{ busy ? '正在处理' : state === 'success' ? '配置已完成' : state === 'error' ? '需要处理' : '连接提示' }}</text>
      <text class="status-copy">{{ statusText }}</text>
      <text v-if="errorMessage" class="error-text">{{ errorMessage }}</text>
      <text v-if="finalIp" class="ip">设备 IP：{{ finalIp }}</text>
    </view>

    <view class="page-note">
      <text>微信小程序 · Android App</text>
      <text>Wi-Fi 扫描需打开手机 Wi-Fi、蓝牙与定位服务</text>
    </view>

    <view v-if="modal" class="modal-mask" @click="closeModal">
      <view class="modal-panel" :class="{ 'password-panel': modal === 'wifi-password', 'result-panel': modal === 'wifi-result' }" @click.stop>
        <view class="modal-header">
          <view>
            <text class="modal-kicker">设备连接设置</text>
            <text class="modal-title">{{ modalTitle }}</text>
          </view>
          <button class="close-button" @click="closeModal">×</button>
        </view>
        <scroll-view scroll-y class="modal-body">
          <view v-if="modal === 'bluetooth'">
            <view v-if="bleConnected" class="connected-panel">
              <text class="item-title">{{ connectedDeviceName }}</text>
              <text class="hint">蓝牙已连接，可以配置 Wi-Fi 与 MQTT</text>
              <button class="danger-button" :disabled="state === 'disconnecting'" @click="disconnectDevice">断开蓝牙连接</button>
            </view>
            <view v-else>
              <text class="hint">开启手机蓝牙，点击搜索后选择你的开发板。系统授权提示请允许。</text>
              <button class="secondary-button scan-button" :disabled="busy" @click="searchDevices">{{ state === 'searching_device' ? '重新搜索' : '搜索附近设备' }}</button>
              <view v-if="devices.length" class="device-list">
                <button v-for="device in devices" :key="device.deviceId" class="list-item" :disabled="busy" @click="connectDevice(device)">
                  <view class="list-copy">
                    <text class="item-title">{{ device.displayName }}</text>
                    <text class="item-caption">{{ device.deviceId }}</text>
                  </view>
                  <text class="signal">{{ device.RSSI ?? '--' }} dBm</text>
                </button>
              </view>
              <view v-else class="empty-state"><text>{{ state === 'searching_device' ? '正在寻找附近设备…' : '暂未发现设备' }}</text><text class="hint">仅显示有名称的 BLE 设备</text></view>
            </view>
          </view>

          <view v-if="modal === 'wifi'">
            <text class="hint">请选择 2.4GHz Wi-Fi。连接成功后，开发板会保存此网络。</text>
            <view class="section-heading">
              <text class="section-label">附近网络</text>
              <button class="small-button" :disabled="!bleConnected || busy" @click="scanWifi">手机扫描</button>
            </view>
            <text class="hint scan-description">在新窗口中选择网络，再输入该网络的密码。</text>
            <text class="section-label manual-heading">手动填写</text>
            <label class="field"><text>Wi-Fi 名称</text><input v-model.trim="ssid" :disabled="busy" maxlength="32" placeholder="手动输入 Wi-Fi 名称" /></label>
            <label class="field"><text>Wi-Fi 密码</text><input v-model="password" :disabled="busy" :password="!showPassword" maxlength="63" placeholder="开放网络可留空" /></label>
            <view class="toggle-row"><text>显示密码</text><switch :checked="showPassword" :disabled="busy" color="#238c79" @change="showPassword = $event.detail.value" /></view>
          </view>

          <view v-if="modal === 'wifi-list'" class="network-selection">
            <text class="hint">选择一个 2.4GHz 网络，下一步填写密码。也可以返回手动填写。</text>
            <view class="section-heading">
              <text class="section-label">{{ state === 'scanning_wifi' ? '正在搜索附近网络…' : `可用网络（${wifiList.length}）` }}</text>
            </view>
            <view v-if="wifiList.length" class="wifi-list">
              <button v-for="network in wifiList" :key="network.SSID" class="list-item" :disabled="busy" @click="selectWifi(network)">
                <view class="list-copy"><text class="item-title">{{ network.SSID }}</text><text class="item-caption">{{ network.secure ? '加密网络' : '开放网络' }}</text></view>
                <text class="signal">{{ network.signalStrength < 0 ? `${network.signalStrength} dBm` : `信号 ${network.signalStrength}` }} ›</text>
              </button>
            </view>
            <view v-else class="empty-state">
              <text>{{ state === 'scanning_wifi' ? '正在调用手机扫描…' : '暂未找到可用网络' }}</text>
              <text class="hint">{{ state === 'scanning_wifi' ? '请保持手机 Wi-Fi 和定位开启' : '可重新扫描，或返回手动输入 Wi-Fi 名称' }}</text>
            </view>
          </view>

          <view v-if="modal === 'wifi-password'">
            <text class="hint">已选择以下网络，填写密码后将通过蓝牙发送给开发板。</text>
            <view class="selected-network">
              <text class="item-caption">Wi-Fi 名称</text>
              <text class="selected-ssid">{{ selectedWifi ? selectedWifi.SSID : '' }}</text>
              <text class="hint">{{ selectedWifi && selectedWifi.secure ? '加密网络' : '开放网络，无需密码' }}</text>
            </view>
            <label class="field"><text>Wi-Fi 密码</text><input v-model="selectedWifiPassword" :disabled="busy" :password="!showSelectedWifiPassword" maxlength="63" :placeholder="selectedWifi && selectedWifi.secure ? '请输入所选网络的密码' : '开放网络可留空'" /></label>
            <view class="toggle-row"><text>显示密码</text><switch :checked="showSelectedWifiPassword" :disabled="busy" color="#238c79" @change="showSelectedWifiPassword = $event.detail.value" /></view>
          </view>

          <view v-if="modal === 'wifi-result' && wifiResult" class="wifi-result-content">
            <view class="result-icon" :class="{ 'result-ok': wifiResult.success }"><text>{{ wifiResult.success ? '✓' : '!' }}</text></view>
            <text class="result-title">{{ wifiResult.success ? 'Wi-Fi 连接成功' : 'Wi-Fi 配置未完成' }}</text>
            <text class="result-network">{{ wifiResult.ssid }}</text>
            <text class="result-detail">{{ wifiResult.message }}</text>
            <text v-if="wifiResult.ip" class="result-ip">设备 IP：{{ wifiResult.ip }}</text>
            <text class="hint result-return">{{ wifiResult.success ? '3 秒后自动返回主页面，或点击确认立即返回' : '3 秒后自动返回密码输入窗口，或点击确认立即返回' }}</text>
          </view>

          <view v-if="modal === 'mqtt'">
            <text class="hint">开发板需先连上 Wi-Fi。新服务器连接验证成功后才会保存；失败时恢复原配置。</text>
            <label class="field"><text>服务器地址</text><input v-model.trim="mqttServer" :disabled="busy" maxlength="127" placeholder="IP 或域名，例如 mqtt.example.com" /></label>
            <label class="field"><text>端口</text><input v-model="mqttPort" :disabled="busy" type="number" maxlength="5" placeholder="1883" /></label>
            <label class="field"><text>用户名</text><input v-model="mqttUsername" :disabled="busy" maxlength="63" placeholder="无认证时留空" /></label>
            <label class="field"><text>密码</text><input v-model="mqttPassword" :disabled="busy" :password="!showMqttPassword" maxlength="127" placeholder="无认证时留空" /></label>
            <view class="toggle-row"><text>显示密码</text><switch :checked="showMqttPassword" :disabled="busy" color="#238c79" @change="showMqttPassword = $event.detail.value" /></view>
            <view class="advanced-heading"><view><text class="section-label">高级设置</text><text class="hint">可选，未填写的字段保留设备当前值</text></view><switch :checked="mqttAdvanced" :disabled="busy" color="#238c79" @change="mqttAdvanced = $event.detail.value" /></view>
            <view v-if="mqttAdvanced" class="advanced-fields">
              <label class="field"><text>订阅主题 · subTopic</text><input v-model="mqttSubTopic" :disabled="busy" maxlength="39" placeholder="留空不修改" /></label>
              <label class="field"><text>发布主题 · pubTopic</text><input v-model="mqttPubTopic" :disabled="busy" maxlength="39" placeholder="留空不修改" /></label>
              <view class="field"><text>订阅 QoS</text><picker :range="mqttQosOptions" :value="mqttQosIndex" :disabled="busy" @change="mqttQosIndex = Number($event.detail.value)"><view class="picker-value">{{ mqttQosOptions[mqttQosIndex] }}<text>⌄</text></view></picker></view>
            </view>
            <text class="hint transport-note">当前使用普通 MQTT TCP 连接，不支持 TLS。</text>
          </view>

          <view v-if="modal !== 'wifi-result' && (busy || errorMessage || state === 'success')" class="modal-feedback" :class="{ error: !!errorMessage, success: state === 'success' }">
            <text>{{ errorMessage || statusText }}</text>
            <text v-if="finalIp && (modal === 'wifi' || modal === 'wifi-password')" class="ip">设备 IP：{{ finalIp }}</text>
          </view>
        </scroll-view>
        <view v-if="modal === 'wifi-result'" class="modal-footer">
          <button class="primary-button submit-button" @click="confirmWifiResult">确认</button>
        </view>
        <view v-else-if="modal === 'wifi-list'" class="modal-footer">
          <button class="secondary-button cancel-button selection-back" @click="returnToWifiForm">返回手动填写</button>
          <button class="primary-button submit-button" :loading="state === 'scanning_wifi'" :disabled="!bleConnected || busy" @click="scanWifi">{{ state === 'scanning_wifi' ? '扫描中' : '重新扫描' }}</button>
        </view>
        <view v-else-if="modal === 'wifi-password'" class="modal-footer">
          <button class="secondary-button cancel-button selection-back" :disabled="busy" @click="backToWifiList">返回列表</button>
          <button class="primary-button submit-button" :loading="activeOperation === 'wifi' && busy" :disabled="!canSubmitWifi" @click="submitWifi">{{ activeOperation === 'wifi' && busy ? '正在验证' : '保存并连接' }}</button>
        </view>
        <view v-else-if="modal !== 'bluetooth'" class="modal-footer">
          <button class="secondary-button cancel-button" @click="closeModal">{{ busy ? '收起' : '取消' }}</button>
          <button v-if="modal === 'wifi'" class="primary-button submit-button" :loading="activeOperation === 'wifi' && busy" :disabled="!canSubmitWifi" @click="submitWifi">{{ activeOperation === 'wifi' && busy ? '正在验证' : '保存并连接' }}</button>
          <button v-else class="primary-button submit-button" :loading="activeOperation === 'mqtt' && busy" :disabled="!canSubmitMqtt" @click="submitMqtt">{{ activeOperation === 'mqtt' && busy ? '正在验证' : '保存并连接' }}</button>
        </view>
      </view>
    </view>
  </view>
</template>

<script>
import configurationPage from '../../services/configuration-page.js'
export default configurationPage
</script>

<style>
.page { min-height:100vh; box-sizing:border-box; padding:40rpx 32rpx 48rpx; background:#f5f7f6; color:#20352e; }
button::after { border:0; }
.hero { padding:12rpx 6rpx 38rpx; }
.eyebrow,.title,.subtitle,.module-title,.module-description,.status-heading,.status-copy,.error-text,.ip,.modal-title,.modal-kicker,.item-title,.item-caption,.hint { display:block; }
.eyebrow { color:#238c79; font-size:24rpx; font-weight:600; letter-spacing:3rpx; }
.title { margin-top:14rpx; font-size:54rpx; font-weight:700; letter-spacing:2rpx; }
.subtitle { margin-top:14rpx; font-size:26rpx; color:#7c8b84; }
.connection-strip { display:flex; align-items:center; justify-content:space-between; min-height:64rpx; margin:0 6rpx 24rpx; gap:16rpx; }
.connection-copy { display:flex; align-items:center; font-size:24rpx; color:#5f756b; min-width:0; }
.connection-copy text { overflow:hidden; text-overflow:ellipsis; white-space:nowrap; }
.dot { width:12rpx; height:12rpx; border-radius:50%; background:#bbc6c0; margin-right:12rpx; flex-shrink:0; }
.dot.online { background:#27a37d; box-shadow:0 0 0 6rpx #e2f2e9; }
.strip-caption { font-size:23rpx; color:#97a39d; flex-shrink:0; }
.disconnect-link { margin:0; padding:0 14rpx; font-size:23rpx; line-height:56rpx; color:#b85c53; background:transparent; flex-shrink:0; }
.config-card { width:100%; display:flex; align-items:center; padding:36rpx 26rpx; margin:0 0 24rpx; min-height:172rpx; text-align:left; border:1rpx solid #e8eeea; border-radius:28rpx; background:#fff; box-shadow:0 8rpx 24rpx rgba(24,55,40,.035); line-height:1.5; }
.config-card[disabled] { background:#fff; opacity:.65; }
.module-icon { width:90rpx; height:90rpx; border-radius:25rpx; flex-shrink:0; display:flex; align-items:center; justify-content:center; margin-right:24rpx; font-weight:700; font-size:32rpx; }
.bluetooth-icon { background:#edf1fd; color:#5779c7; }.wifi-icon { background:#e5f3ed; color:#2c9575; font-size:25rpx; }.mqtt-icon { background:#e6f2fc; color:#3b8fc1; }
.module-copy { flex:1; min-width:0; }.module-title { font-size:32rpx; font-weight:600; }.module-description { margin-top:10rpx; font-size:23rpx; color:#8a9790; overflow:hidden; text-overflow:ellipsis; white-space:nowrap; }
.badge { padding:7rpx 14rpx; border-radius:30rpx; font-size:20rpx; background:#f1f3f2; color:#94a099; margin-left:8rpx; white-space:nowrap; }.badge.connected { background:#e7f5ed; color:#238260; }.chevron { color:#bcc9c1; font-size:42rpx; margin-left:14rpx; }
.status-card { margin-top:32rpx; padding:28rpx; border:1rpx solid #dfe9e3; border-radius:22rpx; background:#edf3ef; }.status-heading { font-size:25rpx; font-weight:600; margin-bottom:10rpx; }.status-copy,.error-text,.ip { font-size:24rpx; line-height:1.65; word-break:break-all; }.status-copy { color:#74867a; }.status-card.success,.modal-feedback.success { background:#e8f5ed; color:#237656; }.status-card.error,.modal-feedback.error { background:#fff0ed; border-color:#f2d8ce; color:#ae5040; }.error-text { margin-top:8rpx; }.ip { margin-top:8rpx; }
.page-note { padding:64rpx 12rpx 20rpx; text-align:center; font-size:21rpx; color:#9ba89f; line-height:1.9; }.page-note text { display:block; }
.modal-mask { position:fixed; z-index:100; top:0; right:0; bottom:0; left:0; background:rgba(14,32,24,.42); display:flex; align-items:center; justify-content:center; padding:28rpx; box-sizing:border-box; }
.modal-panel { width:100%; max-width:660rpx; max-height:88vh; border-radius:30rpx; background:#fff; overflow:hidden; display:flex; flex-direction:column; box-shadow:0 22rpx 80rpx rgba(10,30,20,.18); }
.modal-header { padding:32rpx 32rpx 24rpx; display:flex; justify-content:space-between; align-items:center; border-bottom:1rpx solid #edf1ed; flex-shrink:0; }.modal-kicker { color:#8fa299; font-size:20rpx; letter-spacing:2rpx; margin-bottom:8rpx; }.modal-title { font-size:36rpx; font-weight:650; }.close-button { margin:0; padding:0; width:64rpx; height:64rpx; line-height:60rpx; border-radius:50%; background:#f2f5f3; color:#71877a; font-size:42rpx; }
.modal-body { height:60vh; min-height:0; max-height:64vh; box-sizing:border-box; padding:24rpx 30rpx 30rpx; }
.hint { color:#8a9a90; font-size:23rpx; line-height:1.7; }.field { display:block; margin-top:24rpx; font-size:25rpx; color:#40594b; }.field input,.picker-value { margin-top:12rpx; height:80rpx; padding:0 20rpx; box-sizing:border-box; border:1rpx solid #dce6df; border-radius:14rpx; background:#f8faf9; font-size:26rpx; color:#284b37; }.picker-value { display:flex; justify-content:space-between; align-items:center; }
.toggle-row,.advanced-heading { display:flex; align-items:center; justify-content:space-between; gap:12rpx; }.toggle-row { margin-top:14rpx; font-size:23rpx; color:#8a9a90; }.toggle-row switch,.advanced-heading switch { transform:scale(.78); transform-origin:right center; }.advanced-heading { margin-top:32rpx; padding-top:26rpx; border-top:1rpx solid #e8eeea; }.section-label { font-size:25rpx; font-weight:600; }.advanced-heading .hint { margin-top:6rpx; font-size:21rpx; }.transport-note { margin-top:24rpx; font-size:21rpx; }
.section-heading { display:flex; align-items:center; justify-content:space-between; margin-top:24rpx; }.small-button,.secondary-button { border-radius:14rpx; background:#edf5f0; color:#37795e; font-size:25rpx; }.small-button { margin:0; padding:0 20rpx; font-size:23rpx; line-height:58rpx; }.scan-button { margin-top:24rpx; }.device-list,.wifi-list { margin-top:20rpx; }.wifi-list { max-height:270rpx; overflow-y:auto; }.list-item { display:flex; align-items:center; justify-content:space-between; padding:20rpx 18rpx; margin:12rpx 0; text-align:left; line-height:1.5; background:#f8faf9; border:1rpx solid #e4ece6; border-radius:14rpx; }.list-item.selected { background:#ecf7f0; border-color:#67ae8e; }.list-copy { min-width:0; flex:1; }.item-title { font-size:26rpx; font-weight:600; word-break:break-all; }.item-caption { margin-top:6rpx; color:#94a297; font-size:20rpx; word-break:break-all; }.signal { color:#92a397; font-size:20rpx; margin-left:12rpx; white-space:nowrap; }.empty-state { text-align:center; padding:52rpx 0; color:#8b9a90; font-size:26rpx; }.empty-state .hint { margin-top:10rpx; }.connected-panel { padding:10rpx 0; }.connected-panel .hint { margin-top:12rpx; }.danger-button { background:#fff0ed; color:#b95b4b; margin-top:32rpx; font-size:27rpx; border-radius:14rpx; }
.modal-feedback { margin-top:24rpx; padding:20rpx; border-radius:14rpx; background:#f1f6f3; color:#68816f; font-size:24rpx; line-height:1.7; word-break:break-all; }
.modal-footer { display:flex; gap:18rpx; padding:22rpx 30rpx 28rpx; padding-bottom:calc(28rpx + env(safe-area-inset-bottom)); border-top:1rpx solid #edf1ed; flex-shrink:0; }.cancel-button { width:28%; margin:0; }.submit-button { flex:1; margin:0; }.primary-button { border-radius:14rpx; color:#fff; background:#238c79; font-size:27rpx; }.primary-button[disabled] { background:#dbe5df; color:#9aafa0; }
.scan-description { margin-top:14rpx; }.manual-heading { display:block; margin-top:30rpx; padding-top:26rpx; border-top:1rpx solid #edf1ed; }
.network-selection .wifi-list { max-height:none; }.selected-network { margin-top:26rpx; padding:24rpx; border-radius:18rpx; border:1rpx solid #cfe5d9; background:#eef7f1; }.selected-network .item-caption { margin-top:0; }.selected-ssid { display:block; margin:12rpx 0; font-size:32rpx; font-weight:650; color:#237656; word-break:break-all; }
.password-panel .modal-body { height:42vh; max-height:48vh; }.selection-back { width:40%; font-size:24rpx; padding:0 12rpx; }
.result-panel .modal-body { height:44vh; max-height:54vh; }.wifi-result-content { padding:12rpx 0 8rpx; text-align:center; }.result-icon { display:flex; align-items:center; justify-content:center; width:96rpx; height:96rpx; margin:0 auto 24rpx; border-radius:50%; background:#fff0ed; color:#b85b48; font-size:54rpx; font-weight:700; }.result-icon.result-ok { background:#e5f5ec; color:#238c66; }.result-title,.result-network,.result-detail,.result-ip { display:block; word-break:break-all; }.result-title { font-size:34rpx; font-weight:650; }.result-network { margin-top:16rpx; font-size:27rpx; color:#3d6854; }.result-detail { margin-top:20rpx; font-size:25rpx; line-height:1.75; color:#75847c; }.result-ip { margin-top:16rpx; font-size:25rpx; color:#238c66; }.result-return { margin-top:28rpx; font-size:23rpx; }
</style>
