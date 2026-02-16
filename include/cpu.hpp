#pragma once

#include "bus.hpp"
#include "memory.hpp"
#include <array>
#include <cstdint>
#include <iostream>

class CPU {
public:
  using Instr = uint8_t (CPU::*)(uint8_t);

  CPU() {
    init_tables();
    std::cout << "Initializing CPU\n";
  }

  auto step() -> uint8_t;

private:
  Register reg{};
  Bus bus;
  bool halted = false;
  bool halt_bug = false;
  bool ime = false; // if not already stored elsewhere
  bool ime_enable_pending = false;

  void init_tables();
  auto pending_interrupts() -> bool;

  // TODO
  auto service_interrupt() -> uint8_t;

  auto nop(uint8_t) -> uint8_t;
  auto halt(uint8_t) -> uint8_t;

  auto ld_r8_r8(uint8_t) -> uint8_t;
  auto ld_r8_i8(uint8_t) -> uint8_t;
  auto ld_mem_a(uint8_t) -> uint8_t;
  auto ld_a_mem(uint8_t) -> uint8_t;
  auto ld_i16_sp(uint8_t) -> uint8_t;
  auto ld_r16_i16(uint8_t) -> uint8_t;

  auto rlca(uint8_t) -> uint8_t;
  auto rla(uint8_t) -> uint8_t;
  auto rrca(uint8_t) -> uint8_t;
  auto rra(uint8_t) -> uint8_t;
  auto daa(uint8_t) -> uint8_t;
  auto cpl(uint8_t) -> uint8_t;
  auto scf(uint8_t) -> uint8_t;
  auto ccf(uint8_t) -> uint8_t;

  auto inc_r8(uint8_t) -> uint8_t;
  auto inc_r16(uint8_t) -> uint8_t;

  auto dec_r8(uint8_t) -> uint8_t;
  auto dec_r16(uint8_t) -> uint8_t;

  auto add_hl_r16(uint8_t) -> uint8_t;

  auto add_a_r8(uint8_t) -> uint8_t;
  auto adc_a_r8(uint8_t) -> uint8_t;

  auto sub_a_r8(uint8_t) -> uint8_t;
  auto sbc_a_r8(uint8_t) -> uint8_t;

  auto and_a_r8(uint8_t) -> uint8_t;
  auto xor_a_r8(uint8_t) -> uint8_t;

  auto or_a_r8(uint8_t) -> uint8_t;
  auto cp_a_r8(uint8_t) -> uint8_t;

  auto add_a_i8(uint8_t) -> uint8_t;
  auto adc_a_i8(uint8_t) -> uint8_t;

  auto sub_a_i8(uint8_t) -> uint8_t;
  auto sbc_a_i8(uint8_t) -> uint8_t;

  auto and_a_i8(uint8_t) -> uint8_t;
  auto xor_a_i8(uint8_t) -> uint8_t;

  auto or_a_i8(uint8_t) -> uint8_t;
  auto cp_a_i8(uint8_t) -> uint8_t;

  auto ei(uint8_t) -> uint8_t;
  auto di(uint8_t) -> uint8_t;

  void write_reg(uint8_t dst, uint8_t val);
  auto read_reg(uint8_t src) -> uint8_t;

  void write_r16(uint8_t dst, uint16_t val);
  auto read_r16(uint8_t src) -> uint16_t;

  std::array<Instr, 256> cb_table{}, op_table{};
};
