#include "timer.hpp"

auto Timer::read(uint16_t addr) const -> uint8_t {
  switch (addr) {
  case 0xFF04:
    return counter >> 8; // DIV
  case 0xFF05:
    return tima;
  case 0xFF06:
    return tma;
  case 0xFF07:
    return tac | 0xF8; // upper bits always 1
  default:
    __builtin_unreachable();
  }
  return 0xFF;
}

void Timer::write(uint16_t addr, uint8_t val) {
  switch (addr) {
  case 0xFF04:
    counter = 0; // reset entire divider
    break;

  case 0xFF05:
    tima = val;
    break;

  case 0xFF06:
    tma = val;
    break;

  case 0xFF07:
    tac = val & 0x07;
    break;
  default:
    __builtin_unreachable();
  }
}

void Timer::connect_interrupt_flag(uint8_t *if_reg) { IF = if_reg; }

void Timer::tick(int cycles) {
  for (int i = 0; i < cycles; i++) {
    timer_tick();
  }
}

void Timer::timer_tick() {
  uint16_t prev = counter;
  counter++;

  if ((tac & 0x04) == 0) {
    return;
  }

  int bit = get_timer_bit();

  bool prev_bit = ((prev >> bit) & 1) != 0;
  bool new_bit = ((counter >> bit) & 1) != 0;

  if (prev_bit && !new_bit) {
    increment_tima();
  }
}

auto Timer::get_timer_bit() const -> int {
  switch (tac & 0x03) {
  case 0:
    return 9;
  case 1:
    return 3;
  case 2:
    return 5;
  case 3:
    return 7;
  default:
    return 9;
  }
}

void Timer::increment_tima() {
  if (tima == 0xFF) {
    tima = tma;
    *IF |= 0x04;
  } else {
    tima++;
  }
}
