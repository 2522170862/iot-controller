#include "WifiCredentialStore.h"

#include <Preferences.h>
#include <stddef.h>
#include <string.h>

namespace {
constexpr char kNamespace[] = "iot_wifi";
constexpr char kValidKey[] = "valid";
constexpr char kSsidKey[] = "ssid";
constexpr char kPasswordKey[] = "password";
}  // namespace

bool WifiCredentialPolicy::isValid(
    const WifiCredentialsValue& credentials) {
  const size_t ssidLength = strnlen(credentials.ssid, sizeof(credentials.ssid));
  const size_t passwordLength =
      strnlen(credentials.password, sizeof(credentials.password));
  return ssidLength > 0 && ssidLength <= 32 &&
         passwordLength <= 63;
}

WifiCredentialsValue WifiCredentialPolicy::selectBootCredentials(
    bool savedPresent, const WifiCredentialsValue& saved,
    const WifiCredentialsValue& fallback) {
  if (savedPresent && isValid(saved)) {
    return saved;
  }
  return fallback;
}

bool WifiCredentialStore::load(WifiCredentialsValue* output) {
  if (output == nullptr) {
    return false;
  }
  *output = {};

  Preferences preferences;
  if (!preferences.begin(kNamespace, true)) {
    return false;
  }
  const bool valid = preferences.getBool(kValidKey, false);
  if (valid) {
    preferences.getString(kSsidKey, output->ssid, sizeof(output->ssid));
    preferences.getString(kPasswordKey, output->password,
                          sizeof(output->password));
  }
  preferences.end();

  if (!valid || !WifiCredentialPolicy::isValid(*output)) {
    *output = {};
    return false;
  }
  return true;
}

bool WifiCredentialStore::save(const WifiCredentialsValue& credentials) {
  if (!WifiCredentialPolicy::isValid(credentials)) {
    return false;
  }

  Preferences preferences;
  if (!preferences.begin(kNamespace, false)) {
    return false;
  }
  preferences.putBool(kValidKey, false);
  const size_t ssidWritten = preferences.putString(kSsidKey, credentials.ssid);
  const size_t passwordWritten =
      preferences.putString(kPasswordKey, credentials.password);
  const bool stringsWritten =
      ssidWritten == strlen(credentials.ssid) &&
      passwordWritten == strlen(credentials.password);
  const bool validWritten =
      stringsWritten && preferences.putBool(kValidKey, true) == 1;
  preferences.end();
  return validWritten;
}

bool WifiCredentialStore::clear() {
  Preferences preferences;
  if (!preferences.begin(kNamespace, false)) {
    return false;
  }
  const bool cleared = preferences.clear();
  preferences.end();
  return cleared;
}
