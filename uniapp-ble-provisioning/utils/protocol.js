export const BLE_SERVICE_UUID = '6E400001-B5A3-F393-E0A9-E50E24DCCA9E'
export const BLE_RX_UUID = '6E400002-B5A3-F393-E0A9-E50E24DCCA9E'
export const BLE_TX_UUID = '6E400003-B5A3-F393-E0A9-E50E24DCCA9E'

const MAX_CHUNK_BYTES = 20
const MAX_MESSAGE_BYTES = 256

function appendCodePoint(output, codePoint) {
  if (codePoint <= 0xffff) {
    return output + String.fromCharCode(codePoint)
  }
  const offset = codePoint - 0x10000
  return output + String.fromCharCode(0xd800 + (offset >> 10), 0xdc00 + (offset & 0x3ff))
}

function encodeUtf8(value) {
  const bytes = []
  for (let index = 0; index < value.length; index += 1) {
    let codePoint = value.charCodeAt(index)
    if (codePoint >= 0xd800 && codePoint <= 0xdbff) {
      const lowSurrogate = value.charCodeAt(index + 1)
      if (lowSurrogate >= 0xdc00 && lowSurrogate <= 0xdfff) {
        codePoint =
          0x10000 + ((codePoint - 0xd800) << 10) + (lowSurrogate - 0xdc00)
        index += 1
      } else {
        codePoint = 0xfffd
      }
    } else if (codePoint >= 0xdc00 && codePoint <= 0xdfff) {
      codePoint = 0xfffd
    }

    if (codePoint <= 0x7f) {
      bytes.push(codePoint)
    } else if (codePoint <= 0x7ff) {
      bytes.push(0xc0 | (codePoint >> 6), 0x80 | (codePoint & 0x3f))
    } else if (codePoint <= 0xffff) {
      bytes.push(
        0xe0 | (codePoint >> 12),
        0x80 | ((codePoint >> 6) & 0x3f),
        0x80 | (codePoint & 0x3f),
      )
    } else {
      bytes.push(
        0xf0 | (codePoint >> 18),
        0x80 | ((codePoint >> 12) & 0x3f),
        0x80 | ((codePoint >> 6) & 0x3f),
        0x80 | (codePoint & 0x3f),
      )
    }
  }
  return new Uint8Array(bytes)
}

function isContinuationByte(value) {
  return (value & 0xc0) === 0x80
}

function decodeUtf8(bytes) {
  let output = ''
  let index = 0
  while (index < bytes.length) {
    const first = bytes[index]
    let codePoint = 0xfffd
    let sequenceLength = 1

    if (first <= 0x7f) {
      codePoint = first
    } else if (
      first >= 0xc2 &&
      first <= 0xdf &&
      index + 1 < bytes.length &&
      isContinuationByte(bytes[index + 1])
    ) {
      codePoint = ((first & 0x1f) << 6) | (bytes[index + 1] & 0x3f)
      sequenceLength = 2
    } else if (
      first >= 0xe0 &&
      first <= 0xef &&
      index + 2 < bytes.length &&
      isContinuationByte(bytes[index + 1]) &&
      isContinuationByte(bytes[index + 2]) &&
      !(first === 0xe0 && bytes[index + 1] < 0xa0) &&
      !(first === 0xed && bytes[index + 1] >= 0xa0)
    ) {
      codePoint =
        ((first & 0x0f) << 12) |
        ((bytes[index + 1] & 0x3f) << 6) |
        (bytes[index + 2] & 0x3f)
      sequenceLength = 3
    } else if (
      first >= 0xf0 &&
      first <= 0xf4 &&
      index + 3 < bytes.length &&
      isContinuationByte(bytes[index + 1]) &&
      isContinuationByte(bytes[index + 2]) &&
      isContinuationByte(bytes[index + 3]) &&
      !(first === 0xf0 && bytes[index + 1] < 0x90) &&
      !(first === 0xf4 && bytes[index + 1] >= 0x90)
    ) {
      codePoint =
        ((first & 0x07) << 18) |
        ((bytes[index + 1] & 0x3f) << 12) |
        ((bytes[index + 2] & 0x3f) << 6) |
        (bytes[index + 3] & 0x3f)
      sequenceLength = 4
    }

    output = appendCodePoint(output, codePoint)
    index += sequenceLength
  }
  return output
}

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
  return encodeUtf8(value).byteLength
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

  const frame = encodeUtf8(
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
    this.pendingBytes = new Uint8Array(0)
  }

  push(buffer) {
    const bytes =
      buffer instanceof Uint8Array
        ? buffer
        : new Uint8Array(buffer)
    this.pendingBytes = concatChunks([this.pendingBytes, bytes])

    const frames = []
    let frameStart = 0
    for (let index = 0; index < this.pendingBytes.length; index += 1) {
      if (this.pendingBytes[index] !== 0x0a) {
        continue
      }
      const line = decodeUtf8(this.pendingBytes.slice(frameStart, index)).trim()
      if (line.length > 0) {
        frames.push(JSON.parse(line))
      }
      frameStart = index + 1
    }
    this.pendingBytes = this.pendingBytes.slice(frameStart)
    return frames
  }

  reset() {
    this.pendingBytes = new Uint8Array(0)
  }
}
