#include "cpu.hpp"
#include "memory_map.hpp"
#include <cstdint>
#include <fstream>
#include <ios>
#include <iostream>
#include <string>

#ifdef TRACE
#include <iomanip>
#endif // TRACE

CPU::CPU(const std::string &rom_path)
    : halted(false), halt_bug(false), ime(false), ime_enable_pending(false),
      trace("trace.log"), bus() {
  reg.a = reg.f = 0;
  reg.b = reg.c = 0;
  reg.d = reg.e = 0;
  reg.h = reg.l = 0;

  reg.sp = 0xFFFE; // typical GB start SP
  reg.pc = 0x0100; // typical GB entry point

  std::cout << "Initialized registers\n";

  // Clear flags
  reg.clear_flags();

  std::cout << "Cleared Falgs\n";

  // Initialize opcode tables
  init_tables();

  std::cout << "Initialized Tables\n";

  std::ifstream file(rom_path, std::ios::binary);
  if (!file) {
    throw std::runtime_error("Failed to open ROM: " + rom_path);
  }

  std::vector<uint8_t> rom((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());

  bus.load_rom(rom);

  std::cout << "Loaded ROM\n";

  std::cout << "Initialized CPU\n";
}

auto CPU::step() -> uint8_t {
  if (ime && pending_interrupts()) {
    return service_interrupt();
  }

  if (halted) {
    if (pending_interrupts()) {
      halted = false;

      if (!ime) {
        halt_bug = true;
      }
    }
    return 4;
  }

  uint16_t pc_before = reg.pc;
  uint8_t opcode = bus.read(reg.pc);
#ifdef TRACE
  trace << std::hex << std::uppercase;
  trace << "A:" << std::setw(2) << (int)reg.a << " "
        << "F:" << std::setw(2) << (int)reg.f << " "
        << "B:" << std::setw(2) << (int)reg.b << " "
        << "C:" << std::setw(2) << (int)reg.c << " "
        << "D:" << std::setw(2) << (int)reg.d << " "
        << "E:" << std::setw(2) << (int)reg.e << " "
        << "H:" << std::setw(2) << (int)reg.h << " "
        << "L:" << std::setw(2) << (int)reg.l << " "
        << "SP:" << std::setw(4) << reg.sp << " "
        << "PC:" << std::setw(4) << pc_before << " "
        << "OP:" << std::setw(2) << (int)opcode << "\n";
#endif // TRACE

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

  if (ime_enable_pending) {
    ime = true;
    ime_enable_pending = false;
  }

  return cycles;
}

void CPU::init_tables() {
  op_table.fill(&CPU::illegal);
  cb_table.fill(&CPU::illegal);

  auto assign_op = [&](uint8_t opcode, auto fn) -> auto {
    if (op_table[opcode] != &CPU::illegal) {
      std::cerr << "Opcode 0x" << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<int>(opcode) << " overwritten!\n";
      std::abort();
    }
    op_table[opcode] = fn;
  };

  auto assign_cb = [&](uint8_t opcode, auto fn) -> auto {
    if (cb_table[opcode] != &CPU::illegal) {
      std::cerr << "Opcode 0x" << std::hex << std::setw(2) << std::setfill('0')
                << static_cast<int>(opcode) << " overwritten!\n";
      std::abort();
    }
    cb_table[opcode] = fn;
  };

  // ===== Misc =====
  assign_op(0x00, &CPU::nop);
  assign_op(0x10, &CPU::stop);
  assign_op(0x76, &CPU::halt);

  assign_op(0x07, &CPU::rlca);
  assign_op(0x17, &CPU::rla);
  assign_op(0x0F, &CPU::rrca);
  assign_op(0x1F, &CPU::rra);
  assign_op(0x27, &CPU::daa);
  assign_op(0x2F, &CPU::cpl);
  assign_op(0x37, &CPU::scf);
  assign_op(0x3F, &CPU::ccf);

  assign_op(0xFB, &CPU::ei);
  assign_op(0xF3, &CPU::di);

  // ===== JR =====
  assign_op(0x18, &CPU::jr);
  for (uint8_t op : {0x20, 0x28, 0x30, 0x38}) {
    assign_op(op, &CPU::jr_cond);
  }

  // ===== 16-bit loads =====
  for (uint8_t op : {0x01, 0x11, 0x21, 0x31}) {
    assign_op(op, &CPU::ld_r16_i16);
  }

  for (uint8_t op : {0x02, 0x12, 0x22, 0x32}) {
    assign_op(op, &CPU::ld_mem_a);
  }

  for (uint8_t op : {0x0A, 0x1A, 0x2A, 0x3A}) {
    assign_op(op, &CPU::ld_a_mem);
  }

  assign_op(0x08, &CPU::ld_i16_sp);

  // ===== 16-bit inc/dec =====
  for (uint8_t op : {0x03, 0x13, 0x23, 0x33}) {
    assign_op(op, &CPU::inc_r16);
  }

  for (uint8_t op : {0x0B, 0x1B, 0x2B, 0x3B}) {
    assign_op(op, &CPU::dec_r16);
  }

  for (uint8_t op : {0x09, 0x19, 0x29, 0x39}) {
    assign_op(op, &CPU::add_hl_r16);
  }

  // ===== 8-bit inc/dec/load immediate =====
  for (uint8_t op : {0x04, 0x0C, 0x14, 0x1C, 0x24, 0x2C, 0x34, 0x3C}) {
    assign_op(op, &CPU::inc_r8);
  }

  for (uint8_t op : {0x05, 0x0D, 0x15, 0x1D, 0x25, 0x2D, 0x35, 0x3D}) {
    assign_op(op, &CPU::dec_r8);
  }

  for (uint8_t op : {0x06, 0x0E, 0x16, 0x1E, 0x26, 0x2E, 0x36, 0x3E}) {
    assign_op(op, &CPU::ld_r8_i8);
  }

  // ===== LD r8, r8 block =====
  for (uint16_t op = 0x40; op <= 0x7F; ++op) {
    if (op == 0x76) {
      continue; // HALT already assigned
    }
    assign_op(static_cast<uint8_t>(op), &CPU::ld_r8_r8);
  }

  // ===== ALU r8 =====
  for (uint8_t op = 0x80; op <= 0x87; ++op) {
    assign_op(op, &CPU::add_a_r8);
  }
  for (uint8_t op = 0x88; op <= 0x8F; ++op) {
    assign_op(op, &CPU::adc_a_r8);
  }
  for (uint8_t op = 0x90; op <= 0x97; ++op) {
    assign_op(op, &CPU::sub_a_r8);
  }
  for (uint8_t op = 0x98; op <= 0x9F; ++op) {
    assign_op(op, &CPU::sbc_a_r8);
  }
  for (uint8_t op = 0xA0; op <= 0xA7; ++op) {
    assign_op(op, &CPU::and_a_r8);
  }
  for (uint8_t op = 0xA8; op <= 0xAF; ++op) {
    assign_op(op, &CPU::xor_a_r8);
  }
  for (uint8_t op = 0xB0; op <= 0xB7; ++op) {
    assign_op(op, &CPU::or_a_r8);
  }
  for (uint8_t op = 0xB8; op <= 0xBF; ++op) {
    assign_op(op, &CPU::cp_a_r8);
  }

  // ===== ALU immediate =====
  assign_op(0xC6, &CPU::add_a_i8);
  assign_op(0xCE, &CPU::adc_a_i8);
  assign_op(0xD6, &CPU::sub_a_i8);
  assign_op(0xDE, &CPU::sbc_a_i8);
  assign_op(0xE6, &CPU::and_a_i8);
  assign_op(0xEE, &CPU::xor_a_i8);
  assign_op(0xF6, &CPU::or_a_i8);
  assign_op(0xFE, &CPU::cp_a_i8);

  // ===== JP =====
  assign_op(0xC3, &CPU::jp_i16);
  assign_op(0xE9, &CPU::jp_hl);
  for (uint8_t op : {0xC2, 0xCA, 0xD2, 0xDA}) {
    assign_op(op, &CPU::jp_cond_i16);
  }

  // ===== RET =====
  for (uint8_t op : {0xC0, 0xC8, 0xD0, 0xD8}) {
    assign_op(op, &CPU::ret_cond);
  }

  assign_op(0xC9, &CPU::ret);
  assign_op(0xD9, &CPU::reti);

  // ===== CALL =====
  assign_op(0xCD, &CPU::call);
  for (uint8_t op : {0xC4, 0xCC, 0xD4, 0xDC}) {
    assign_op(op, &CPU::call_cond);
  }

  // ===== RST =====
  for (uint8_t op : {0xC7, 0xCF, 0xD7, 0xDF, 0xE7, 0xEF, 0xF7, 0xFF}) {
    assign_op(op, &CPU::rst);
  }

  // ===== PUSH =====
  for (uint8_t op : {0xC5, 0xD5, 0xE5, 0xF5}) {
    assign_op(op, &CPU::push_r16);
  }

  // ===== POP =====
  for (uint8_t op : {0xC1, 0xD1, 0xE1, 0xF1}) {
    assign_op(op, &CPU::pop_r16);
  }

  // ===== HIGH MEM =====
  assign_op(0xE0, &CPU::ldh_i8_a);
  assign_op(0xF0, &CPU::ldh_a_i8);
  assign_op(0xE2, &CPU::ldh_c_a);
  assign_op(0xF2, &CPU::ldh_a_c);

  assign_op(0xEA, &CPU::ld_i16_a);
  assign_op(0xFA, &CPU::ld_a_i16);

  assign_op(0xE8, &CPU::add_sp_i8);
  assign_op(0xF8, &CPU::ld_hl_sp_i8);
  assign_op(0xF9, &CPU::ld_sp_hl);

  // Optional: coverage report
  int implemented_op = 0;
  for (auto fn : op_table) {
    if (fn != &CPU::illegal) {
      implemented_op++;
    }
  }

  std::cout << "Implemented opcodes: " << implemented_op << "/256\n";

  // === Prefix Table ===

  for (uint8_t op = 0x00; op <= 0x07; op++) {
    assign_cb(op, &CPU::rlc);
  }
  for (uint8_t op = 0x08; op <= 0x0F; op++) {
    assign_cb(op, &CPU::rrc);
  }
  for (uint8_t op = 0x10; op <= 0x17; op++) {
    assign_cb(op, &CPU::rl);
  }
  for (uint8_t op = 0x18; op <= 0x1F; op++) {
    assign_cb(op, &CPU::rr);
  }
  for (uint8_t op = 0x20; op <= 0x27; op++) {
    assign_cb(op, &CPU::sla);
  }
  for (uint8_t op = 0x28; op <= 0x2F; op++) {
    assign_cb(op, &CPU::sra);
  }
  for (uint8_t op = 0x30; op <= 0x37; op++) {
    assign_cb(op, &CPU::swap);
  }
  for (uint8_t op = 0x38; op <= 0x3F; op++) {
    assign_cb(op, &CPU::srl);
  }

  for (uint16_t op = 0x40; op <= 0x7F; ++op) {
    assign_cb(static_cast<uint8_t>(op), &CPU::bit);
  }
  for (uint16_t op = 0x80; op <= 0xBF; ++op) {
    assign_cb(static_cast<uint8_t>(op), &CPU::res);
  }
  for (uint16_t op = 0xC0; op <= 0xFF; ++op) {
    assign_cb(static_cast<uint8_t>(op), &CPU::set);
  }

  // Optional: coverage report
  int implemented_cb = 0;
  for (auto fn : cb_table) {
    if (fn != &CPU::illegal) {
      implemented_cb++;
    }
  }

  std::cout << "Implemented opcodes(Prefixed): " << implemented_cb << "/256\n";
}

auto CPU::nop(uint8_t opcode) -> uint8_t { return 4; }

auto CPU::service_interrupt() -> uint8_t {
  if (!ime) {
    return 0;
  }

  uint8_t interrupt_flag = bus.read(0xFF0F);
  uint8_t interrupt_enable = bus.read(0xFFFF);

  uint8_t pending = interrupt_flag & interrupt_enable;

  if (pending == 0) {
    return 0;
  }

  ime = false;
  halted = false;

  for (int i = 0; i < 5; i++) {
    if ((pending & (1 << i)) != 0) {
      interrupt_flag &= ~(1 << i);
      bus.write(0xFF0F, interrupt_flag);

      push_stack_u16(reg.pc);

      switch (i) {
      case 0:
        reg.pc = 0x40;
        break;
      case 1:
        reg.pc = 0x48;
        break;
      case 2:
        reg.pc = 0x50;
        break;
      case 3:
        reg.pc = 0x58;
        break;
      case 4:
        reg.pc = 0x60;
        break;
      default:
        __builtin_unreachable();
      }

      return 20;
    }
  }

  return 0;
}

// TODO
auto CPU::stop(uint8_t opcode) -> uint8_t {
  reg.pc++;
  return 4;
}

auto CPU::rlc(uint8_t opcode) -> uint8_t {
  uint8_t src = opcode & 0x07;
  uint8_t val = read_reg(src);

  uint8_t carry = ((val >> 7) & 1);
  uint8_t result = (val << 1) | carry;
  write_reg(src, result);

  reg.set_flag(gb::mem::FLAG_Z, result == 0);
  reg.set_flag(gb::mem::FLAG_N, false);
  reg.set_flag(gb::mem::FLAG_H, false);
  reg.set_flag(gb::mem::FLAG_C, carry != 0);

  if (src == 6) {
    return 16;
  }
  return 8;
}

auto CPU::rrc(uint8_t opcode) -> uint8_t {
  uint8_t src = opcode & 0x07;
  uint8_t val = read_reg(src);

  uint8_t carry = (val & 1);
  uint8_t result = (val >> 1) | (carry << 7);
  write_reg(src, result);

  reg.set_flag(gb::mem::FLAG_Z, result == 0);
  reg.set_flag(gb::mem::FLAG_N, false);
  reg.set_flag(gb::mem::FLAG_H, false);
  reg.set_flag(gb::mem::FLAG_C, carry != 0);

  if (src == 6) {
    return 16;
  }
  return 8;
}

auto CPU::sla(uint8_t opcode) -> uint8_t {
  uint8_t src = opcode & 0x07;
  uint8_t val = read_reg(src);

  uint8_t carry = ((val >> 7) & 1);

  uint8_t result = (val << 1);
  write_reg(src, result);

  reg.set_flag(gb::mem::FLAG_Z, result == 0);
  reg.set_flag(gb::mem::FLAG_N, false);
  reg.set_flag(gb::mem::FLAG_H, false);
  reg.set_flag(gb::mem::FLAG_C, carry != 0);

  if (src == 6) {
    return 16;
  }
  return 8;
}

auto CPU::rl(uint8_t opcode) -> uint8_t {
  uint8_t src = opcode & 0x07;
  uint8_t val = read_reg(src);

  uint8_t old_carry = reg.get_flag(gb::mem::FLAG_C) ? 1 : 0;
  uint8_t carry = ((val >> 7) & 1);

  uint8_t result = (val << 1) | old_carry;
  write_reg(src, result);

  reg.set_flag(gb::mem::FLAG_Z, result == 0);
  reg.set_flag(gb::mem::FLAG_N, false);
  reg.set_flag(gb::mem::FLAG_H, false);
  reg.set_flag(gb::mem::FLAG_C, carry != 0);

  if (src == 6) {
    return 16;
  }
  return 8;
}

auto CPU::sra(uint8_t opcode) -> uint8_t {
  uint8_t src = opcode & 0x07;
  uint8_t val = read_reg(src);

  uint8_t carry = val & 1;

  uint8_t result = (val >> 1) | (val & 0x80);
  write_reg(src, result);

  reg.set_flag(gb::mem::FLAG_Z, result == 0);
  reg.set_flag(gb::mem::FLAG_N, false);
  reg.set_flag(gb::mem::FLAG_H, false);
  reg.set_flag(gb::mem::FLAG_C, carry != 0);

  if (src == 6) {
    return 16;
  }
  return 8;
}

auto CPU::swap(uint8_t opcode) -> uint8_t {
  uint8_t src = opcode & 0x07;
  uint8_t val = read_reg(src);

  uint8_t result = (val << 4) | (val >> 4);
  write_reg(src, result);

  reg.set_flag(gb::mem::FLAG_Z, result == 0);
  reg.set_flag(gb::mem::FLAG_N, false);
  reg.set_flag(gb::mem::FLAG_H, false);
  reg.set_flag(gb::mem::FLAG_C, false);

  if (src == 6) {
    return 16;
  }
  return 8;
}

auto CPU::srl(uint8_t opcode) -> uint8_t {
  uint8_t src = opcode & 0x07;
  uint8_t val = read_reg(src);

  uint8_t carry = val & 1;
  uint8_t result = val >> 1;

  write_reg(src, result);

  reg.set_flag(gb::mem::FLAG_Z, result == 0);
  reg.set_flag(gb::mem::FLAG_N, false);
  reg.set_flag(gb::mem::FLAG_H, false);
  reg.set_flag(gb::mem::FLAG_C, carry != 0);

  return (src == 6) ? 16 : 8;
}

auto CPU::rr(uint8_t opcode) -> uint8_t {
  uint8_t src = opcode & 0x07;
  uint8_t val = read_reg(src);

  uint8_t old_carry = reg.get_flag(gb::mem::FLAG_C) ? 1 : 0;
  uint8_t carry = val & 1;

  uint8_t result = (val >> 1) | (old_carry << 7);
  write_reg(src, result);

  reg.set_flag(gb::mem::FLAG_Z, result == 0);
  reg.set_flag(gb::mem::FLAG_N, false);
  reg.set_flag(gb::mem::FLAG_H, false);
  reg.set_flag(gb::mem::FLAG_C, carry != 0);

  if (src == 6) {
    return 16;
  }
  return 8;
}

auto CPU::bit(uint8_t opcode) -> uint8_t {
  uint8_t src = opcode & 0x07;
  uint8_t bit = (opcode >> 3) & 0x07;

  reg.set_flag(gb::mem::FLAG_Z, (read_reg(src) & (1 << bit)) == 0);
  reg.set_flag(gb::mem::FLAG_N, false);
  reg.set_flag(gb::mem::FLAG_H, true);

  if (src == 6) {
    return 12;
  }
  return 8;
}

auto CPU::res(uint8_t opcode) -> uint8_t {
  uint8_t src = opcode & 0x07;
  uint8_t bit = (opcode >> 3) & 0x07;

  uint8_t val = (read_reg(src) & ~(1 << bit));

  write_reg(src, val);

  if (src == 6) {
    return 16;
  }
  return 8;
}

auto CPU::set(uint8_t opcode) -> uint8_t {
  uint8_t src = opcode & 0x07;
  uint8_t bit = (opcode >> 3) & 0x07;

  uint8_t val = (read_reg(src) | (1 << bit));

  write_reg(src, val);

  if (src == 6) {
    return 16;
  }
  return 8;
}

auto CPU::ld_sp_hl(uint8_t opcode) -> uint8_t {
  reg.sp = reg.hl();

  return 8;
}

auto CPU::ld_hl_sp_i8(uint8_t opcode) -> uint8_t {
  auto offset = static_cast<int8_t>(bus.read(reg.pc++));

  uint16_t sp = reg.sp;
  uint16_t result = sp + offset;

  reg.set_flag(gb::mem::FLAG_Z, false);
  reg.set_flag(gb::mem::FLAG_N, false);

  reg.set_flag(gb::mem::FLAG_H, ((sp & 0xF) + (offset & 0xF)) > 0xF);

  reg.set_flag(gb::mem::FLAG_C, ((sp & 0xFF) + (offset & 0xFF)) > 0xFF);

  reg.set_hl(result);

  return 12;
}

auto CPU::add_sp_i8(uint8_t opcode) -> uint8_t {
  auto offset = static_cast<int8_t>(bus.read(reg.pc++));

  uint16_t sp = reg.sp;
  uint16_t result = sp + offset;

  reg.set_flag(gb::mem::FLAG_Z, false);
  reg.set_flag(gb::mem::FLAG_N, false);

  reg.set_flag(gb::mem::FLAG_H, ((sp & 0xF) + (offset & 0xF)) > 0xF);

  reg.set_flag(gb::mem::FLAG_C, ((sp & 0xFF) + (offset & 0xFF)) > 0xFF);

  reg.sp = result;

  return 16;
}

auto CPU::ld_i16_a(uint8_t opcode) -> uint8_t {
  uint8_t low = bus.read(reg.pc++);
  uint8_t high = bus.read(reg.pc++);

  uint16_t addr = (high << 8) | low;

  bus.write(addr, reg.a);
  return 16;
}

auto CPU::ld_a_i16(uint8_t opcode) -> uint8_t {
  uint8_t low = bus.read(reg.pc++);
  uint8_t high = bus.read(reg.pc++);

  uint16_t addr = (high << 8) | low;

  reg.a = bus.read(addr);
  return 16;
}

auto CPU::ldh_i8_a(uint8_t opcode) -> uint8_t {
  uint8_t offset = bus.read(reg.pc++);
  bus.write(0xFF00 + offset, reg.a);
  return 12;
}

auto CPU::ldh_a_i8(uint8_t opcode) -> uint8_t {
  uint8_t offset = bus.read(reg.pc++);
  reg.a = bus.read(0xFF00 + offset);
  return 12;
}

auto CPU::ldh_c_a(uint8_t opcode) -> uint8_t {
  bus.write(0xFF00 + reg.c, reg.a);
  return 8;
}

auto CPU::ldh_a_c(uint8_t opcode) -> uint8_t {
  reg.a = bus.read(0xFF00 + reg.c);
  return 8;
}

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

auto CPU::jp_i16(uint8_t opcode) -> uint8_t {
  uint8_t low = bus.read(reg.pc++);
  uint8_t high = bus.read(reg.pc++);

  uint16_t addr = (high << 8) | low;
  reg.pc = addr;
  return 16;
}

auto CPU::jp_cond_i16(uint8_t opcode) -> uint8_t {
  uint8_t low = bus.read(reg.pc++);
  uint8_t high = bus.read(reg.pc++);

  uint16_t addr = (high << 8) | low;

  bool do_jump = false;
  switch ((opcode >> 3) & 0x03) {
  case 0:
    do_jump = !reg.get_flag(gb::mem::FLAG_Z);
    break; // NZ
  case 1:
    do_jump = reg.get_flag(gb::mem::FLAG_Z);
    break; // Z
  case 2:
    do_jump = !reg.get_flag(gb::mem::FLAG_C);
    break; // NC
  case 3:
    do_jump = reg.get_flag(gb::mem::FLAG_C);
    break; // C
  default:
    __builtin_unreachable();
  }

  if (do_jump) {
    reg.pc = addr;
    return 16; // taken
  }
  return 12; // not taken
}

auto CPU::jr(uint8_t opcode) -> uint8_t {
  auto offset = static_cast<int8_t>(bus.read(reg.pc++));
  reg.pc += offset;
  return 12; // 12 cycles for JR n
}

auto CPU::jp_hl(uint8_t opcode) -> uint8_t {
  reg.pc = reg.hl();
  return 4;
}

auto CPU::jr_cond(uint8_t opcode) -> uint8_t {
  auto offset = static_cast<int8_t>(bus.read(reg.pc++));
  bool do_jump = false;

  switch ((opcode >> 3) & 0x03) {
  case 0:
    do_jump = !reg.get_flag(gb::mem::FLAG_Z);
    break; // NZ
  case 1:
    do_jump = reg.get_flag(gb::mem::FLAG_Z);
    break; // Z
  case 2:
    do_jump = !reg.get_flag(gb::mem::FLAG_C);
    break; // NC
  case 3:
    do_jump = reg.get_flag(gb::mem::FLAG_C);
    break; // C
  default:
    __builtin_unreachable();
  }

  if (do_jump) {
    reg.pc += offset;
    return 12; // taken
  }
  return 8; // not taken
}

auto CPU::reti(uint8_t opcode) -> uint8_t {
  uint8_t low = bus.read(reg.sp++);
  uint8_t high = bus.read(reg.sp++);
  reg.pc = (high << 8) | low;
  ime = true; // enable interrupts
  return 16;
}

auto CPU::ret(uint8_t opcode) -> uint8_t {
  uint8_t low = bus.read(reg.sp++);
  uint8_t high = bus.read(reg.sp++);
  reg.pc = (high << 8) | low;
  return 16;
}

auto CPU::ret_cond(uint8_t opcode) -> uint8_t {
  bool do_ret = false;
  switch ((opcode >> 3) & 0x03) {
  case 0:
    do_ret = !reg.get_flag(gb::mem::FLAG_Z);
    break; // NZ
  case 1:
    do_ret = reg.get_flag(gb::mem::FLAG_Z);
    break; // Z
  case 2:
    do_ret = !reg.get_flag(gb::mem::FLAG_C);
    break; // NC
  case 3:
    do_ret = reg.get_flag(gb::mem::FLAG_C);
    break; // C
  default:
    __builtin_unreachable();
  }

  if (do_ret) {
    uint8_t low = bus.read(reg.sp++);
    uint8_t high = bus.read(reg.sp++);
    reg.pc = (high << 8) | low;
    return 20; // taken
  }
  return 8; // not taken
}

// Unconditional 16-bit call
auto CPU::call(uint8_t opcode) -> uint8_t {
  uint8_t low = bus.read(reg.pc++);
  uint8_t high = bus.read(reg.pc++);
  uint16_t addr = (high << 8) | low;

  // Push current PC onto stack (little-endian)
  push_stack_u16(reg.pc);

  // Jump to target
  reg.pc = addr;
  return 24;
}

auto CPU::push_r16(uint8_t opcode) -> uint8_t {
  switch ((opcode >> 4) & 0x3) {
  case 0:
    push_stack_u16(reg.bc());
    break;
  case 1:
    push_stack_u16(reg.de());
    break;
  case 2:
    push_stack_u16(reg.hl());
    break;
  case 3:
    push_stack_u16(reg.af());
    break;
  default:
    __builtin_unreachable();
  }
  return 16;
}

auto CPU::pop_r16(uint8_t opcode) -> uint8_t {
  uint16_t value = pop();

  switch ((opcode >> 4) & 0x3) {
  case 0:
    reg.set_bc(value);
    break;
  case 1:
    reg.set_de(value);
    break;
  case 2:
    reg.set_hl(value);
    break;
  case 3:
    reg.set_af(value & 0xFFF0);
    break; // lower 4 bits forced 0
  default:
    __builtin_unreachable();
  }

  return 12;
}

auto CPU::pop() -> uint16_t {
  uint8_t lo = bus.read(reg.sp++);
  uint8_t hi = bus.read(reg.sp++);
  return (hi << 8) | lo;
}

void CPU::push_stack_u16(uint16_t val) {
  reg.sp -= 2;
  bus.write(reg.sp, val & 0xFF);     // low byte
  bus.write(reg.sp + 1, (val >> 8)); // high byte
}

// Conditional 16-bit call
auto CPU::call_cond(uint8_t opcode) -> uint8_t {
  uint8_t low = bus.read(reg.pc++);
  uint8_t high = bus.read(reg.pc++);
  uint16_t addr = (high << 8) | low;

  bool do_call = false;
  switch ((opcode >> 3) & 0x03) {
  case 0:
    do_call = !reg.get_flag(gb::mem::FLAG_Z);
    break; // NZ
  case 1:
    do_call = reg.get_flag(gb::mem::FLAG_Z);
    break; // Z
  case 2:
    do_call = !reg.get_flag(gb::mem::FLAG_C);
    break; // NC
  case 3:
    do_call = reg.get_flag(gb::mem::FLAG_C);
    break; // C
  default:
    __builtin_unreachable();
  }

  if (do_call) {
    // Push current PC and jump
    push_stack_u16(reg.pc);
    reg.pc = addr;
    return 24; // taken
  }

  return 12; // not taken
}

// Reset to fixed vector (rst)
auto CPU::rst(uint8_t opcode) -> uint8_t {
  // RST target address = opcode & 0x38
  uint16_t addr = opcode & 0x38;

  // Push current PC
  push_stack_u16(reg.pc);

  // Jump to fixed address
  reg.pc = addr;
  return 16;
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

auto CPU::illegal(uint8_t opcode) -> uint8_t {
  std::cerr << "Illegal opcode: 0x" << std::hex << std::uppercase
            << std::setw(2) << std::setfill('0') << static_cast<int>(opcode)
            << " at PC=0x" << std::setw(4) << reg.pc - 1 << "\n";
  std::abort();
}

void CPU::dump_opcode_table() const {
  std::cout << "Operator Table:\n";
  for (int i = 0; i < 256; ++i) {
    if (op_table[i] == &CPU::illegal) {
      std::cout << "Missing opcode: 0x" << std::hex << std::uppercase
                << std::setw(2) << std::setfill('0') << i << "\n";
    }
  }
  std::cout << "Prefix Table:\n";
  for (int i = 0; i < 256; ++i) {
    if (cb_table[i] == &CPU::illegal) {
      std::cout << "Missing opcode: 0x" << std::hex << std::uppercase
                << std::setw(2) << std::setfill('0') << i << "\n";
    }
  }
}
