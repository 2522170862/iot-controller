#pragma once
#include <stdint.h>
#include <string.h>
#include <map>
#include <string>
#include <vector>

// In-memory double of NVS only; the production storage code is exercised as-is.
class Preferences {
 public:
  inline static std::map<std::string, std::vector<uint8_t>> values;
  inline static std::string failKey;
  bool begin(const char*, bool) { return true; }
  void end() {}
  uint8_t getUChar(const char* key, uint8_t fallback) {
    auto item=values.find(key); return item==values.end() || item->second.empty() ? fallback : item->second[0];
  }
  size_t putUChar(const char* key, uint8_t value) {
    if(failKey==key) return 0; values[key]={value}; return 1;
  }
  size_t getBytesLength(const char* key) { auto item=values.find(key); return item==values.end()?0:item->second.size(); }
  size_t getBytes(const char* key, void* output, size_t length) {
    auto item=values.find(key); if(item==values.end() || item->second.size()>length) return 0;
    memcpy(output,item->second.data(),item->second.size()); return item->second.size();
  }
  size_t putBytes(const char* key, const void* input, size_t length) {
    if(failKey==key) return 0;
    const auto* bytes=static_cast<const uint8_t*>(input); values[key]=std::vector<uint8_t>(bytes,bytes+length); return length;
  }
};
