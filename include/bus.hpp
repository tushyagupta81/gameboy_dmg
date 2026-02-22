#pragma once

#include "memory.hpp"
#include "ppu.hpp"
#include "timer.hpp"
#include <cstdint>
#include <vector>

class Bus {
private:
  Memory mem{};
  Timer timer;
  PPU ppu;

  [[nodiscard]] auto is_unused_io(uint16_t) const -> bool;

public:
  Bus();
  [[nodiscard]] auto read(uint16_t addr) const -> uint8_t;
  void write(uint16_t addr, uint8_t value);
  void load_rom(const std::vector<uint8_t> &rom);
  void timer_tick();

  void request_interrupt(uint8_t);

  void reset_DIV();
};
