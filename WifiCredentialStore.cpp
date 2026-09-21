#include "WifiCredentialStore.h"

#include <Preferences.h>
#include <stddef.h>
#include <string.h>

namespace {
constexpr char kNamespace[] = "iot_wifi";
constexpr char kValidKey[] = "valid";
constexpr char kSsidKey[] = "ssid";
constexpr char kPasswordKey[] = "password";
constexpr char kActiveSlotKey[] = "active_slot";
constexpr uint8_t kNoActiveSlot = 0xFF;
constexpr char kSlotValidKeys[2][9] = {"s0_valid", "s1_valid"};
constexpr char kSlotSsidKeys[2][8] = {"s0_ssid", "s1_ssid"};
constexpr char kSlotPasswordKeys[2][8] = {"s0_pass", "s1_pass"};

bool readCredentials(Preferences& preferences, const char* validKey,
                     const char* ssidKey, const char* passwordKey,
                     WifiCredentialsValue* output) {
  if (!preferences.getBool(validKey, false)) {
    return false;
  }
  *output = {};
  preferences.getString(ssidKey, output->ssid, sizeof(output->ssid));
  preferences.getString(passwordKey, output->password,
                        sizeof(output->password));
  if (!WifiCredentialPolicy::isValid(*output)) {
    *output = {};
    return false;
  }
  return true;
}

bool readSlot(Preferences& preferences, uint8_t slot,
              WifiCredentialsValue* output) {
  if (!WifiCredentialSlotPolicy::isSlot(slot)) {
    return false;
  }
  return readCredentials(preferences, kSlotValidKeys[slot],
                         kSlotSsidKeys[slot], kSlotPasswordKeys[slot], output);
}
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

bool WifiCredentialSlotPolicy::isSlot(uint8_t slot) { return slot < 2; }

uint8_t WifiCredentialSlotPolicy::nextSlot(uint8_t activeSlot) {
  return activeSlot == 0 ? 1 : 0;
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
  const uint8_t activeSlot =
      preferences.getUChar(kActiveSlotKey, kNoActiveSlot);
  bool loaded = false;
  if (WifiCredentialSlotPolicy::isSlot(activeSlot)) {
    loaded = readSlot(preferences, activeSlot, output);
    if (!loaded) {
      loaded = readSlot(preferences,
                        WifiCredentialSlotPolicy::nextSlot(activeSlot), output);
    }
    if (!loaded) {
      loaded = readCredentials(preferences, kValidKey, kSsidKey, kPasswordKey,
                               output);
    }
  } else {
    loaded = readCredentials(preferences, kValidKey, kSsidKey, kPasswordKey,
                             output);
  }
  preferences.end();

  if (!loaded) {
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
  const uint8_t activeSlot =
      preferences.getUChar(kActiveSlotKey, kNoActiveSlot);
  const uint8_t targetSlot =
      WifiCredentialSlotPolicy::nextSlot(activeSlot);
  const bool invalidated =
      preferences.putBool(kSlotValidKeys[targetSlot], false) == 1;
  const size_t ssidWritten =
      invalidated
          ? preferences.putString(kSlotSsidKeys[targetSlot], credentials.ssid)
          : 0;
  const size_t passwordWritten =
      invalidated ? preferences.putString(kSlotPasswordKeys[targetSlot],
                                          credentials.password)
                  : 0;
  const bool stringsWritten =
      invalidated && ssidWritten == strlen(credentials.ssid) &&
      passwordWritten == strlen(credentials.password);
  const bool slotCommitted =
      stringsWritten &&
      preferences.putBool(kSlotValidKeys[targetSlot], true) == 1;
  const bool activeCommitted =
      slotCommitted &&
      preferences.putUChar(kActiveSlotKey, targetSlot) == 1;
  preferences.end();
  return activeCommitted;
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
