#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../WiFiDataSource.cpp"
#include "../../WifiProvisioningCoordinator.cpp"

class Store : public IWifiCredentialStore {
 public:
  int saves = 0;
  WifiCredentialsValue saved = {};
  bool load(WifiCredentialsValue*) override { return false; }
  bool save(const WifiCredentialsValue& value) override {
    ++saves;
    saved = value;
    return true;
  }
  bool clear() override { return true; }
};

BleWifiRequest request(const char* id, const char* password) {
  BleWifiRequest value = {};
  snprintf(value.id, sizeof(value.id), "%s", id);
  snprintf(value.ssid, sizeof(value.ssid), "Target");
  snprintf(value.password, sizeof(value.password), "%s", password);
  return value;
}

void retryDoesNotConsumePreviousFailure(wl_status_t staleStatus) {
  WiFi = {};
  WiFiDataSource wifi;
  Store store;
  WifiProvisioningCoordinator coordinator(&wifi, &store);
  assert(coordinator.start(request("11111111", "wrong-pass"), 100));
  WiFi.rawStatus = staleStatus;
  wifi.poll(15100);
  coordinator.poll(15100);
  assert(!coordinator.active());
  assert(store.saves == 0);
  WifiProvisioningEvent event;
  while (coordinator.takeEvent(&event)) {}

  assert(coordinator.start(request("22222222", "correct-pass"), 16000));
  assert(coordinator.takeEvent(&event));
  wifi.poll(16001);  // The SDK has not delivered a new event yet.
  coordinator.poll(16001);
  assert(coordinator.active() && "retry must not inherit previous failure");
  assert(!coordinator.takeEvent(&event));
  // An in-flight validation must not be restarted by the 10 s reconnect timer.
  const int begins = WiFi.beginCount;
  wifi.poll(27000);
  coordinator.poll(27000);
  assert(coordinator.active());
  assert(WiFi.beginCount == begins);
  WiFi.rawStatus = WL_CONNECTED;
  WiFi.actualSsid = "Target";
  WiFi.ip = "192.168.5.53";
  wifi.poll(28000);
  coordinator.poll(28000);
  assert(!coordinator.active());
  assert(store.saves == 1);
  assert(strcmp(store.saved.password, "correct-pass") == 0);
  assert(coordinator.takeEvent(&event));
  assert(event.kind == WifiProvisioningEventKind::kConnected);
  assert(strcmp(event.id, "22222222") == 0);
  assert(strcmp(event.ip, "192.168.5.53") == 0);
}

void staleConnectionCannotConfirmNewNetwork() {
  WiFi = {};
  WiFiDataSource wifi;
  WifiCredentialsValue old = {};
  snprintf(old.ssid, sizeof(old.ssid), "Old");
  wifi.connect(old, 0);
  WiFi.rawStatus = WL_CONNECTED;
  WiFi.actualSsid = "Old";
  WiFi.ip = "192.168.1.2";
  wifi.poll(10);
  Store store;
  WifiProvisioningCoordinator coordinator(&wifi, &store);
  assert(coordinator.start(request("33333333", "correct-pass"), 100));
  assert(!wifi.readNetwork().connected);
  assert(strcmp(wifi.localIp(), "0.0.0.0") == 0);
  wifi.poll(101);
  coordinator.poll(101);
  assert(coordinator.active());
  assert(store.saves == 0);
  WiFi.actualSsid = "Target";
  WiFi.ip = "0.0.0.0";
  wifi.poll(200);
  coordinator.poll(200);
  assert(coordinator.active());
  WiFi.ip = "192.168.5.53";
  wifi.poll(300);
  coordinator.poll(300);
  assert(store.saves == 1);
  assert(strcmp(wifi.localIp(), "192.168.5.53") == 0);
}

void backgroundAttemptTimesOutAndRecovers(uint32_t started) {
  WiFi = {};
  WiFiDataSource wifi;
  WifiCredentialsValue credentials = {};
  snprintf(credentials.ssid, sizeof(credentials.ssid), "Target");
  wifi.connect(credentials, started);
  WiFi.rawStatus = WL_CONNECT_FAILED;
  wifi.poll(started + 14999U);
  assert(wifi.attemptState() == WifiAttemptState::kConnecting);
  assert(WiFi.beginCount == 1);
  wifi.poll(started + 15000U);
  assert(wifi.attemptState() == WifiAttemptState::kFailed);
  wifi.poll(started + 24999U);
  assert(WiFi.beginCount == 1);
  wifi.poll(started + 25000U);
  assert(wifi.attemptState() == WifiAttemptState::kConnecting);
  assert(WiFi.beginCount == 2);
  WiFi.rawStatus = WL_CONNECTED;
  WiFi.actualSsid = "Target";
  WiFi.ip = "192.168.5.53";
  wifi.poll(started + 26000U);
  assert(wifi.readNetwork().connected);
}

void switchingCancelsAnUnassociatedAttemptBeforeConfiguring() {
  WiFi = {};
  WiFi.driverStarted = true;
  WiFi.connecting = true;
  WiFi.configuredSsid = "Old";
  WiFi.configuredPassword = "old-password";
  WiFiDataSource wifi;
  Store store;
  WifiProvisioningCoordinator coordinator(&wifi, &store);
  assert(coordinator.start(request("44444444", "correct-pass"), 100));
  assert(WiFi.configuredSsid == "Target" && "old association must stop before applying new SSID");
  assert(WiFi.configuredPassword == "correct-pass");
  assert(WiFi.rejectedConfigurations == 0);
  assert(!WiFi.autoReconnect);
  WiFi.actualSsid = WiFi.configuredSsid;
  WiFi.rawStatus = WL_CONNECTED;
  WiFi.ip = "192.168.5.53";
  wifi.poll(200);
  coordinator.poll(200);
  assert(!coordinator.active() && store.saves == 1);
  assert(strcmp(store.saved.password, "correct-pass") == 0);
}

void sameNetworkPasswordChangeCancelsTheOldAuthentication() {
  WiFi = {};
  WiFi.driverStarted = true;
  WiFi.connecting = true;
  WiFi.configuredSsid = "Target";
  WiFi.configuredPassword = "wrong-pass";
  WiFiDataSource wifi;
  WifiCredentialsValue credentials = {};
  snprintf(credentials.ssid, sizeof(credentials.ssid), "Target");
  snprintf(credentials.password, sizeof(credentials.password), "correct-pass");
  wifi.connect(credentials, 100);
  assert(WiFi.configuredPassword == "correct-pass");
  assert(WiFi.rejectedConfigurations == 0);
}

void driverErrorsDoNotPretendAConnectionWasStarted() {
  for (int failure = 0; failure < 3; ++failure) {
    WiFi = {};
    WiFi.stopResult = failure != 0;
    WiFi.startResult = failure != 1;
    WiFi.connectResult = failure != 2;
    WiFiDataSource wifi;
    WifiCredentialsValue credentials = {};
    snprintf(credentials.ssid, sizeof(credentials.ssid), "Target");
    wifi.connect(credentials, 100);
    assert(wifi.attemptState() == WifiAttemptState::kFailed);
    assert(!wifi.readNetwork().connected);
    assert(strcmp(wifi.localIp(), "0.0.0.0") == 0);
    const int attempts = WiFi.beginCount;
    // A failed stop/configure may leave the SDK reporting the old association.
    WiFi.rawStatus = WL_CONNECTED;
    WiFi.actualSsid = "Target";
    WiFi.ip = "192.168.1.2";
    wifi.poll(101);
    assert(wifi.attemptState() == WifiAttemptState::kFailed);
    assert(!wifi.readNetwork().connected);
    assert(WiFi.beginCount == attempts);  // No tight retry loop on driver errors.
  }
}

int main() {
  switchingCancelsAnUnassociatedAttemptBeforeConfiguring();
  sameNetworkPasswordChangeCancelsTheOldAuthentication();
  driverErrorsDoNotPretendAConnectionWasStarted();
  retryDoesNotConsumePreviousFailure(WL_CONNECT_FAILED);
  retryDoesNotConsumePreviousFailure(WL_NO_SSID_AVAIL);
  staleConnectionCannotConfirmNewNetwork();
  backgroundAttemptTimesOutAndRecovers(0);
  backgroundAttemptTimesOutAndRecovers(UINT32_MAX - 5000U);
  puts("PASS: Wi-Fi retry and current-network validation");
}
