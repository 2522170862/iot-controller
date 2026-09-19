#pragma once

#include <stdint.h>

#include "DashboardTypes.h"
#include "WifiCredentialStore.h"

enum class WifiAttemptState : uint8_t {
  kIdle,
  kConnecting,
  kConnected,
  kFailed,
};

class IWifiConnection {
 public:
  virtual ~IWifiConnection() = default;
  virtual void connect(const WifiCredentialsValue& credentials,
                       uint32_t nowMs) = 0;
  virtual WifiCredentialsValue currentCredentials() const = 0;
  virtual WifiAttemptState attemptState() const = 0;
  virtual const char* localIp() const = 0;
};

class WiFiDataSource : public IWifiConnection {
 public:
  WiFiDataSource();

  void begin();
  void begin(const WifiCredentialsValue& credentials);
  void poll(uint32_t nowMs);
  NetworkStatus readNetwork() const;
  void connect(const WifiCredentialsValue& credentials,
               uint32_t nowMs) override;
  WifiCredentialsValue currentCredentials() const override;
  WifiAttemptState attemptState() const override;
  const char* localIp() const override;

 private:
  static constexpr uint32_t kReconnectIntervalMs = 10000;

  bool connected_;
  uint32_t lastAttemptMs_;
  char localIp_[16];
  WifiCredentialsValue currentCredentials_;
  WifiAttemptState attemptState_;

  void startConnection(uint32_t nowMs);
  void setLocalIp(const char* value);
};
