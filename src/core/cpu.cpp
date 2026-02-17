#include "cpu.hpp"
#include "memory_map.hpp"
#include <cstdint>
#include <fstream>
#include <iostream>

CPU::CPU(const std::string &rom_path)
    : halted(false), halt_bug(false), ime(false), ime_enable_pending(false) {
  reg.a = reg.f = 0;
  reg.b = reg.c = 0;
  reg.d = reg.e = 0;
  reg.h = reg.l = 0;

  reg.sp = 0xFFFE; // typical GB start SP
  reg.pc = 0x0100; // typical GB entry point

  // Clear flags
  reg.clear_flags();

  // Initialize opcode tables
  init_tables();

  std::ifstream file(rom_path, std::ios::binary);
  if (!file) {
    throw std::runtime_error("Failed to open ROM: " + rom_path);
  }

  std::vector<uint8_t> rom((std::istreambuf_iterator<char>(file)),
                           std::istreambuf_iterator<char>());

  bus.load_rom(rom);

  std::cout << "Initializing CPU\n";
}

auto CPU::step() -> uint8_t {
  if (ime_enable_pending) {
    ime = true;
    ime_enable_pending = false;
  }

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
  op_table[0x00] = &CPU::nop;
  op_table[0x01] = &CPU::stop;

  op_table[0x18] = &CPU::jr;

  op_table[0x07] = &CPU::rlca;
  op_table[0x17] = &CPU::rla;
  op_table[0x0F] = &CPU::rrca;
  op_table[0x1F] = &CPU::rra;
  op_table[0x27] = &CPU::daa;
  op_table[0x2F] = &CPU::cpl;
  op_table[0x37] = &CPU::scf;
  op_table[0x3F] = &CPU::ccf;

  for (uint8_t i = 0x20; i <= 0x38; i += 8) {
    op_table[i] = &CPU::jr_cond;
  }

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
  // 3 tables done
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

  op_table[0xEA] = &CPU::jp_hl;
  op_table[0xC3] = &CPU::jp_i16;
  for (uint8_t i = 0xC2; i <= 0xDA; i += 8) {
    op_table[i] = &CPU::jp_cond_i16;
  }

  for (uint8_t i = 0xC0; i <= 0xD8; i += 8) {
    op_table[i] = &CPU::ret_cond;
  }
  // RET unconditional
  op_table[0xC9] = &CPU::ret;
  // RETI (return from interrupt)
  op_table[0xD9] = &CPU::reti;

  for (uint8_t i = 0xC4; i <= 0xDC; i += 8) {
    op_table[i] = &CPU::call_cond;
  }
  // call unconditional
  op_table[0xCD] = &CPU::call;
  // rst instructions
  for (uint8_t i = 0xC7; i <= 0xFF; i += 0x08) {
    op_table[i] = &CPU::rst;
  }
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
  halted = true;
  return 4;
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
