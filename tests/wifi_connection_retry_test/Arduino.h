#pragma once
#include <stdint.h>
#include <string>
using String = std::string;
inline uint32_t millis() { return 0; }
struct TestSerial {
  template <typename T> void print(T) {}
  template <typename T> void println(T) {}
};
inline TestSerial Serial;
