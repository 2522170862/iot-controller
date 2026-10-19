import test from 'node:test'
import assert from 'node:assert/strict'
import { BleService } from '../services/ble-service.js'
import { BLE_SERVICE_UUID, BLE_RX_UUID, BLE_TX_UUID } from '../utils/protocol.js'
import pageOptions from '../services/configuration-page.js'

function installDevice() {
  const state={ writes:[], handlers:{}, closes:0, removed:0 }
  const ok=(options,value={})=>options.success(value)
  globalThis.uni={
    openBluetoothAdapter:ok, createBLEConnection:ok,
    getBLEDeviceServices:o=>ok(o,{services:[{uuid:BLE_SERVICE_UUID}]}),
    getBLEDeviceCharacteristics:o=>ok(o,{characteristics:[{uuid:BLE_RX_UUID,properties:{write:true}},{uuid:BLE_TX_UUID,properties:{notify:true}}]}),
    notifyBLECharacteristicValueChange:ok,
    writeBLECharacteristicValue:o=>{state.writes.push(new Uint8Array(o.value));ok(o)},
    closeBLEConnection:o=>{state.closes++;ok(o)},
    onBLECharacteristicValueChange:fn=>{state.handlers.message=fn},
    onBLEConnectionStateChange:fn=>{state.handlers.connection=fn},
    offBLECharacteristicValueChange:()=>{state.removed++},
    offBLEConnectionStateChange:()=>{state.removed++},
  }
  state.reply=message=>state.handlers.message({deviceId:'board',characteristicId:BLE_TX_UUID,value:new TextEncoder().encode(JSON.stringify(message)+'\n').buffer})
  return state
}
const config={server:'mqtt.example.com',port:1883,username:'',password:''}

test('wrong-password retry accepts only its new result and returns a successful page', async t => {
  t.mock.timers.enable({apis:['setTimeout']})
  const device=installDevice(),p=pageOptions.data()
  for(const [name,fn] of Object.entries(pageOptions.methods)) p[name]=fn.bind(p)
  for(const [name,fn] of Object.entries(pageOptions.computed)) Object.defineProperty(p,name,{get:()=>fn.call(p)})
  await p.bleService.connect('board',message=>p.handleProvisioningMessage(message),()=>{})
  p.bleConnected=true;p.state='ready';p.modal='wifi-list'
  p.selectWifi({SSID:'Lab',secure:true});p.selectedWifiPassword='wrong-pass'
  await p.submitWifi()
  const firstId=p.bleService.currentRequestId
  device.reply({id:firstId,event:'wifi_failed',reason:'connection_failed'})
  assert.equal(p.wifiResult.success,false)
  p.confirmWifiResult();p.selectedWifiPassword='correct-pass'
  device.writes=[]
  await p.submitWifi()
  const secondId=p.bleService.currentRequestId
  assert.notEqual(secondId,firstId)
  const sent=JSON.parse(Buffer.concat(device.writes.map(v=>Buffer.from(v))).toString())
  assert.equal(sent.password,'correct-pass')
  device.reply({id:firstId,event:'wifi_failed',reason:'connection_failed'})
  assert.equal(p.state,'validating');assert.equal(p.wifiResult,null)
  device.reply({id:secondId,event:'wifi_connected',ip:'192.168.5.53'})
  assert.equal(p.wifiResult.success,true);assert.equal(p.errorMessage,'')
  assert.equal(p.wifiResult.ip,'192.168.5.53')
  t.mock.timers.tick(3000);assert.equal(p.modal,'')
  await p.bleService.disconnect()
})

test('BLE sends MQTT JSON and rejects a second configuration until a terminal reply', async()=>{
  const device=installDevice(), messages=[],service=new BleService()
  await service.connect('board',m=>messages.push(m),()=>{})
  const id=await service.sendMqttConfiguration(config)
  const bytes=Buffer.concat(device.writes.map(v=>Buffer.from(v)))
  assert.deepEqual(JSON.parse(bytes.toString()),{id,cmd:'configure_mqtt',...config})
  await assert.rejects(service.sendWifiCredentials('Lab',''),/处理|等待/)
  device.reply({id:'000000ff',event:'mqtt_connected'})
  assert.equal(messages.length,0)
  device.reply({id,event:'mqtt_connected'})
  assert.equal(messages[0].event,'mqtt_connected')
  assert.equal(service.currentRequestId,'')
  await service.disconnect()
})

test('manual disconnect clears requests and listeners and permits reconnect',async()=>{
  const device=installDevice(),service=new BleService()
  await service.connect('board',()=>{},()=>{})
  await service.sendWifiCredentials('Lab','')
  await service.disconnect()
  assert.equal(service.deviceId,'')
  assert.equal(service.currentRequestId,'')
  assert.equal(device.closes,1)
  assert.equal(device.removed,2)
  await service.connect('board',()=>{},()=>{})
  assert.equal(service.deviceId,'board')
  await service.disconnect()
})

test('request timeout reports an error and releases the request lock',async()=>{
  installDevice()
  const messages=[],service=new BleService({requestTimeoutMs:15})
  await service.connect('board',m=>messages.push(m),()=>{})
  const id=await service.sendMqttConfiguration(config)
  await new Promise(resolve=>setTimeout(resolve,35))
  assert.deepEqual(messages,[{id,event:'error',reason:'device_timeout'}])
  assert.equal(service.currentRequestId,'')
  await service.disconnect()
})

test('firmware uncorrelated protocol errors release the current request',async()=>{
  const device=installDevice(),messages=[],service=new BleService()
  await service.connect('board',m=>messages.push(m),()=>{})
  await service.sendMqttConfiguration(config)
  device.reply({id:'00000000',event:'error',reason:'message_too_long'})
  assert.equal(messages[0].reason,'message_too_long')
  assert.equal(service.currentRequestId,'')
  await service.disconnect()
})

test('disconnect during notification setup cannot resolve as a connected session',async()=>{
  const device=installDevice(),service=new BleService()
  uni.notifyBLECharacteristicValueChange=options=>{
    device.handlers.connection({deviceId:'board',connected:false})
    options.success({})
  }
  await assert.rejects(service.connect('board',()=>{},()=>{}),/断开|取消/)
  assert.equal(service.deviceId,'')
})
