#include "cpu.hpp"
#include "memory_map.hpp"
#include <cstdint>

auto CPU::step() -> uint8_t {
  if (ime_enable_pending) {
    ime = true;
    ime_enable_pending = false;
  }

  if (ime && pending_interrupts()) {
    halted = false;
    return service_interrupt();
  }

  if (halted) {
    if (pending_interrupts()) {
      halted = false;
    }
    return 4;
  }

  uint8_t opcode = bus.read(reg.pc);

  if (!halt_bug) {
    reg.pc++;
  } else {
    halt_bug = false;
  }

  uint8_t cycles = 0;
  if (opcode == 0xCB) {
    opcode = bus.read(reg.pc++);
    cycles = (this->*cb_table[opcode])(opcode);
  } else {
    cycles = (this->*op_table[opcode])(opcode);
  }

  return cycles;
}

void CPU::init_tables() {
  // https://gbdev.io/pandocs/CPU_Instruction_Set.html
  // BLOCK 0 -> first 6 tables done last 2 left
  op_table[0x00] = &CPU::nop;

  op_table[0x07] = &CPU::rlca;
  op_table[0x17] = &CPU::rla;
  op_table[0x0F] = &CPU::rrca;
  op_table[0x1F] = &CPU::rra;
  op_table[0x27] = &CPU::daa;
  op_table[0x2F] = &CPU::cpl;
  op_table[0x37] = &CPU::scf;
  op_table[0x3F] = &CPU::ccf;

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
  for (uint8_t i = 0x40; i <= 0x7F; i++) {
    if (i == 0x76) {
      op_table[i] = &CPU::halt;
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

  for (uint8_t i = 0x90; i <= 0x97; i++) {
    op_table[i] = &CPU::sub_a_r8;
  }

  for (uint8_t i = 0x98; i <= 0x9F; i++) {
    op_table[i] = &CPU::sbc_a_r8;
  }

  for (uint8_t i = 0xA0; i <= 0xA7; i++) {
    op_table[i] = &CPU::and_a_r8;
  }

  for (uint8_t i = 0xA8; i <= 0xAF; i++) {
    op_table[i] = &CPU::xor_a_r8;
  }

  for (uint8_t i = 0xB0; i <= 0xB7; i++) {
    op_table[i] = &CPU::or_a_r8;
  }

  for (uint8_t i = 0xB8; i <= 0xBF; i++) {
    op_table[i] = &CPU::cp_a_r8;
  }

  // BLOCK 3
  op_table[0xC6] = &CPU::add_a_i8;
  op_table[0xCE] = &CPU::adc_a_i8;
  op_table[0xD6] = &CPU::sub_a_i8;
  op_table[0xDE] = &CPU::sbc_a_i8;
  op_table[0xE6] = &CPU::and_a_i8;
  op_table[0xEE] = &CPU::xor_a_i8;
  op_table[0xF6] = &CPU::or_a_i8;
  op_table[0xFE] = &CPU::cp_a_i8;

  op_table[0xFB] = &CPU::ei;
  op_table[0xF3] = &CPU::di;
}

auto CPU::nop(uint8_t opcode) -> uint8_t { return 4; }

auto CPU::ei(uint8_t opcode) -> uint8_t {
  ime_enable_pending = true; // IME will become 1 after next instruction
  return 4;
}

auto CPU::di(uint8_t opcode) -> uint8_t {
  ime = false; // immediately disable interrupts
  return 4;
}

auto CPU::halt(uint8_t opcode) -> uint8_t {
  if (!ime && pending_interrupts()) {
    halt_bug = true;
  } else {
    halted = true;
  }

  return 4;
}

auto CPU::rra(uint8_t opcode) -> uint8_t {
  uint8_t old = reg.a;
  bool carry = reg.get_flag(gb::mem::FLAG_C);

  reg.a = (old >> 1) | ((carry ? 1 : 0) << 7);

  reg.set_flag(gb::mem::FLAG_Z, false);
  reg.set_flag(gb::mem::FLAG_N, false);
  reg.set_flag(gb::mem::FLAG_H, false);
  reg.set_flag(gb::mem::FLAG_C, static_cast<bool>(old & 0x01));
  return 4;
}

auto CPU::rrca(uint8_t opcode) -> uint8_t {
  uint8_t old = reg.a;
  reg.a = (old >> 1) | (old << 7); // rotate left circular

  reg.set_flag(gb::mem::FLAG_Z, false);
  reg.set_flag(gb::mem::FLAG_N, false);
  reg.set_flag(gb::mem::FLAG_H, false);
  reg.set_flag(gb::mem::FLAG_C, static_cast<bool>(old & 0x01));
  return 4;
}

auto CPU::rla(uint8_t opcode) -> uint8_t {
  uint8_t old = reg.a;
  bool carry = reg.get_flag(gb::mem::FLAG_C);

  reg.a = (old << 1) | (carry ? 1 : 0);

  reg.set_flag(gb::mem::FLAG_Z, false);
  reg.set_flag(gb::mem::FLAG_N, false);
  reg.set_flag(gb::mem::FLAG_H, false);
  reg.set_flag(gb::mem::FLAG_C, static_cast<bool>(old & 0x80));
  return 4;
}

auto CPU::rlca(uint8_t opcode) -> uint8_t {
  uint8_t old = reg.a;
  reg.a = (old << 1) | (old >> 7); // rotate left circular
  reg.set_flag(gb::mem::FLAG_Z, false);
  reg.set_flag(gb::mem::FLAG_N, false);
  reg.set_flag(gb::mem::FLAG_H, false);
  reg.set_flag(gb::mem::FLAG_C, static_cast<bool>(old & 0x80));
  return 4;
}

// DAA – Decimal Adjust Accumulator
auto CPU::daa(uint8_t opcode) -> uint8_t {
  uint8_t a = reg.a;
  bool n = reg.get_flag(gb::mem::FLAG_N);
  bool h = reg.get_flag(gb::mem::FLAG_H);
  bool c = reg.get_flag(gb::mem::FLAG_C);
  uint8_t adjust = 0;

  if (!n) { // after addition
    if (h || (a & 0x0F) > 9) {
      adjust |= 0x06;
    }
    if (c || a > 0x99) {
      adjust |= 0x60;
      reg.set_flag(gb::mem::FLAG_C, true);
    }
  } else { // after subtraction
    if (h) {
      adjust |= 0x06;
    }
    if (c) {
      adjust |= 0x60;
    }
  }

  a = n ? a - adjust : a + adjust;
  reg.a = a;

  reg.set_flag(gb::mem::FLAG_Z, a == 0);
  reg.set_flag(gb::mem::FLAG_H, false);
  // N unchanged, C already handled above

  return 4;
}

// CPL – Complement Accumulator
auto CPU::cpl(uint8_t opcode) -> uint8_t {
  reg.a = ~reg.a;

  reg.set_flag(gb::mem::FLAG_N, true);
  reg.set_flag(gb::mem::FLAG_H, true);
  // Z and C unchanged

  return 4;
}

// SCF – Set Carry Flag
auto CPU::scf(uint8_t opcode) -> uint8_t {
  reg.set_flag(gb::mem::FLAG_C, true);
  reg.set_flag(gb::mem::FLAG_N, false);
  reg.set_flag(gb::mem::FLAG_H, false);
  // Z unchanged

  return 4;
}

// CCF – Complement Carry Flag
auto CPU::ccf(uint8_t opcode) -> uint8_t {
  bool c = reg.get_flag(gb::mem::FLAG_C);
  reg.set_flag(gb::mem::FLAG_C, !c);
  reg.set_flag(gb::mem::FLAG_N, false);
  reg.set_flag(gb::mem::FLAG_H, false);
  // Z unchanged

  return 4;
}

auto CPU::pending_interrupts() -> bool {
  uint8_t ie = bus.read(0xFFFF);
  uint8_t iff = bus.read(0xFF0F);
  return (ie & iff) != 0;
}

auto CPU::or_a_i8(uint8_t opcode) -> uint8_t {
  uint8_t i8 = bus.read(reg.pc++);

  uint8_t val = reg.a | i8;

  reg.set_flag(gb::mem::FLAG_Z, val == 0);
  reg.set_flag(gb::mem::FLAG_N, false);
  reg.set_flag(gb::mem::FLAG_H, false);
  reg.set_flag(gb::mem::FLAG_C, false);

  reg.a = val;

  return 8;
}

auto CPU::cp_a_i8(uint8_t opcode) -> uint8_t {
  uint8_t i8 = bus.read(reg.pc++);
  uint8_t a = reg.a;

  uint16_t result = a - i8;
  auto final = static_cast<uint8_t>(result);

  reg.set_flag(gb::mem::FLAG_Z, final == 0);
  reg.set_flag(gb::mem::FLAG_N, true);
  reg.set_flag(gb::mem::FLAG_H, (a & 0x0F) < (i8 & 0x0F));
  reg.set_flag(gb::mem::FLAG_C, a < i8);

  return 8;
}

auto CPU::xor_a_i8(uint8_t opcode) -> uint8_t {
  uint8_t i8 = bus.read(reg.pc++);

  uint8_t val = reg.a ^ i8;

  reg.set_flag(gb::mem::FLAG_Z, val == 0);
  reg.set_flag(gb::mem::FLAG_N, false);
  reg.set_flag(gb::mem::FLAG_H, false);
  reg.set_flag(gb::mem::FLAG_C, false);

  reg.a = val;

  return 8;
}

auto CPU::and_a_i8(uint8_t opcode) -> uint8_t {
  uint8_t i8 = bus.read(reg.pc++);

  uint8_t val = reg.a & i8;

  reg.set_flag(gb::mem::FLAG_Z, val == 0);
  reg.set_flag(gb::mem::FLAG_N, false);
  reg.set_flag(gb::mem::FLAG_H, true);
  reg.set_flag(gb::mem::FLAG_C, false);

  reg.a = val;

  return 8;
}

auto CPU::sub_a_i8(uint8_t opcode) -> uint8_t {
  uint8_t i8 = bus.read(reg.pc++);
  uint8_t a = reg.a;

  uint16_t result = a - i8;
  auto final = static_cast<uint8_t>(result);

  reg.set_flag(gb::mem::FLAG_Z, final == 0);
  reg.set_flag(gb::mem::FLAG_N, true);
  reg.set_flag(gb::mem::FLAG_H, (a & 0x0F) < (i8 & 0x0F));
  reg.set_flag(gb::mem::FLAG_C, i8 > a);

  reg.a = final;

  return 8;
}

auto CPU::sbc_a_i8(uint8_t opcode) -> uint8_t {
  uint8_t i8 = bus.read(reg.pc++);
  uint8_t a = reg.a;
  auto carry = static_cast<uint8_t>(reg.get_flag(gb::mem::FLAG_C));

  uint16_t result = a - i8 - carry;
  auto final = static_cast<uint8_t>(result);

  reg.set_flag(gb::mem::FLAG_Z, final == 0);
  reg.set_flag(gb::mem::FLAG_N, true);
  reg.set_flag(gb::mem::FLAG_H, (a & 0x0F) < ((i8 & 0x0F) + carry));
  reg.set_flag(gb::mem::FLAG_C, i8 + carry > a);

  reg.a = final;

  return 8;
}

auto CPU::adc_a_i8(uint8_t opcode) -> uint8_t {
  uint8_t i8 = bus.read(reg.pc++);
  uint8_t a = reg.a;
  auto carry = static_cast<uint8_t>(reg.get_flag(gb::mem::FLAG_C));

  uint16_t result = a + i8 + carry;
  auto final = static_cast<uint8_t>(result);

  reg.set_flag(gb::mem::FLAG_Z, final == 0);
  reg.set_flag(gb::mem::FLAG_N, false);
  reg.set_flag(gb::mem::FLAG_H, ((a & 0x0F) + (i8 & 0x0F) + carry) > 0x0F);
  reg.set_flag(gb::mem::FLAG_C, result > 0xFF);

  reg.a = final;

  return 8;
}

auto CPU::add_a_i8(uint8_t opcode) -> uint8_t {
  uint8_t i8 = bus.read(reg.pc++);
  uint8_t a = reg.a;

  uint16_t result = a + i8;
  auto final = static_cast<uint8_t>(result);

  reg.set_flag(gb::mem::FLAG_Z, final == 0);
  reg.set_flag(gb::mem::FLAG_N, false);
  reg.set_flag(gb::mem::FLAG_H, ((a & 0x0F) + (i8 & 0x0F)) > 0x0F);
  reg.set_flag(gb::mem::FLAG_C, result > 0xFF);

  reg.a = final;

  return 8;
}

auto CPU::or_a_r8(uint8_t opcode) -> uint8_t {
  uint8_t reg_no = opcode & 0x07;

  uint8_t r8 = read_reg(reg_no);

  uint8_t val = reg.a | r8;

  reg.set_flag(gb::mem::FLAG_Z, val == 0);
  reg.set_flag(gb::mem::FLAG_N, false);
  reg.set_flag(gb::mem::FLAG_H, false);
  reg.set_flag(gb::mem::FLAG_C, false);

  reg.a = val;

  if (reg_no == 6) {
    return 8;
  }
  return 4;
}

auto CPU::cp_a_r8(uint8_t opcode) -> uint8_t {
  uint8_t reg_no = opcode & 0x07;

  uint8_t val = read_reg(reg_no);
  uint8_t a = reg.a;

  uint16_t result = a - val;
  auto final = static_cast<uint8_t>(result);

  reg.set_flag(gb::mem::FLAG_Z, final == 0);
  reg.set_flag(gb::mem::FLAG_N, true);
  reg.set_flag(gb::mem::FLAG_H, (a & 0x0F) < (val & 0x0F));
  reg.set_flag(gb::mem::FLAG_C, a < val);

  if (reg_no == 6) {
    return 8;
  }
  return 4;
}

auto CPU::xor_a_r8(uint8_t opcode) -> uint8_t {
  uint8_t reg_no = opcode & 0x07;

  uint8_t r8 = read_reg(reg_no);

  uint8_t val = reg.a ^ r8;

  reg.set_flag(gb::mem::FLAG_Z, val == 0);
  reg.set_flag(gb::mem::FLAG_N, false);
  reg.set_flag(gb::mem::FLAG_H, false);
  reg.set_flag(gb::mem::FLAG_C, false);

  reg.a = val;

  if (reg_no == 6) {
    return 8;
  }
  return 4;
}

auto CPU::and_a_r8(uint8_t opcode) -> uint8_t {
  uint8_t reg_no = opcode & 0x07;

  uint8_t r8 = read_reg(reg_no);

  uint8_t val = reg.a & r8;

  reg.set_flag(gb::mem::FLAG_Z, val == 0);
  reg.set_flag(gb::mem::FLAG_N, false);
  reg.set_flag(gb::mem::FLAG_H, true);
  reg.set_flag(gb::mem::FLAG_C, false);

  reg.a = val;

  if (reg_no == 6) {
    return 8;
  }
  return 4;
}

auto CPU::sub_a_r8(uint8_t opcode) -> uint8_t {
  uint8_t src = opcode & 0x07;

  uint8_t val = read_reg(src);
  uint8_t a = reg.a;

  uint16_t result = a - val;
  auto final = static_cast<uint8_t>(result);

  reg.set_flag(gb::mem::FLAG_Z, final == 0);
  reg.set_flag(gb::mem::FLAG_N, true);
  reg.set_flag(gb::mem::FLAG_H, (a & 0x0F) < (val & 0x0F));
  reg.set_flag(gb::mem::FLAG_C, val > a);

  reg.a = final;

  if (src == 6) {
    return 8;
  }
  return 4;
}

auto CPU::sbc_a_r8(uint8_t opcode) -> uint8_t {
  uint8_t src = opcode & 0x07;

  uint8_t val = read_reg(src);
  uint8_t a = reg.a;
  auto carry = static_cast<uint8_t>(reg.get_flag(gb::mem::FLAG_C));

  uint16_t result = a - val - carry;
  auto final = static_cast<uint8_t>(result);

  reg.set_flag(gb::mem::FLAG_Z, final == 0);
  reg.set_flag(gb::mem::FLAG_N, true);
  reg.set_flag(gb::mem::FLAG_H, (a & 0x0F) < ((val & 0x0F) + carry));
  reg.set_flag(gb::mem::FLAG_C, val + carry > a);

  reg.a = final;

  if (src == 6) {
    return 8;
  }
  return 4;
}

auto CPU::adc_a_r8(uint8_t opcode) -> uint8_t {
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

auto CPU::add_a_r8(uint8_t opcode) -> uint8_t {
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

auto CPU::ld_r16_i16(uint8_t opcode) -> uint8_t {
  uint8_t dst_reg = (opcode >> 4) & 0x3;

  uint8_t low = bus.read(reg.pc++);
  uint8_t high = bus.read(reg.pc++);

  uint16_t val = (high << 8) | low;

  write_r16(dst_reg, val);

  return 12;
}

auto CPU::ld_i16_sp(uint8_t opcode) -> uint8_t {
  uint8_t low_byte = bus.read(reg.pc++);
  uint8_t high_byte = bus.read(reg.pc++);

  uint16_t addr = (high_byte << 8) | low_byte;

  // write SP little-endian
  bus.write(addr, reg.sp & 0xFF);            // low byte
  bus.write(addr + 1, (reg.sp >> 8) & 0xFF); // high byte

  return 20;
}

auto CPU::ld_r8_r8(uint8_t opcode) -> uint8_t {

  uint8_t dst = (opcode >> 3) & 0x07;
  uint8_t src = opcode & 0x07;

  write_reg(dst, read_reg(src));

  if (src == 6 || dst == 6) {
    return 8;
  }
  return 4;
}

auto CPU::ld_r8_i8(uint8_t opcode) -> uint8_t {

  uint8_t dst = (opcode >> 3) & 0x07;
  uint8_t val = bus.read(reg.pc++);

  write_reg(dst, val);

  if (dst == 6) {
    return 12;
  }
  return 8;
}

auto CPU::ld_mem_a(uint8_t opcode) -> uint8_t {

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

auto CPU::ld_a_mem(uint8_t opcode) -> uint8_t {
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

auto CPU::add_hl_r16(uint8_t opcode) -> uint8_t {
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

auto CPU::inc_r16(uint8_t opcode) -> uint8_t {
  uint8_t dst = (opcode >> 4) & 0x03;

  uint16_t val = read_r16(dst);
  uint16_t result = val + 1;

  write_r16(dst, result);

  return 8;
}

auto CPU::dec_r16(uint8_t opcode) -> uint8_t {
  uint8_t dst = (opcode >> 4) & 0x03;

  uint16_t val = read_r16(dst);
  uint16_t result = val - 1;

  write_r16(dst, result);

  return 8;
}

auto CPU::inc_r8(uint8_t opcode) -> uint8_t {
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

auto CPU::dec_r8(uint8_t opcode) -> uint8_t {
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
