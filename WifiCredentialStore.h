#pragma once

#include <stdint.h>

struct WifiCredentialsValue {
  char ssid[33] = {};
  char password[64] = {};
};

class WifiCredentialPolicy {
 public:
  static bool isValid(const WifiCredentialsValue& credentials);
  static WifiCredentialsValue selectBootCredentials(
      bool savedPresent, const WifiCredentialsValue& saved,
      const WifiCredentialsValue& fallback);
};

class IWifiCredentialStore {
 public:
  virtual ~IWifiCredentialStore() = default;
  virtual bool load(WifiCredentialsValue* output) = 0;
  virtual bool save(const WifiCredentialsValue& credentials) = 0;
  virtual bool clear() = 0;
};

class WifiCredentialStore : public IWifiCredentialStore {
 public:
  bool load(WifiCredentialsValue* output) override;
  bool save(const WifiCredentialsValue& credentials) override;
  bool clear() override;
};
