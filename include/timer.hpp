#pragma once

#include <cstdint>

class Timer {
public:
  void tick(int cycles);

  [[nodiscard]] auto read(uint16_t addr) const -> uint8_t;
  void write(uint16_t addr, uint8_t value);

  void connect_interrupt_flag(uint8_t *if_reg);

  void write_div_raw(uint8_t);

private:
  uint16_t counter = 0;

  uint8_t tima = 0;
  uint8_t tma = 0;
  uint8_t tac = 0;

  bool tima_reload_pending = false;
  int tima_delay = 0;
  bool time_reloading_now = false;

  uint8_t *IF = nullptr;

  void timer_tick();
  void increment_tima();
  [[nodiscard]] auto timer_bit_from_tac(uint8_t) const -> int;
};
