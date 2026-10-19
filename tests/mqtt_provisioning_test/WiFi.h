#pragma once
#include <stdint.h>
class WiFiClient {
 public:
  void setConnectionTimeout(uint32_t) {}
  void stop() {}
};
struct TestSerial {
  template<class T> void print(T) {}
  template<class T> void println(T) {}
};
inline TestSerial Serial;
