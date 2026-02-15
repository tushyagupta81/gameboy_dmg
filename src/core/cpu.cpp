#include "cpu.hpp"
#include <cstdint>

auto CPU::step() -> uint8_t {
  uint8_t opcode = bus.read(reg.pc++);

  uint8_t cycles = 0;
  if (opcode == 0xCB) {
    opcode = bus.read(reg.pc++);
    cycles = (this->*cb_table[opcode])();
  } else {
    cycles = (this->*op_table[opcode])();
  }

  return cycles;
}

void CPU::init_tables() {
  op_table[0x00] = &CPU::nop;

  for (uint8_t i = 0x6; i <= 0x3E; i += 8) {
    op_table[i] = &CPU::ld_r_d8;
  }

  for (uint8_t i = 0x04; i <= 0x3C; i += 0x08) {
    op_table[i] = &CPU::inc_r;
  }

  for (uint8_t i = 0x05; i <= 0x3D; i += 0x08) {
    op_table[i] = &CPU::dec_r;
  }

  for (uint8_t i = 0x40; i <= 0x4F; i++) {
    if (i == 0x76) {
      // op_table[i] =
      continue;
    }
    op_table[i] = &CPU::ld_r_r;
  }
}

auto CPU::nop() -> uint8_t { return 4; }

auto CPU::ld_r_r() -> uint8_t {
  uint8_t opcode = bus.read(reg.pc - 1);

  uint8_t dst = (opcode >> 3) & 0x07;
  uint8_t src = opcode & 0x07;

  write_reg(dst, read_reg(src));

  if (src == 6 || dst == 6) {
    return 8;
  }
  return 4;
}

auto CPU::ld_r_d8() -> uint8_t {
  uint8_t opcode = bus.read(reg.pc - 1);

  uint8_t dst = (opcode >> 3) & 0x07;
  uint8_t val = bus.read(reg.pc++);

  write_reg(dst, val);

  if (dst == 6) {
    return 12;
  }
  return 8;
}

auto CPU::inc_r() -> uint8_t {
  uint8_t opcode = bus.read(reg.pc - 1);
  uint8_t dst = (opcode >> 3) & 0x07;

  uint8_t val = read_reg(dst);
  uint8_t result = val + 1;

  // Flags
  reg.set_flag(gb::mem::FLAG_Z, result == 0);
  reg.set_flag(gb::mem::FLAG_N, false);
  reg.set_flag(gb::mem::FLAG_H, ((val & 0x0F) + 1) > 0x0F);
  // C untouched

  write_reg(dst, result);

  return (dst == 6) ? 12 : 4;
}

auto CPU::dec_r() -> uint8_t {
  uint8_t opcode = bus.read(reg.pc - 1);
  uint8_t dst = (opcode >> 3) & 0x07;

  uint8_t val = read_reg(dst);
  uint8_t result = val - 1;

  // Flags
  reg.set_flag(gb::mem::FLAG_Z, result == 0);
  reg.set_flag(gb::mem::FLAG_N, true);
  reg.set_flag(gb::mem::FLAG_H, (val & 0x0F) == 0);
  // C untouched

  write_reg(dst, result);

  return (dst == 6) ? 12 : 4;
}

void CPU::write_reg(uint8_t dst, uint8_t val) {
  switch (dst) {
  case 0:
    reg.b = val;
    break;
  case 1:
    reg.c = val;
    break;
  case 2:
    reg.d = val;
    break;
  case 3:
    reg.e = val;
    break;
  case 4:
    reg.h = val;
    break;
  case 5:
    reg.l = val;
    break;
  case 6:
    bus.write(reg.hl(), val);
    break; // memory write
  case 7:
    reg.a = val;
    break;
  default:
    __builtin_unreachable();
  }
}

auto CPU::read_reg(uint8_t src) -> uint8_t {
  switch (src) {
  case 0:
    return reg.b;
  case 1:
    return reg.c;
  case 2:
    return reg.d;
  case 3:
    return reg.e;
  case 4:
    return reg.h;
  case 5:
    return reg.l;
  case 6:
    return bus.read(reg.hl());
  case 7:
    return reg.a;
  default:
    __builtin_unreachable();
  }
}
