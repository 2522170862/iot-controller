#include "BleProvisioningService.h"

#include <BLE2902.h>
#include <BLEDevice.h>
#include <BLEServer.h>
#include <BLEUtils.h>
#include <Esp.h>
#include <stdio.h>
#include <string.h>

class BleProvisioningService::ServerCallbacks : public BLEServerCallbacks {
 public:
  explicit ServerCallbacks(BleProvisioningService* owner) : owner_(owner) {}

  void onConnect(BLEServer*) override {
    if (owner_ != nullptr) {
      owner_->onConnected();
    }
  }

  void onDisconnect(BLEServer*) override {
    if (owner_ != nullptr) {
      owner_->onDisconnected();
    }
  }

 private:
  BleProvisioningService* owner_;
};

class BleProvisioningService::RxCallbacks
    : public BLECharacteristicCallbacks {
 public:
  explicit RxCallbacks(BleProvisioningService* owner) : owner_(owner) {}

  void onWrite(BLECharacteristic* characteristic) override {
    if (owner_ == nullptr || characteristic == nullptr) {
      return;
    }
    const String value = characteristic->getValue();
    owner_->onWrite(reinterpret_cast<const uint8_t*>(value.c_str()),
                    value.length());
  }

 private:
  BleProvisioningService* owner_;
};

void BleProvisioningService::begin() {
  const uint64_t mac = ESP.getEfuseMac();
  char deviceName[24] = {};
  snprintf(deviceName, sizeof(deviceName), "IoT-Controller-%02X%02X",
           static_cast<unsigned>((mac >> 8) & 0xFF),
           static_cast<unsigned>(mac & 0xFF));

  BLEDevice::init(deviceName);
  server_ = BLEDevice::createServer();
  serverCallbacks_ = new ServerCallbacks(this);
  server_->setCallbacks(serverCallbacks_);

  BLEService* service =
      server_->createService(BleProvisioningUuids::kService);
  txCharacteristic_ = service->createCharacteristic(
      BleProvisioningUuids::kTx, BLECharacteristic::PROPERTY_NOTIFY);
  txCharacteristic_->addDescriptor(new BLE2902());

  BLECharacteristic* rxCharacteristic = service->createCharacteristic(
      BleProvisioningUuids::kRx,
      BLECharacteristic::PROPERTY_WRITE |
          BLECharacteristic::PROPERTY_WRITE_NR);
  rxCallbacks_ = new RxCallbacks(this);
  rxCharacteristic->setCallbacks(rxCallbacks_);

  service->start();
  BLEAdvertising* advertising = server_->getAdvertising();
  advertising->addServiceUUID(BleProvisioningUuids::kService);
  advertising->setScanResponse(true);
  advertising->start();

  Serial.print("BLE provisioning advertising: ");
  Serial.println(deviceName);
}

void BleProvisioningService::poll(uint32_t nowMs) {
  if (advertisingRestartPending_ &&
      nowMs - disconnectedMs_ >= kAdvertisingRestartDelayMs) {
    advertisingRestartPending_ = false;
    if (server_ != nullptr) {
      server_->startAdvertising();
      Serial.println("BLE provisioning advertising restarted");
    }
  }

  if (!connected_ || txCharacteristic_ == nullptr) {
    return;
  }

  char chunk[kBleProvisioningMaxChunkBytes] = {};
  size_t chunkLength = 0;
  bool frameCompleted = false;
  portENTER_CRITICAL(&queueMux_);
  if (outgoingSize_ > 0) {
    const char* frame = outgoing_[outgoingHead_];
    const size_t remaining = strlen(frame) - outgoingOffset_;
    chunkLength = remaining < sizeof(chunk) ? remaining : sizeof(chunk);
    memcpy(chunk, frame + outgoingOffset_, chunkLength);
    outgoingOffset_ += chunkLength;
    frameCompleted = outgoingOffset_ >= strlen(frame);
    if (frameCompleted) {
      outgoingHead_ = (outgoingHead_ + 1) % kOutgoingCapacity;
      --outgoingSize_;
      outgoingOffset_ = 0;
    }
  }
  portEXIT_CRITICAL(&queueMux_);

  if (chunkLength > 0) {
    txCharacteristic_->setValue(
        reinterpret_cast<const uint8_t*>(chunk), chunkLength);
    txCharacteristic_->notify();
  }
}

bool BleProvisioningService::connected() const { return connected_; }

bool BleProvisioningService::takeRequest(BleWifiRequest* output) {
  portENTER_CRITICAL(&queueMux_);
  const bool popped = requests_.pop(output);
  portEXIT_CRITICAL(&queueMux_);
  return popped;
}

bool BleProvisioningService::enqueueEvent(
    const WifiProvisioningEvent& event) {
  const char* eventName = "error";
  if (event.kind == WifiProvisioningEventKind::kConnecting) {
    eventName = "wifi_connecting";
  } else if (event.kind == WifiProvisioningEventKind::kConnected) {
    eventName = "wifi_connected";
  } else if (event.kind == WifiProvisioningEventKind::kFailed) {
    eventName = "wifi_failed";
  }

  char frame[kOutgoingFrameCapacity] = {};
  if (!BleProvisioningProtocol::makeEvent(
          event.id, eventName,
          event.reason[0] == '\0' ? nullptr : event.reason,
          event.ip[0] == '\0' ? nullptr : event.ip, frame, sizeof(frame))) {
    return false;
  }
  return enqueueFrame(frame);
}

void BleProvisioningService::onConnected() {
  connected_ = true;
  advertisingRestartPending_ = false;
  Serial.println("BLE provisioning client connected");
}

void BleProvisioningService::onDisconnected() {
  connected_ = false;
  disconnectedMs_ = millis();
  advertisingRestartPending_ = true;
  clearOutgoing();
  Serial.println("BLE provisioning client disconnected");
}

void BleProvisioningService::onWrite(const uint8_t* data, size_t length) {
  BleWifiRequest request = {};
  const BleProtocolResult result = assembler_.append(data, length, &request);
  if (result == BleProtocolResult::kNeedMore) {
    return;
  }
  if (result == BleProtocolResult::kOk) {
    portENTER_CRITICAL(&queueMux_);
    const bool queued = requests_.push(request);
    portEXIT_CRITICAL(&queueMux_);
    if (!queued) {
      enqueueProtocolError(request.id, "busy");
    }
    return;
  }

  const char* reason = "invalid_request";
  if (result == BleProtocolResult::kInvalidJson) {
    reason = "invalid_json";
  } else if (result == BleProtocolResult::kInvalidSsid) {
    reason = "invalid_ssid";
  } else if (result == BleProtocolResult::kInvalidPassword) {
    reason = "invalid_password";
  } else if (result == BleProtocolResult::kMessageTooLong) {
    reason = "message_too_long";
  }
  enqueueProtocolError("00000000", reason);
}

bool BleProvisioningService::enqueueFrame(const char* frame) {
  if (frame == nullptr || strlen(frame) >= kOutgoingFrameCapacity) {
    return false;
  }
  portENTER_CRITICAL(&queueMux_);
  if (outgoingSize_ >= kOutgoingCapacity) {
    portEXIT_CRITICAL(&queueMux_);
    return false;
  }
  const uint8_t tail = (outgoingHead_ + outgoingSize_) % kOutgoingCapacity;
  snprintf(outgoing_[tail], sizeof(outgoing_[tail]), "%s", frame);
  ++outgoingSize_;
  portEXIT_CRITICAL(&queueMux_);
  return true;
}

bool BleProvisioningService::enqueueProtocolError(const char* id,
                                                  const char* reason) {
  char frame[kOutgoingFrameCapacity] = {};
  if (!BleProvisioningProtocol::makeEvent(id, "error", reason, nullptr,
                                          frame, sizeof(frame))) {
    return false;
  }
  return enqueueFrame(frame);
}

void BleProvisioningService::clearOutgoing() {
  portENTER_CRITICAL(&queueMux_);
  outgoingHead_ = 0;
  outgoingSize_ = 0;
  outgoingOffset_ = 0;
  portEXIT_CRITICAL(&queueMux_);
}
