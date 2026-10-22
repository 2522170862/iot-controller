#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

namespace DeviceIdentity {
constexpr size_t kNameCapacity = 17;

// BLE and MQTT share the complete factory MAC, not a truncated suffix.
// ESP.getEfuseMac() places the first MAC byte in the least significant byte.
inline void formatName(uint64_t mac, char (&name)[kNameCapacity]) {
  snprintf(name, sizeof(name), "Iot-%02x%02x%02x%02x%02x%02x",
           static_cast<unsigned>(mac & 0xFF),
           static_cast<unsigned>((mac >> 8) & 0xFF),
           static_cast<unsigned>((mac >> 16) & 0xFF),
           static_cast<unsigned>((mac >> 24) & 0xFF),
           static_cast<unsigned>((mac >> 32) & 0xFF),
           static_cast<unsigned>((mac >> 40) & 0xFF));
}
}  // namespace DeviceIdentity
