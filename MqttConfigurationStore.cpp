#include "MqttConfigurationStore.h"

#include <Preferences.h>
#include <string.h>

namespace {
constexpr char kNamespace[] = "iot_mqtt";
constexpr char kKeys[2][5] = {"cfg0", "cfg1"};
struct Record {
  uint32_t version = 1;
  MqttConfiguration value;
};
bool readSlot(Preferences& storage, uint8_t slot, MqttConfiguration* output) {
  if (slot > 1 || storage.getBytesLength(kKeys[slot]) != sizeof(Record)) return false;
  Record record;
  if (storage.getBytes(kKeys[slot], &record, sizeof(record)) != sizeof(record) ||
      record.version != 1 || !validMqttConfiguration(record.value)) return false;
  *output = record.value;
  return true;
}
}  // namespace

bool MqttConfigurationStore::load(MqttConfiguration* output) {
  if (output == nullptr) return false;
  *output = {};
  Preferences storage;
  if (!storage.begin(kNamespace, true)) return false;
  const uint8_t slot = storage.getUChar("active", 0xFF);
  const bool loaded = slot < 2 && (readSlot(storage, slot, output) || readSlot(storage, 1 - slot, output));
  storage.end();
  return loaded;
}

bool MqttConfigurationStore::save(const MqttConfiguration& value) {
  if (!validMqttConfiguration(value)) return false;
  Preferences storage;
  if (!storage.begin(kNamespace, false)) return false;
  const uint8_t slot = storage.getUChar("active", 0xFF) == 0 ? 1 : 0;
  Record record;
  record.value = value;
  const bool written = storage.putBytes(kKeys[slot], &record, sizeof(record)) == sizeof(record);
  MqttConfiguration verified;
  const bool valid = written && readSlot(storage, slot, &verified) &&
      memcmp(&verified, &record.value, sizeof(verified)) == 0;
  const bool committed = valid && storage.putUChar("active", slot) == 1;
  storage.end();
  return committed;
}
