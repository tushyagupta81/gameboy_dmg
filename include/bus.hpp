#pragma once

#include "memory.hpp"
#include <cstdint>

class Bus {
private:
  Memory mem;

public:
  [[nodiscard]] auto read(uint16_t addr) const -> uint8_t;
  void write(uint16_t addr, uint8_t value);
};
