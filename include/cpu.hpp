#pragma once

#include <array>
#include <cstdint>
struct CPUReg {
  uint8_t a;
  uint8_t f;
  uint8_t b;
  uint8_t c;
  uint8_t d;
  uint8_t e;
  uint8_t h;
  uint8_t l;

  uint16_t sp;
  uint16_t pc;

  [[nodiscard]] auto read_af() -> uint16_t { return (a << 8) | f; }
  [[nodiscard]] auto read_bc() -> uint16_t { return (b << 8) | c; }
  [[nodiscard]] auto read_de() -> uint16_t { return (d << 8) | e; }
  [[nodiscard]] auto read_hl() -> uint16_t { return (h << 8) | l; }

  void set_af(uint16_t val) {
    a = val >> 8;
    f = val & 0xFF;
  }
  void set_bc(uint16_t val) {
    b = val >> 8;
    c = val & 0xFF;
  }
  void set_de(uint16_t val) {
    d = val >> 8;
    e = val & 0xFF;
  }
  void set_hl(uint16_t val) {
    h = val >> 8;
    l = val & 0xFF;
  }
};

class CPU {
  using Inst = void (CPU::*)(uint8_t);

private:
  CPUReg reg{};
  std::array<Inst, 0xFF> op_table{}, pre_table{};
  void initialize_tables();
  void assign_op(uint8_t, Inst);
  void assign_pre(uint8_t, Inst);

  auto read_r8(uint8_t) -> uint8_t;
  void write_r8(uint8_t, uint8_t);

  void illegal(uint8_t);

  void ld_r8_r8(uint8_t);

public:
  CPU();
  void dump_missing_op();
};
