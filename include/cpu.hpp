#pragma once

#include "bus.hpp"
#include "hardware.hpp"
#include "memory.hpp"
#include <array>
#include <cstdint>
#include <fstream>
#include <string>

class CPU {
public:
  using Instr = uint8_t (CPU::*)(uint8_t);
  CPU(const std::string &rom_path);
  auto step() -> uint8_t;

  void dump_opcode_table() const;
  void timer_tick(int cycles);

private:
  std::ofstream trace;

  HardwareType curr_hardware;

  Register reg{};
  Bus bus;
  bool halted;
  bool halt_bug;
  bool ime;
  bool ime_enable_pending;

  void init_tables();
  auto pending_interrupts() -> bool;

  auto service_interrupt() -> uint8_t;

  auto nop(uint8_t) -> uint8_t;
  auto halt(uint8_t) -> uint8_t;
  auto illegal(uint8_t) -> uint8_t;

  // TODO
  auto stop(uint8_t) -> uint8_t;

  auto jr(uint8_t) -> uint8_t;
  auto jr_cond(uint8_t) -> uint8_t;

  auto jp_i16(uint8_t) -> uint8_t;
  auto jp_cond_i16(uint8_t) -> uint8_t;
  auto jp_hl(uint8_t) -> uint8_t;

  auto ld_r8_r8(uint8_t) -> uint8_t;
  auto ld_r8_i8(uint8_t) -> uint8_t;
  auto ld_mem_a(uint8_t) -> uint8_t;
  auto ld_a_mem(uint8_t) -> uint8_t;
  auto ld_i16_sp(uint8_t) -> uint8_t;
  auto ld_r16_i16(uint8_t) -> uint8_t;

  auto ldh_i8_a(uint8_t) -> uint8_t;
  auto ldh_a_i8(uint8_t) -> uint8_t;
  auto ldh_c_a(uint8_t) -> uint8_t;
  auto ldh_a_c(uint8_t) -> uint8_t;

  auto ld_i16_a(uint8_t) -> uint8_t;
  auto ld_a_i16(uint8_t) -> uint8_t;

  auto ld_hl_sp_i8(uint8_t) -> uint8_t;
  auto ld_sp_hl(uint8_t) -> uint8_t;

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

  auto add_sp_i8(uint8_t) -> uint8_t;

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

  auto ret(uint8_t) -> uint8_t;
  auto ret_cond(uint8_t) -> uint8_t;
  auto reti(uint8_t) -> uint8_t;

  auto call(uint8_t) -> uint8_t;
  auto call_cond(uint8_t) -> uint8_t;
  auto rst(uint8_t) -> uint8_t;

  auto push_r16(uint8_t) -> uint8_t;
  auto pop_r16(uint8_t) -> uint8_t;

  void write_reg(uint8_t dst, uint8_t val);
  auto read_reg(uint8_t src) -> uint8_t;

  void write_r16(uint8_t dst, uint16_t val);
  auto read_r16(uint8_t src) -> uint16_t;

  void push_stack_u16(uint16_t);
  auto pop() -> uint16_t;

  // === Prefix Table ===

  auto res(uint8_t) -> uint8_t;
  auto set(uint8_t) -> uint8_t;
  auto bit(uint8_t) -> uint8_t;

  auto rlc(uint8_t) -> uint8_t;
  auto rrc(uint8_t) -> uint8_t;
  auto rl(uint8_t) -> uint8_t;
  auto rr(uint8_t) -> uint8_t;
  auto sla(uint8_t) -> uint8_t;
  auto sra(uint8_t) -> uint8_t;
  auto swap(uint8_t) -> uint8_t;
  auto srl(uint8_t) -> uint8_t;

  void mcycle();

  std::array<Instr, 256> cb_table{}, op_table{};
};
