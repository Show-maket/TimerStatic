#include "Arduino.h"

HostSerialPort Serial;

namespace {
const std::chrono::steady_clock::time_point kStart = std::chrono::steady_clock::now();
}

unsigned long millis()
{
  const auto elapsed = std::chrono::steady_clock::now() - kStart;
  return static_cast<unsigned long>(
      std::chrono::duration_cast<std::chrono::milliseconds>(elapsed).count());
}

unsigned long micros()
{
  const auto elapsed = std::chrono::steady_clock::now() - kStart;
  return static_cast<unsigned long>(
      std::chrono::duration_cast<std::chrono::microseconds>(elapsed).count());
}

void delay(unsigned long milliseconds)
{
  const auto until = std::chrono::steady_clock::now() +
                     std::chrono::milliseconds(milliseconds);
  while (std::chrono::steady_clock::now() < until) {}
}

void yield()
{
  // The single-threaded reference runner has nothing else to schedule.
}
