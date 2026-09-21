export const BLE_SERVICE_UUID = '6E400001-B5A3-F393-E0A9-E50E24DCCA9E'
export const BLE_RX_UUID = '6E400002-B5A3-F393-E0A9-E50E24DCCA9E'
export const BLE_TX_UUID = '6E400003-B5A3-F393-E0A9-E50E24DCCA9E'
export const BLE_DEVICE_NAME_PREFIX = 'IoT-Controller-'

const MAX_CHUNK_BYTES = 20
const MAX_MESSAGE_BYTES = 256
const encoder = new TextEncoder()

export function createRequestId() {
  let id = ''
  for (let index = 0; index < 8; index += 1) {
    id += Math.floor(Math.random() * 16).toString(16)
  }
  return id
}

export function concatChunks(chunks) {
  const size = chunks.reduce((total, chunk) => total + chunk.byteLength, 0)
  const result = new Uint8Array(size)
  let offset = 0
  for (const chunk of chunks) {
    result.set(chunk, offset)
    offset += chunk.byteLength
  }
  return result
}

function encodedLength(value) {
  return encoder.encode(value).byteLength
}

export function encodeWifiRequest(id, ssid, password) {
  if (!/^[0-9a-f]{8}$/.test(id)) {
    throw new Error('请求 ID 必须是 8 位小写十六进制字符')
  }
  const ssidBytes = encodedLength(ssid)
  if (ssidBytes < 1 || ssidBytes > 32) {
    throw new Error('Wi-Fi 名称必须为 1 至 32 字节')
  }
  const passwordBytes = encodedLength(password)
  if (passwordBytes > 63) {
    throw new Error('Wi-Fi 密码不能超过 63 字节')
  }

  const frame = encoder.encode(
    `${JSON.stringify({
      id,
      cmd: 'configure_wifi',
      ssid,
      password,
    })}\n`,
  )
  if (frame.byteLength > MAX_MESSAGE_BYTES) {
    throw new Error('配网消息不能超过 256 字节')
  }

  const chunks = []
  for (let offset = 0; offset < frame.byteLength; offset += MAX_CHUNK_BYTES) {
    chunks.push(frame.slice(offset, offset + MAX_CHUNK_BYTES))
  }
  return chunks
}

export function isReplyForRequest(message, requestId) {
  return Boolean(message && message.id === requestId)
}

export function provisioningFailureReason(message) {
  if (
    message?.event !== 'wifi_failed' &&
    message?.event !== 'error' &&
    message?.event !== 'protocol_error'
  ) {
    return null
  }
  return message.reason || 'connection_failed'
}

export class JsonLineDecoder {
  constructor() {
    this.decoder = new TextDecoder()
    this.pendingText = ''
  }

  push(buffer) {
    const bytes =
      buffer instanceof Uint8Array
        ? buffer
        : new Uint8Array(buffer)
    this.pendingText += this.decoder.decode(bytes, { stream: true })

    const frames = []
    let newlineIndex = this.pendingText.indexOf('\n')
    while (newlineIndex >= 0) {
      const line = this.pendingText.slice(0, newlineIndex).trim()
      this.pendingText = this.pendingText.slice(newlineIndex + 1)
      if (line.length > 0) {
        frames.push(JSON.parse(line))
      }
      newlineIndex = this.pendingText.indexOf('\n')
    }
    return frames
  }

  reset() {
    this.decoder = new TextDecoder()
    this.pendingText = ''
  }
}
