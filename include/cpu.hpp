#pragma once

#include "bus.hpp"
#include "memory.hpp"
#include <array>
#include <cstdint>
#include <iostream>

class CPU {
public:
  using Instr = uint8_t (CPU::*)();

  CPU() {
    init_tables();
    std::cout << "Initializing CPU\n";
  }

  auto step() -> uint8_t;

private:
  Register reg{};
  Bus bus;

  void init_tables();

  auto nop() -> uint8_t;

  auto ld_r_r() -> uint8_t;
  auto ld_r_d8() -> uint8_t;
  auto inc_r() -> uint8_t;
  auto dec_r() -> uint8_t;

  void write_reg(uint8_t dst, uint8_t val);
  auto read_reg(uint8_t src) -> uint8_t;

  std::array<Instr, 256> cb_table{}, op_table{};
};
