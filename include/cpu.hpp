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

  auto ld_r8_r8() -> uint8_t;
  auto ld_r8_i8() -> uint8_t;
  auto ld_mem_a() -> uint8_t;
  auto ld_a_mem() -> uint8_t;
  auto ld_i16_sp() -> uint8_t;
  auto ld_r16_i16() -> uint8_t;

  auto inc_r8() -> uint8_t;
  auto inc_r16() -> uint8_t;

  auto dec_r8() -> uint8_t;
  auto dec_r16() -> uint8_t;

  auto add_hl_r16() -> uint8_t;
  auto add_a_r8() -> uint8_t;

  auto adc_a_r8() -> uint8_t;

  void write_reg(uint8_t dst, uint8_t val);
  auto read_reg(uint8_t src) -> uint8_t;

  void write_r16(uint8_t dst, uint16_t val);
  auto read_r16(uint8_t src) -> uint16_t;

  std::array<Instr, 256> cb_table{}, op_table{};
};
