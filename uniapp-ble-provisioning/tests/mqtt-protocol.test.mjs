import test from 'node:test'
import assert from 'node:assert/strict'
import * as protocol from '../utils/protocol.js'

const configuration = { server: 'mqtt.example.com', port: 1883, username: '用户', password: 'secret' }
function decoded(chunks) { return JSON.parse(new TextDecoder().decode(protocol.concatChunks(chunks))) }

test('MQTT configuration uses the agreed JSON and omits unspecified topics', () => {
  const chunks = protocol.encodeMqttRequest('1234abcd', configuration)
  assert.ok(chunks.every(chunk => chunk.length <= 20))
  assert.deepEqual(decoded(chunks), { id: '1234abcd', cmd: 'configure_mqtt', ...configuration })
})

test('MQTT optional fields are independent and preserve explicit QoS zero', () => {
  assert.deepEqual(decoded(protocol.encodeMqttRequest('1234abcd', { ...configuration, subQos: 0 })),
    { id: '1234abcd', cmd: 'configure_mqtt', ...configuration, subQos: 0 })
  assert.equal(decoded(protocol.encodeMqttRequest('1234abcd', { ...configuration, pubTopic: 'new/data' })).pubTopic, 'new/data')
})

test('MQTT encoder validates ports, hosts, topics and UTF-8 limits', () => {
  for (const override of [{port: 0}, {port: '1883'}, {port: 65536}, {port: 1.5},
    {server: 'mqtt://host'}, {server: 'bad host'}, {server: ''},
    {username: '中'.repeat(22)}, {password: 'x'.repeat(128)},
    {pubTopic: 'a/#'}, {subTopic: 'a/#/b'}, {subTopic: ''}, {subQos: 2},
    {subQos: null}, {pubTopic: 'x'.repeat(40)}]) {
    assert.throws(() => protocol.encodeMqttRequest('1234abcd', {...configuration, ...override}), undefined, JSON.stringify(override))
  }
  assert.equal(decoded(protocol.encodeMqttRequest('1234abcd', {...configuration, subTopic: 'a/+/b/#'})).subTopic, 'a/+/b/#')
})

test('MQTT frames may exceed the former 256-byte Wi-Fi-only limit', () => {
  const chunks = protocol.encodeMqttRequest('1234abcd', {...configuration, server: 'a'.repeat(127), password: 'p'.repeat(127)})
  assert.ok(protocol.concatChunks(chunks).length > 256)
  assert.equal(decoded(chunks).password.length, 127)
})

test('MQTT failures use the same failure helper as Wi-Fi', () => {
  assert.equal(protocol.provisioningFailureReason({event:'mqtt_failed', reason:'authentication_failed'}), 'authentication_failed')
})
