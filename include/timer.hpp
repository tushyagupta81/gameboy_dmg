#pragma once
#include <cstdint>
class Timer {
private:
  uint16_t div;
  uint8_t tima;
  uint8_t tma;
  uint8_t tac;

  bool tima_overflow;
  uint8_t tima_delay;

  auto get_bit(uint8_t) -> uint8_t;

  void (*enable_interrupt)(uint8_t);

public:
  auto read(uint16_t) -> uint8_t;
  void write(uint16_t, uint8_t);
  void tick();
};
