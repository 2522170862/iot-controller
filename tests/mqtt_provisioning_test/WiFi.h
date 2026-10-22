#pragma once
#include <stdint.h>
#include <stdio.h>
#include <string>
#include <vector>
class WiFiClient {
 public:
  void setConnectionTimeout(uint32_t) {}
  void stop() {}
};
struct TestSerial {
  std::string output;
  template<class T> void print(T) {}
  template<class T> void println(T) {}
  template<class... Args> void printf(const char* format, Args... args) {
    const int length = snprintf(nullptr, 0, format, args...);
    if (length < 0) return;
    std::vector<char> buffer(static_cast<size_t>(length) + 1);
    snprintf(buffer.data(), buffer.size(), format, args...);
    output.append(buffer.data(), static_cast<size_t>(length));
  }
};
inline TestSerial Serial;
