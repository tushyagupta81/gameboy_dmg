#include "cpu.hpp"
#include "memory_map.hpp"
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
  // https://gbdev.io/pandocs/CPU_Instruction_Set.html
  // BLOCK 0 -> first 5 tables done last 3 left
  op_table[0x00] = &CPU::nop;

  for (uint8_t i = 0x1; i <= 0x31; i += 16) {
    op_table[i] = &CPU::ld_r16_i16;
  }

  for (uint8_t i = 0x2; i <= 0x32; i += 16) {
    op_table[i] = &CPU::ld_mem_a;
  }

  for (uint8_t i = 0x3; i <= 0x33; i += 16) {
    op_table[i] = &CPU::inc_r16;
  }

  for (uint8_t i = 0x04; i <= 0x3C; i += 8) {
    op_table[i] = &CPU::inc_r8;
  }

  for (uint8_t i = 0x05; i <= 0x3D; i += 8) {
    op_table[i] = &CPU::dec_r8;
  }

  for (uint8_t i = 0x6; i <= 0x3E; i += 8) {
    op_table[i] = &CPU::ld_r8_i8;
  }

  op_table[0x08] = &CPU::ld_i16_sp;

  for (uint8_t i = 0x9; i <= 0x39; i += 16) {
    op_table[i] = &CPU::add_hl_r16;
  }

  for (uint8_t i = 0xA; i <= 0x3A; i += 16) {
    op_table[i] = &CPU::ld_a_mem;
  }

  for (uint8_t i = 0xB; i <= 0x3B; i += 16) {
    op_table[i] = &CPU::dec_r16;
  }

  // BLOCK 1 halt inst left
  for (uint8_t i = 0x40; i <= 0x4F; i++) {
    if (i == 0x76) {
      // op_table[i] =
      continue;
    }
    op_table[i] = &CPU::ld_r8_r8;
  }

  // BLOCK 2
  for (uint8_t i = 0x80; i <= 0x87; i++) {
    op_table[i] = &CPU::add_a_r8;
  }
  for (uint8_t i = 0x88; i <= 0x8F; i++) {
    op_table[i] = &CPU::adc_a_r8;
  }
}

auto CPU::nop() -> uint8_t { return 4; }

auto CPU::adc_a_r8() -> uint8_t {
  uint8_t opcode = bus.read(reg.pc - 1);
  uint8_t src = opcode & 0x07;

  uint8_t val = read_reg(src);
  uint8_t a = reg.a;
  auto carry = static_cast<uint8_t>(reg.get_flag(gb::mem::FLAG_C));

  uint16_t result = a + val + carry;
  auto final = static_cast<uint8_t>(result);

  reg.set_flag(gb::mem::FLAG_Z, final == 0);
  reg.set_flag(gb::mem::FLAG_N, false);
  reg.set_flag(gb::mem::FLAG_H, ((a & 0x0F) + (val & 0x0F) + carry) > 0x0F);
  reg.set_flag(gb::mem::FLAG_C, result > 0xFF);

  reg.a = final;

  if (src == 6) {
    return 8;
  }
  return 4;
}

auto CPU::add_a_r8() -> uint8_t {
  uint8_t opcode = bus.read(reg.pc - 1);
  uint8_t src = opcode & 0x07;

  uint8_t val = read_reg(src);
  uint8_t a = reg.a;

  uint16_t result = a + val;
  auto final = static_cast<uint8_t>(result);

  reg.set_flag(gb::mem::FLAG_Z, final == 0);
  reg.set_flag(gb::mem::FLAG_N, false);
  reg.set_flag(gb::mem::FLAG_H, ((a & 0x0F) + (val & 0x0F)) > 0x0F);
  reg.set_flag(gb::mem::FLAG_C, result > 0xFF);

  reg.a = final;

  if (src == 6) {
    return 8;
  }
  return 4;
}

auto CPU::ld_r16_i16() -> uint8_t {
  uint8_t opcode = bus.read(reg.pc - 1);
  uint8_t dst_reg = (opcode >> 4) & 0x3;

  uint8_t low = bus.read(reg.pc++);
  uint8_t high = bus.read(reg.pc++);

  uint16_t val = (high << 8) | low;

  write_r16(dst_reg, val);

  return 12;
}

auto CPU::ld_i16_sp() -> uint8_t {
  uint8_t low_byte = bus.read(reg.pc++);
  uint8_t high_byte = bus.read(reg.pc++);

  uint16_t addr = (high_byte << 8) | low_byte;

  // write SP little-endian
  bus.write(addr, reg.sp & 0xFF);            // low byte
  bus.write(addr + 1, (reg.sp >> 8) & 0xFF); // high byte

  return 20;
}

auto CPU::ld_r8_r8() -> uint8_t {
  uint8_t opcode = bus.read(reg.pc - 1);

  uint8_t dst = (opcode >> 3) & 0x07;
  uint8_t src = opcode & 0x07;

  write_reg(dst, read_reg(src));

  if (src == 6 || dst == 6) {
    return 8;
  }
  return 4;
}

auto CPU::ld_r8_i8() -> uint8_t {
  uint8_t opcode = bus.read(reg.pc - 1);

  uint8_t dst = (opcode >> 3) & 0x07;
  uint8_t val = bus.read(reg.pc++);

  write_reg(dst, val);

  if (dst == 6) {
    return 12;
  }
  return 8;
}

auto CPU::ld_mem_a() -> uint8_t {
  uint8_t opcode = bus.read(reg.pc - 1);

  uint8_t dst_addr_reg = (opcode >> 4) & 0x03;
  uint16_t dst_addr = 0;

  switch (dst_addr_reg) {
  case 0:
    dst_addr = reg.bc();
    break;
  case 1:
    dst_addr = reg.de();
    break;
  case 2:
    dst_addr = reg.hl();
    reg.set_hl(dst_addr + 1);
    break;
  case 3:
    dst_addr = reg.hl();
    reg.set_hl(dst_addr - 1);
    break;
  default:
    __builtin_unreachable();
  }

  bus.write(dst_addr, reg.a);

  return 8;
}

auto CPU::ld_a_mem() -> uint8_t {
  uint8_t opcode = bus.read(reg.pc - 1);
  uint8_t src_addr_reg = (opcode >> 4) & 0x03;

  uint16_t src_addr = 0;

  switch (src_addr_reg) {
  case 0:
    src_addr = reg.bc();
    break;
  case 1:
    src_addr = reg.de();
    break;
  case 2:
    src_addr = reg.hl();
    reg.set_hl(src_addr + 1);
    break;
  case 3:
    src_addr = reg.hl();
    reg.set_hl(src_addr - 1);
    break;
  default:
    __builtin_unreachable();
  }

  reg.a = bus.read(src_addr);

  return 8;
}

auto CPU::add_hl_r16() -> uint8_t {
  uint8_t opcode = bus.read(reg.pc - 1);
  uint8_t dst = (opcode >> 4) & 0x03;

  uint16_t hl = reg.hl();
  uint16_t val = read_r16(dst);

  uint32_t result = hl + val;

  // Flags
  reg.set_flag(gb::mem::FLAG_N, false);
  reg.set_flag(gb::mem::FLAG_H, ((hl & 0x0FFF) + (val & 0x0FFF)) > 0x0FFF);
  reg.set_flag(gb::mem::FLAG_C, result > 0xFFFF);
  // Z untouched

  reg.set_hl(static_cast<uint16_t>(result));

  return 8;
}

auto CPU::inc_r16() -> uint8_t {
  uint8_t opcode = bus.read(reg.pc - 1);
  uint8_t dst = (opcode >> 4) & 0x03;

  uint16_t val = read_r16(dst);
  uint16_t result = val + 1;

  write_r16(dst, result);

  return 8;
}

auto CPU::dec_r16() -> uint8_t {
  uint8_t opcode = bus.read(reg.pc - 1);
  uint8_t dst = (opcode >> 4) & 0x03;

  uint16_t val = read_r16(dst);
  uint16_t result = val - 1;

  write_r16(dst, result);

  return 8;
}

auto CPU::inc_r8() -> uint8_t {
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

auto CPU::dec_r8() -> uint8_t {
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

void CPU::write_r16(uint8_t dst, uint16_t val) {
  switch (dst) {
  case 0:
    reg.set_bc(val);
    break;
  case 1:
    reg.set_de(val);
    break;
  case 2:
    reg.set_hl(val);
    break;
  case 3:
    reg.sp = val;
    break;
  default:
    __builtin_unreachable();
  }
}

auto CPU::read_r16(uint8_t src) -> uint16_t {
  switch (src) {
  case 0:
    return reg.bc();
  case 1:
    return reg.de();
  case 2:
    return reg.hl();
  case 3:
    return reg.sp;
  default:
    __builtin_unreachable();
  }
}
