import test from 'node:test'
import assert from 'node:assert/strict'

import {
  JsonLineDecoder,
  concatChunks,
  createRequestId,
  encodeWifiRequest,
  isReplyForRequest,
} from '../utils/protocol.js'

test('createRequestId returns eight lowercase hexadecimal characters', () => {
  assert.match(createRequestId(), /^[0-9a-f]{8}$/)
})

test('encodeWifiRequest produces UTF-8 chunks no larger than 20 bytes', () => {
  const chunks = encodeWifiRequest('a1b2c3d4', '实验室WiFi', '12345678')

  assert.ok(chunks.length > 1)
  assert.ok(chunks.every((chunk) => chunk instanceof Uint8Array))
  assert.ok(chunks.every((chunk) => chunk.byteLength <= 20))

  const frame = new TextDecoder().decode(concatChunks(chunks))
  assert.equal(frame.endsWith('\n'), true)
  assert.deepEqual(JSON.parse(frame), {
    id: 'a1b2c3d4',
    cmd: 'configure_wifi',
    ssid: '实验室WiFi',
    password: '12345678',
  })
})

test('encodeWifiRequest validates identifiers and UTF-8 byte limits', () => {
  assert.throws(() => encodeWifiRequest('INVALID', 'WiFi', ''), /请求 ID/)
  assert.throws(() => encodeWifiRequest('a1b2c3d4', '', ''), /Wi-Fi 名称/)
  assert.throws(
    () => encodeWifiRequest('a1b2c3d4', '中'.repeat(11), ''),
    /32 字节/,
  )
  assert.throws(
    () => encodeWifiRequest('a1b2c3d4', 'WiFi', '密'.repeat(22)),
    /63 字节/,
  )
})

test('JsonLineDecoder retains a split multibyte UTF-8 character', () => {
  const decoder = new JsonLineDecoder()
  const bytes = new TextEncoder().encode(
    '{"id":"a1b2c3d4","event":"wifi_connected","name":"实验室"}\n',
  )
  const split = bytes.indexOf(0xe5) + 1

  assert.deepEqual(decoder.push(bytes.slice(0, split).buffer), [])
  assert.deepEqual(decoder.push(bytes.slice(split).buffer), [
    {
      id: 'a1b2c3d4',
      event: 'wifi_connected',
      name: '实验室',
    },
  ])
})

test('JsonLineDecoder returns every complete frame in one notification', () => {
  const decoder = new JsonLineDecoder()
  const bytes = new TextEncoder().encode(
    '{"id":"a1b2c3d4","event":"wifi_connecting"}\n' +
      '{"id":"a1b2c3d4","event":"wifi_connected","ip":"192.168.1.8"}\n',
  )

  assert.deepEqual(decoder.push(bytes.buffer), [
    { id: 'a1b2c3d4', event: 'wifi_connecting' },
    {
      id: 'a1b2c3d4',
      event: 'wifi_connected',
      ip: '192.168.1.8',
    },
  ])
})

test('reply correlation ignores messages from another request', () => {
  assert.equal(
    isReplyForRequest(
      { id: 'ffffffff', event: 'wifi_connected' },
      'a1b2c3d4',
    ),
    false,
  )
  assert.equal(
    isReplyForRequest(
      { id: 'a1b2c3d4', event: 'wifi_connected' },
      'a1b2c3d4',
    ),
    true,
  )
})
