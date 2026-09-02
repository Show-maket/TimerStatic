#pragma once

#include <chrono>
#include <cstdint>
#include <iostream>

#define F(value) value

class HostSerialPort {
public:
  void begin(unsigned long) {}
  explicit operator bool() const { return true; }

  template <typename T>
  void print(const T& value) { std::cout << value; }

  void print(uint8_t value) { std::cout << static_cast<unsigned int>(value); }
  void print(int8_t value) { std::cout << static_cast<int>(value); }

  template <typename T>
  void println(const T& value)
  {
    print(value);
    std::cout << '\n';
  }

  void println() { std::cout << '\n'; }
  void flush() { std::cout.flush(); }
};

extern HostSerialPort Serial;

unsigned long millis();
unsigned long micros();
void delay(unsigned long milliseconds);
void yield();
