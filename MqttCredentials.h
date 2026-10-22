#pragma once

#include <stdint.h>

namespace MqttCredentials {
// constexpr char kServer[] = "121.40.47.37";
// constexpr uint16_t kPort = 1883;
// constexpr char kUserName[] = "user-client";
// constexpr char kPassword[] = "123456";
constexpr char kServer[] = "charge.jy400.cn";
constexpr uint16_t kPort = 58613;
constexpr char kUserName[] = "HscIot";
constexpr char kPassword[] = "Ahau&1234";
constexpr char kSubTopic[] = "LoTS2C/";
constexpr char kPubTopic[] = "LoTC2S/";
constexpr uint8_t kSubQos = 0;
// Client ID is generated from the hardware MAC using DeviceIdentity.h.
}  // namespace MqttCredentials
