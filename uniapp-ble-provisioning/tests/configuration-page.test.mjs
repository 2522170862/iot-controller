import test from 'node:test'
import assert from 'node:assert/strict'
import options from '../services/configuration-page.js'

function page() {
  const state=options.data()
  for(const [name,fn] of Object.entries(options.methods)) state[name]=fn.bind(state)
  for(const [name,fn] of Object.entries(options.computed)) Object.defineProperty(state,name,{get:()=>fn.call(state)})
  return state
}
test('configuration cards require a Bluetooth connection before configuration',()=>{
  const p=page();p.openModal('mqtt');assert.equal(p.modal,'bluetooth')
  p.bleConnected=true;p.openModal('mqtt');assert.equal(p.modal,'mqtt')
})
test('advanced MQTT form omits blank optional fields and sends explicitly selected QoS zero',()=>{
  const p=page();p.mqttServer=' mqtt.example.com ';p.mqttUsername='u';p.mqttPassword='p'
  assert.deepEqual(p.mqttConfiguration(),{server:'mqtt.example.com',port:1883,username:'u',password:'p'})
  p.mqttAdvanced=true;p.mqttPubTopic='new/out';p.mqttQosIndex=1
  assert.deepEqual(p.mqttConfiguration(),{server:'mqtt.example.com',port:1883,username:'u',password:'p',pubTopic:'new/out',subQos:0})
})
test('early MQTT success is not overwritten by the send promise resolving',async()=>{
  const p=page();p.bleConnected=true;p.mqttServer='mqtt.example.com';p.mqttPassword='secret'
  p.bleService={sendMqttConfiguration:async()=>{p.handleProvisioningMessage({event:'mqtt_connected'})}}
  await p.submitMqtt()
  assert.equal(p.state,'success');assert.equal(p.mqttPassword,'');assert.equal(p.mqttSummary,'mqtt.example.com:1883')
})
test('Wi-Fi flow retains scan-independent manual entry and success IP',async()=>{
  const p=page();p.bleConnected=true;p.ssid='Lab';p.password='pass'
  let sent
  p.bleService={sendWifiCredentials:async(...args)=>{sent=args}}
  await p.submitWifi();assert.deepEqual(sent,['Lab','pass']);assert.equal(p.state,'validating')
  p.handleProvisioningMessage({event:'wifi_connected',ip:'192.168.1.4'})
  assert.equal(p.finalIp,'192.168.1.4');assert.equal(p.password,'');assert.equal(p.state,'success')
})
test('MQTT failure leaves the old success summary and displays an actionable error',()=>{
  const p=page();p.activeOperation='mqtt';p.mqttSummary='old.example.com:1883'
  p.handleProvisioningMessage({event:'mqtt_failed',reason:'authentication_failed'})
  assert.equal(p.state,'error');assert.match(p.errorMessage,/用户名|密码/);assert.equal(p.mqttSummary,'old.example.com:1883')
})
test('disconnect clears credentials and connection state',async()=>{
  const p=page();p.bleConnected=true;p.password='secret';p.mqttPassword='secret';let disconnected=false
  p.bleService={disconnect:async()=>{disconnected=true}}
  await p.disconnectDevice()
  assert(disconnected);assert.equal(p.bleConnected,false);assert.equal(p.password,'');assert.equal(p.mqttPassword,'');assert.equal(p.state,'checking')
})

test('a delayed send failure from an old connection cannot cancel a new MQTT transaction',async()=>{
  const p=page();p.bleConnected=true;p.mqttServer='old.example.com'
  let rejectOld
  p.bleService={sendMqttConfiguration:()=>new Promise((_,reject)=>{rejectOld=reject}),disconnect:async()=>{}}
  const oldSubmission=p.submitMqtt()
  await p.disconnectDevice()
  p.bleConnected=true;p.mqttServer='new.example.com'
  p.bleService={sendMqttConfiguration:async()=>{}}
  await p.submitMqtt()
  rejectOld(new Error('old write disconnected'))
  await oldSubmission
  assert.equal(p.activeOperation,'mqtt');assert.equal(p.state,'validating')
  p.handleProvisioningMessage({event:'mqtt_connected'})
  assert.equal(p.state,'success');assert.equal(p.mqttSummary,'new.example.com:1883')
})

test('phone scanning immediately opens a separate network window and preserves manual fields', async () => {
  const p=page();p.bleConnected=true;p.modal='wifi';p.ssid='Manual';p.password='manual-pass'
  let finish
  p.wifiService={scan:()=>new Promise(resolve=>{finish=resolve})}
  const scanning=p.scanWifi()
  assert.equal(p.modal,'wifi-list');assert.equal(p.state,'scanning_wifi')
  finish([{SSID:'Lab',secure:true}]);await scanning
  assert.equal(p.modal,'wifi-list');assert.equal(p.wifiList[0].SSID,'Lab')
  assert.equal(p.ssid,'Manual');assert.equal(p.password,'manual-pass')
})

test('selecting a network opens its password window without copying a previous password', () => {
  const p=page();p.bleConnected=true;p.state='ready';p.modal='wifi-list';p.password='manual-pass'
  p.selectWifi({SSID:'Lab',secure:true})
  assert.equal(p.modal,'wifi-password');assert.equal(p.selectedWifi.SSID,'Lab')
  assert.equal(p.selectedWifiPassword,'');assert.equal(p.showSelectedWifiPassword,false)
  assert.equal(p.password,'manual-pass');assert.equal(p.canSubmitWifi,true)
})

test('returning to the network list clears the selected password and allows choosing again', () => {
  const p=page();p.bleConnected=true;p.state='ready';p.modal='wifi-list'
  p.selectWifi({SSID:'Lab',secure:true});p.selectedWifiPassword='first-pass';p.showSelectedWifiPassword=true
  p.backToWifiList()
  assert.equal(p.modal,'wifi-list');assert.equal(p.selectedWifiPassword,'')
  p.selectWifi({SSID:'Guest',secure:false})
  assert.equal(p.selectedWifi.SSID,'Guest');assert.equal(p.selectedWifiPassword,'')
  assert.equal(p.showSelectedWifiPassword,false)
})

test('the selected-network form sends the selected SSID and its own password over existing BLE', async () => {
  const p=page();p.bleConnected=true;p.state='ready';p.modal='wifi-list'
  p.ssid='Manual';p.password='manual-pass';p.selectWifi({SSID:'Lab',secure:true});p.selectedWifiPassword='lab-pass'
  let sent
  p.bleService={sendWifiCredentials:async(...args)=>{sent=args}}
  await p.submitWifi()
  assert.deepEqual(sent,['Lab','lab-pass']);assert.equal(p.state,'validating')
  p.handleProvisioningMessage({event:'wifi_connected',ip:'192.168.1.4'})
  assert.equal(p.wifiSummary,'Lab');assert.equal(p.selectedWifiPassword,'')
  assert.equal(p.state,'success');assert.equal(p.finalIp,'192.168.1.4')
  p.confirmWifiResult()
})

test('closing the selection window cancels scanning and ignores its late result', async () => {
  const p=page();p.bleConnected=true;p.modal='wifi';p.ssid='Manual';p.password='manual-pass'
  let finish,cancelled=false
  p.wifiService={scan:()=>new Promise(resolve=>{finish=resolve}),cancelScan:()=>{cancelled=true}}
  const scanning=p.scanWifi()
  p.closeModal()
  assert.equal(cancelled,true);assert.equal(p.modal,'wifi');assert.equal(p.busy,false)
  finish([{SSID:'Late'}]);await scanning
  assert.equal(p.modal,'wifi');assert.equal(p.wifiList.length,0)
  assert.equal(p.ssid,'Manual');assert.equal(p.password,'manual-pass')
})

test('a failed scan stays in the selection window and can return to manual entry', async () => {
  const p=page();p.bleConnected=true;p.modal='wifi';p.ssid='Manual'
  p.wifiService={scan:async()=>{throw new Error('scan timeout')},cancelScan:()=>{}}
  await p.scanWifi()
  assert.equal(p.modal,'wifi-list');assert.equal(p.state,'error');assert.equal(p.errorMessage,'scan timeout')
  p.returnToWifiForm()
  assert.equal(p.modal,'wifi');assert.equal(p.busy,false);assert.equal(p.ssid,'Manual')
})

test('a scan response after Bluetooth disconnect cannot restore the old selection window', async () => {
  const p=page();p.bleConnected=true;p.modal='wifi'
  let finish
  p.wifiService={scan:()=>new Promise(resolve=>{finish=resolve}),cancelScan:()=>{}}
  const scanning=p.scanWifi();p.resetLink();p.state='error'
  finish([{SSID:'Late'}]);await scanning
  assert.equal(p.modal,'');assert.equal(p.state,'error');assert.equal(p.wifiList.length,0)
})

async function selectedWifiAttempt(t) {
  t.mock.timers.enable({ apis: ['setTimeout'] })
  const p=page();p.bleConnected=true;p.state='ready';p.modal='wifi-list'
  p.selectWifi({SSID:'Lab',secure:true});p.selectedWifiPassword='lab-pass'
  p.bleService={sendWifiCredentials:async()=>{}}
  await p.submitWifi()
  return p
}

test('sending BLE configuration is not treated as a successful Wi-Fi connection', async t => {
  const p=await selectedWifiAttempt(t)
  t.mock.timers.tick(4000)
  assert.equal(p.modal,'wifi-password');assert.equal(p.state,'validating')
  assert.equal(p.activeOperation,'wifi');assert.equal(p.wifiResult,null)
})

test('a real Wi-Fi success opens a result page then returns home exactly after three seconds', async t => {
  const p=await selectedWifiAttempt(t)
  p.handleProvisioningMessage({event:'wifi_connected',ip:'192.168.1.8'})
  assert.equal(p.modal,'wifi-result');assert.equal(p.wifiResult.success,true)
  assert.equal(p.wifiResult.ssid,'Lab');assert.equal(p.wifiResult.ip,'192.168.1.8')
  assert.equal(p.selectedWifiPassword,'');assert.equal(p.state,'success')
  t.mock.timers.tick(2999);assert.equal(p.modal,'wifi-result')
  t.mock.timers.tick(1);assert.equal(p.modal,'');assert.equal(p.wifiResult,null)
})

test('confirming success returns home immediately and cannot later close a new MQTT window', async t => {
  const p=await selectedWifiAttempt(t)
  p.handleProvisioningMessage({event:'wifi_connected',ip:'192.168.1.8'})
  p.confirmWifiResult();assert.equal(p.modal,'');assert.equal(p.wifiResultTimer,null)
  p.openModal('mqtt');t.mock.timers.tick(3000)
  assert.equal(p.modal,'mqtt')
})

test('Wi-Fi failure returns to the same password form after three seconds', async t => {
  const p=await selectedWifiAttempt(t)
  p.handleProvisioningMessage({event:'wifi_failed',reason:'connection_failed'})
  assert.equal(p.modal,'wifi-result');assert.equal(p.wifiResult.success,false)
  assert.match(p.wifiResult.message,/连接失败/)
  t.mock.timers.tick(2999);assert.equal(p.modal,'wifi-result')
  t.mock.timers.tick(1)
  assert.equal(p.modal,'wifi-password');assert.equal(p.selectedWifi.SSID,'Lab')
  assert.equal(p.selectedWifiPassword,'lab-pass');assert.equal(p.canSubmitWifi,true)
})

test('confirming failure returns immediately and an old countdown cannot interrupt a new result', async t => {
  const p=await selectedWifiAttempt(t)
  p.handleProvisioningMessage({event:'wifi_failed',reason:'timeout'})
  t.mock.timers.tick(1000);p.confirmWifiResult()
  assert.equal(p.modal,'wifi-password');assert.equal(p.wifiResultTimer,null)
  await p.submitWifi()
  p.handleProvisioningMessage({event:'wifi_connected',ip:'192.168.1.9'})
  t.mock.timers.tick(2000);assert.equal(p.modal,'wifi-result')
  t.mock.timers.tick(1000);assert.equal(p.modal,'')
})

test('missing device replies are labelled unconfirmed, never as a successful connection', async t => {
  const p=await selectedWifiAttempt(t)
  p.handleProvisioningMessage({event:'error',reason:'device_timeout'})
  assert.equal(p.modal,'wifi-result');assert.equal(p.wifiResult.success,false)
  assert.match(p.wifiResult.message,/尚未确认/)
  p.confirmWifiResult();assert.equal(p.modal,'wifi-password')
})

test('disconnecting clears the result countdown so it cannot reopen a password form', async t => {
  const p=await selectedWifiAttempt(t)
  p.handleProvisioningMessage({event:'wifi_failed',reason:'connection_failed'})
  p.resetLink();p.state='error'
  t.mock.timers.tick(3000)
  assert.equal(p.modal,'');assert.equal(p.wifiResult,null);assert.equal(p.wifiResultTimer,null)
  assert.equal(p.selectedWifi,null);assert.equal(p.state,'error')
})

test('manual Wi-Fi configuration keeps its original result display without automatic navigation', async t => {
  t.mock.timers.enable({apis:['setTimeout']})
  const p=page();p.bleConnected=true;p.modal='wifi';p.ssid='Manual'
  p.bleService={sendWifiCredentials:async()=>{}}
  await p.submitWifi();p.handleProvisioningMessage({event:'wifi_connected',ip:'192.168.1.6'})
  t.mock.timers.tick(3000)
  assert.equal(p.modal,'wifi');assert.equal(p.state,'success');assert.equal(p.wifiResult,null)
})

test('closing the result window uses the same immediate return as its confirm button', async t => {
  const p=await selectedWifiAttempt(t)
  p.handleProvisioningMessage({event:'wifi_failed',reason:'connection_failed'})
  p.closeModal()
  assert.equal(p.modal,'wifi-password');assert.equal(p.wifiResultTimer,null)
  p.backToWifiList();t.mock.timers.tick(3000)
  assert.equal(p.modal,'wifi-list')
})

test('unloading the page cancels the Wi-Fi result timer', async t => {
  const p=await selectedWifiAttempt(t)
  p.handleProvisioningMessage({event:'wifi_connected',ip:'192.168.1.8'})
  p.wifiService={close:async()=>{}};p.bleService={close:async()=>{}}
  options.onUnload.call(p)
  assert.equal(p.wifiResultTimer,null);assert.equal(p.wifiResult,null)
  p.modal='other-page';t.mock.timers.tick(3000)
  assert.equal(p.modal,'other-page')
})
