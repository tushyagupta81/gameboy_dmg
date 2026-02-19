#pragma once

#include <cstdint>

class Timer {
public:
  void tick(int cycles);

  [[nodiscard]] auto read(uint16_t addr) const -> uint8_t;
  void write(uint16_t addr, uint8_t value);

  void connect_interrupt_flag(uint8_t *if_reg);

private:
  uint16_t counter = 0;

  uint8_t tima = 0;
  uint8_t tma = 0;
  uint8_t tac = 0;

  uint8_t *IF = nullptr;

  void timer_tick();
  void increment_tima();
  [[nodiscard]] auto get_timer_bit() const -> int;
};
