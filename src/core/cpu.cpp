#include "cpu.hpp"
#include <cstdint>
#include <iomanip>
#include <iostream>

CPU::CPU() {
  reg.set_af(0x01B0);
  reg.set_bc(0x0013);
  reg.set_de(0x00D8);
  reg.set_hl(0x014D);
  reg.sp = 0xFFFE;
  reg.pc = 0x0100;

  initialize_tables();
}

void CPU::assign_op(uint8_t opcode, Inst inst_fn) {
  if (op_table[opcode] == &CPU::illegal) {
    op_table[opcode] = inst_fn;
  } else {
    std::cerr << "Opcode 0x" << std::hex << std::uppercase << opcode
              << " already assigned to OP_TABLE\n";
    abort();
  }
}

void CPU::assign_pre(uint8_t opcode, Inst inst_fn) {
  if (pre_table[opcode] == &CPU::illegal) {
    pre_table[opcode] = inst_fn;
  } else {
    std::cerr << "Opcode 0x" << std::hex << std::uppercase << opcode
              << " already assigned to PRE_TABLE\n";
    abort();
  }
}

auto CPU::read_r8(uint8_t src) -> uint8_t {
  switch (src) {
  case 0:
    return reg.b;
    break;
  case 1:
    return reg.c;
    break;
  case 2:
    return reg.d;
    break;
  case 3:
    return reg.e;
    break;
  case 4:
    return reg.h;
    break;
  case 5:
    return reg.l;
    break;
  case 6:
    return bus.read(reg.read_hl());
    break;
  case 7:
    return reg.a;
    break;
  default:
    __builtin_unreachable();
  }
}

void CPU::write_r8(uint8_t src, uint8_t val) {
  switch (src) {
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
    bus.write(reg.read_hl(), val);
    break;
  case 7:
    reg.a = val;
    break;
  default:
    __builtin_unreachable();
  }
}

void CPU::initialize_tables() {
  op_table.fill(&CPU::illegal);
  pre_table.fill(&CPU::illegal);

  for (int i = 0x40; i <= 0x7F; i++) {
    if (i == 0x76) {
      continue;
    }
    assign_op(i, &CPU::ld_r8_r8);
  }
}

void CPU::illegal(uint8_t opcode) {
  std::cerr << "Illegal entry. Opcode 0x" << std::hex << std::uppercase
            << opcode << '\n';
  abort();
}

void CPU::ld_r8_r8(uint8_t opcode) {
  uint8_t src = opcode & 0x03;
  uint8_t dst = (opcode >> 3) & 0x03;

  uint8_t src_val = read_r8(src);

  write_r8(dst, src_val);
}

void CPU::dump_missing_op() {
  std::cout << "=== OP TABLE ===\n";
  for (int i = 0; i < 0x10; i++) {
    for (int j = 0; j < 0x10; j++) {
      int index = (i * 0x10) + j;

      if (op_table[index] == &CPU::illegal) {
        std::cout << "0x" << std::setw(2) << std::setfill('0') << std::hex
                  << std::uppercase << index << " ";
      } else {
        std::cout << "     ";
      }
    }
    std::cout << "\n";
  }
  std::cout << "\n\n=== PRE TABLE ===\n";
  for (int i = 0; i < 0x10; i++) {
    for (int j = 0; j < 0x10; j++) {
      int index = (i * 0x10) + j;

      if (pre_table[index] == &CPU::illegal) {
        std::cout << "0x" << std::setw(2) << std::setfill('0') << std::hex
                  << std::uppercase << index << " ";
      } else {
        std::cout << "     ";
      }
    }
    std::cout << "\n";
  }
}
