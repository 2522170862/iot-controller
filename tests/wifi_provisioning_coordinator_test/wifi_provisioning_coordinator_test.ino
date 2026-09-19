#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "../../WifiProvisioningCoordinator.cpp"

namespace {
WifiCredentialsValue makeCredentials(const char* ssid, const char* password) {
  WifiCredentialsValue value = {};
  snprintf(value.ssid, sizeof(value.ssid), "%s", ssid);
  snprintf(value.password, sizeof(value.password), "%s", password);
  return value;
}

BleWifiRequest makeRequest(const char* id, const char* ssid,
                           const char* password) {
  BleWifiRequest request = {};
  snprintf(request.id, sizeof(request.id), "%s", id);
  snprintf(request.ssid, sizeof(request.ssid), "%s", ssid);
  snprintf(request.password, sizeof(request.password), "%s", password);
  return request;
}

class FakeWifiConnection : public IWifiConnection {
 public:
  explicit FakeWifiConnection(const WifiCredentialsValue& initial)
      : current_(initial) {}

  void connect(const WifiCredentialsValue& credentials,
               uint32_t nowMs) override {
    current_ = credentials;
    lastConnectMs = nowMs;
    ++connectCount;
    state = WifiAttemptState::kConnecting;
  }

  WifiCredentialsValue currentCredentials() const override {
    return current_;
  }

  WifiAttemptState attemptState() const override { return state; }
  const char* localIp() const override { return ip; }

  WifiAttemptState state = WifiAttemptState::kConnected;
  uint32_t lastConnectMs = 0;
  int connectCount = 0;
  const char* ip = "192.168.1.9";

 private:
  WifiCredentialsValue current_ = {};
};

class FakeCredentialStore : public IWifiCredentialStore {
 public:
  bool load(WifiCredentialsValue*) override { return false; }
  bool save(const WifiCredentialsValue& credentials) override {
    ++saveCount;
    saved = credentials;
    return saveResult;
  }
  bool clear() override { return true; }

  bool saveResult = true;
  int saveCount = 0;
  WifiCredentialsValue saved = {};
};

WifiProvisioningEvent takeEvent(WifiProvisioningCoordinator& coordinator) {
  WifiProvisioningEvent event = {};
  assert(coordinator.takeEvent(&event));
  return event;
}

void assertSuccessSavesCandidate() {
  FakeWifiConnection wifi(makeCredentials("Old", "old-pass"));
  FakeCredentialStore store;
  WifiProvisioningCoordinator coordinator(&wifi, &store);
  const BleWifiRequest request =
      makeRequest("11111111", "New", "new-pass");

  assert(coordinator.start(request, 100));
  assert(coordinator.active());
  assert(wifi.connectCount == 1);
  WifiProvisioningEvent event = takeEvent(coordinator);
  assert(event.kind == WifiProvisioningEventKind::kConnecting);
  assert(strcmp(event.id, "11111111") == 0);

  wifi.state = WifiAttemptState::kConnected;
  coordinator.poll(200);
  assert(!coordinator.active());
  assert(store.saveCount == 1);
  assert(strcmp(store.saved.ssid, "New") == 0);
  event = takeEvent(coordinator);
  assert(event.kind == WifiProvisioningEventKind::kConnected);
  assert(strcmp(event.ip, "192.168.1.9") == 0);
}

void assertFailureRestoresPreviousNetwork() {
  FakeWifiConnection wifi(makeCredentials("Old", "old-pass"));
  FakeCredentialStore store;
  WifiProvisioningCoordinator coordinator(&wifi, &store);
  assert(coordinator.start(makeRequest("22222222", "Bad", "bad-pass"),
                           100));
  takeEvent(coordinator);

  wifi.state = WifiAttemptState::kFailed;
  coordinator.poll(300);
  assert(!coordinator.active());
  assert(store.saveCount == 0);
  assert(wifi.connectCount == 2);
  assert(strcmp(wifi.currentCredentials().ssid, "Old") == 0);
  const WifiProvisioningEvent event = takeEvent(coordinator);
  assert(event.kind == WifiProvisioningEventKind::kFailed);
  assert(strcmp(event.reason, "connection_failed") == 0);
}

void assertTimeoutRestoresPreviousNetwork() {
  FakeWifiConnection wifi(makeCredentials("Old", "old-pass"));
  FakeCredentialStore store;
  WifiProvisioningCoordinator coordinator(&wifi, &store);
  assert(coordinator.start(makeRequest("33333333", "Slow", "pass"), 100));
  takeEvent(coordinator);

  coordinator.poll(15099);
  assert(coordinator.active());
  coordinator.poll(15100);
  assert(!coordinator.active());
  assert(strcmp(wifi.currentCredentials().ssid, "Old") == 0);
  const WifiProvisioningEvent event = takeEvent(coordinator);
  assert(strcmp(event.reason, "timeout") == 0);
}

void assertBusyRequestIsRejected() {
  FakeWifiConnection wifi(makeCredentials("Old", "old-pass"));
  FakeCredentialStore store;
  WifiProvisioningCoordinator coordinator(&wifi, &store);
  assert(coordinator.start(makeRequest("44444444", "One", "pass"), 0));
  assert(!coordinator.start(makeRequest("55555555", "Two", "pass"), 1));
  assert(wifi.connectCount == 1);
}

void assertStorageFailureRollsBack() {
  FakeWifiConnection wifi(makeCredentials("Old", "old-pass"));
  FakeCredentialStore store;
  store.saveResult = false;
  WifiProvisioningCoordinator coordinator(&wifi, &store);
  assert(coordinator.start(makeRequest("66666666", "New", "pass"), 20));
  takeEvent(coordinator);

  wifi.state = WifiAttemptState::kConnected;
  coordinator.poll(30);
  assert(!coordinator.active());
  assert(wifi.connectCount == 2);
  assert(strcmp(wifi.currentCredentials().ssid, "Old") == 0);
  const WifiProvisioningEvent event = takeEvent(coordinator);
  assert(event.kind == WifiProvisioningEventKind::kFailed);
  assert(strcmp(event.reason, "storage_failed") == 0);
}
}  // namespace

void setup() {
  assertSuccessSavesCandidate();
  assertFailureRestoresPreviousNetwork();
  assertTimeoutRestoresPreviousNetwork();
  assertBusyRequestIsRejected();
  assertStorageFailureRollsBack();
}

void loop() {}
