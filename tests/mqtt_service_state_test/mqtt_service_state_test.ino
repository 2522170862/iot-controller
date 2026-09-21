#include <assert.h>

#include <PubSubClient.h>

#include "../../MqttConnectionState.h"
#include "../../MqttMessageQueue.cpp"
#include "../../MqttService.cpp"

constexpr bool connectionEventsAreEdgeTriggered() {
  MqttConnectionState state;
  return state.observeConnection(false) == MqttConnectionEvent::kNone &&
         state.observeConnection(true) == MqttConnectionEvent::kConnected &&
         state.observeConnection(true) == MqttConnectionEvent::kNone &&
         state.observeConnection(false) == MqttConnectionEvent::kDisconnected &&
         state.observeConnection(false) == MqttConnectionEvent::kNone;
}

static_assert(connectionEventsAreEdgeTriggered());

void setup() {
  MqttConnectionState state;
  assert(!state.shouldAttempt(0, false));
  assert(state.shouldAttempt(0, true));
  state.recordAttempt(0);
  assert(!state.shouldAttempt(4999, true));
  assert(state.shouldAttempt(5000, true));
}

void loop() {}
