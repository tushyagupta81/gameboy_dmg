#include "timer.hpp"
#include <cstdint>

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
  case 0xFF04: {
    bool enabled = (tac & 0x04) != 0;
    int bit = timer_bit_from_tac(tac);

    bool old_bit = ((counter >> bit) & 1) != 0;
    bool old_input = enabled && old_bit;

    counter = 0;

    bool new_bit = ((counter >> bit) & 1) != 0; // usually 0
    bool new_input = enabled && new_bit;

    if (old_input && !new_input) {
      increment_tima();
    }

    break;
  }

  case 0xFF05: {
    if (time_reloading_now) {
      break;
    }
    if (tima_reload_pending) {
      tima_reload_pending = false;
      tima_delay = 0;
    }
    tima = val;
    break;
  }

  case 0xFF06:
    tma = val;
    if (time_reloading_now) {
      tima = tma;
    }
    break;

  case 0xFF07: {
    uint8_t old_tac = tac;

    bool old_enabled = (old_tac & 0x04) != 0;
    int old_bit = timer_bit_from_tac(old_tac);
    bool old_input = old_enabled && (((counter >> old_bit) & 1) != 0);

    uint8_t new_tac = (val & 0x07); // internal form
    bool new_enabled = (new_tac & 0x04) != 0;
    int new_bit = timer_bit_from_tac(new_tac);
    bool new_input = new_enabled && (((counter >> new_bit) & 1) != 0);

    tac = new_tac;

    if (old_input && !new_input) {
      increment_tima();
    }

    break;
  }
  default:
    __builtin_unreachable();
  }
}

void Timer::connect_interrupt_flag(uint8_t *if_reg) { IF = if_reg; }

void Timer::tick(int cycles) {
  for (int i = 0; i < cycles; i++) {
    time_reloading_now = false;
    if (tima_reload_pending) {
      tima_delay--;
      if (tima_delay == 0) {
        time_reloading_now = true;
        tima = tma;
        *IF |= 0x04;
        tima_reload_pending = false;
      }
    }
    timer_tick();
  }
}

void Timer::timer_tick() {
  uint16_t prev = counter;
  counter++;

  if ((tac & 0x04) == 0) {
    return;
  }

  int bit = timer_bit_from_tac(tac);

  bool prev_bit = ((prev >> bit) & 1) != 0;
  bool new_bit = ((counter >> bit) & 1) != 0;

  if (prev_bit && !new_bit) {
    increment_tima();
  }
}

auto Timer::timer_bit_from_tac(uint8_t tac_val) const -> int {
  switch (tac_val & 0x03) {
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
  if (tima_reload_pending) {
    return;
  }
  if (tima == 0xFF) {
    tima = 0;
    tima_reload_pending = true;
    tima_delay = 4;
  } else {
    tima++;
  }
}

void Timer::write_div_raw(uint8_t val) {
  counter = static_cast<uint16_t>(val) << 8;
}
