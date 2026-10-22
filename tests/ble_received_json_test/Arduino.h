#pragma once

#include <string>

struct ReceivedJsonSerial {
  std::string output;
  void print(const char* value) { output += value; }
  void println(const char* value) { output += value; output += '\n'; }
};

inline ReceivedJsonSerial Serial;
