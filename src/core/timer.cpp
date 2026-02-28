#include "timer.hpp"
#include <cstdint>

auto Timer::read(uint16_t addr) -> uint8_t {
  switch (addr) {
  case 0xFF04:
    return (div >> 8);
  case 0xFF05:
    return tima;
  case 0xFF06:
    return tma;
  case 0xFF07:
    return tac;
  default:
    __builtin_unreachable();
  }
}

void Timer::write(uint16_t addr, uint8_t val) {
  switch (addr) {
  case 0xFF04:
    div = 0;
  case 0xFF05:
    tima = val;
  case 0xFF06:
    tma = val;
  case 0xFF07:
    tac = val;
  default:
    __builtin_unreachable();
  }
}

auto Timer::get_bit(uint8_t clock_select) -> uint8_t {
  switch (clock_select) {
  case 0:
    return 7;
  case 1:
    return 1;
  case 2:
    return 3;
  case 3:
    return 5;
  default:
    __builtin_unreachable();
  }
}

void Timer::tick() {
  uint16_t old_div = div;
  div++;

  if(--tima_delay == 0) {
    tima = tma;
    enable_interrupt(2);
  }

  uint8_t inc_enable = tac & 0x04;
  uint8_t clock_select = tac & 0x03;

  uint8_t bit = get_bit(clock_select);
  uint8_t old_div_bit = (old_div >> bit) & 0x1;
  uint8_t new_div_bit = (div >> bit) & 0x1;
  if ((old_div_bit & inc_enable) == 1 && (new_div_bit & inc_enable) == 0) {
    if (tima == 0xFF) {
      tima = 0;
      tima_overflow = true;
      tima_delay = 1;
    } else {
      tima++;
    }
  }
}
