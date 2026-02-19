#pragma once

#include "memory.hpp"
#include "timer.hpp"
#include <cstdint>
#include <vector>

class Bus {
private:
  Memory mem{};
  Timer timer;

public:
  Bus();
  [[nodiscard]] auto read(uint16_t addr) const -> uint8_t;
  void write(uint16_t addr, uint8_t value);
  void load_rom(const std::vector<uint8_t> &rom);
  void timer_tick(int cycles);
};
