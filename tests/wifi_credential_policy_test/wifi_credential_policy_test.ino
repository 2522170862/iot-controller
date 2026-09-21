#include <assert.h>
#include <string.h>

#include "../../WifiCredentialStore.cpp"

namespace {
WifiCredentialsValue makeCredentials(const char* ssid, const char* password) {
  WifiCredentialsValue value = {};
  snprintf(value.ssid, sizeof(value.ssid), "%s", ssid);
  snprintf(value.password, sizeof(value.password), "%s", password);
  return value;
}

void assertValidityRules() {
  assert(!WifiCredentialPolicy::isValid(makeCredentials("", "")));
  assert(WifiCredentialPolicy::isValid(makeCredentials("Lab", "")));

  WifiCredentialsValue maximum = {};
  memset(maximum.ssid, 's', 32);
  maximum.ssid[32] = '\0';
  memset(maximum.password, 'p', 63);
  maximum.password[63] = '\0';
  assert(WifiCredentialPolicy::isValid(maximum));

  WifiCredentialsValue unterminatedSsid = {};
  memset(unterminatedSsid.ssid, 's', sizeof(unterminatedSsid.ssid));
  assert(!WifiCredentialPolicy::isValid(unterminatedSsid));

  WifiCredentialsValue unterminatedPassword = makeCredentials("Lab", "");
  memset(unterminatedPassword.password, 'p',
         sizeof(unterminatedPassword.password));
  assert(!WifiCredentialPolicy::isValid(unterminatedPassword));
}

void assertBootSelection() {
  const WifiCredentialsValue saved = makeCredentials("Saved", "saved-pass");
  const WifiCredentialsValue fallback =
      makeCredentials("Fallback", "fallback-pass");

  WifiCredentialsValue selected = WifiCredentialPolicy::selectBootCredentials(
      true, saved, fallback);
  assert(strcmp(selected.ssid, "Saved") == 0);
  assert(strcmp(selected.password, "saved-pass") == 0);

  selected = WifiCredentialPolicy::selectBootCredentials(false, saved, fallback);
  assert(strcmp(selected.ssid, "Fallback") == 0);

  const WifiCredentialsValue invalidSaved = makeCredentials("", "bad");
  selected = WifiCredentialPolicy::selectBootCredentials(
      true, invalidSaved, fallback);
  assert(strcmp(selected.ssid, "Fallback") == 0);
}

void assertTransactionalSlotSelection() {
  assert(WifiCredentialSlotPolicy::nextSlot(0) == 1);
  assert(WifiCredentialSlotPolicy::nextSlot(1) == 0);
  assert(WifiCredentialSlotPolicy::nextSlot(0xFF) == 0);
  assert(WifiCredentialSlotPolicy::isSlot(0));
  assert(WifiCredentialSlotPolicy::isSlot(1));
  assert(!WifiCredentialSlotPolicy::isSlot(2));
}
}  // namespace

void setup() {
  assertValidityRules();
  assertBootSelection();
  assertTransactionalSlotSelection();
}

void loop() {}
