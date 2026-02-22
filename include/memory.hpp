#pragma once

#include "memory_map.hpp"
#include <array>
#include <cstdint>

#define F_MASK 0xF0
#define ROM_SIZE 0x8000
#define EXRAM_SIZE 0x2000
#define WRAM_SIZE 0x2000
#define IO_SIZE 0x7F
#define HRAM_SIZE 0x7F

struct Register {
  uint8_t a, f;
  uint8_t b, c;
  uint8_t d, e;
  uint8_t h, l;

  [[nodiscard]] auto af() const -> uint16_t { return (a << 8) | (f & F_MASK); }
  [[nodiscard]] auto bc() const -> uint16_t { return (b << 8) | c; }
  [[nodiscard]] auto de() const -> uint16_t { return (d << 8) | e; }
  [[nodiscard]] auto hl() const -> uint16_t { return (h << 8) | l; }

  void set_af(uint16_t val) {
    a = static_cast<uint8_t>(val >> 8);
    f = static_cast<uint8_t>(val & 0xF0);
  }

  void set_bc(uint16_t val) {
    b = static_cast<uint8_t>(val >> 8);
    c = static_cast<uint8_t>(val);
  }

  void set_de(uint16_t val) {
    d = static_cast<uint8_t>(val >> 8);
    e = static_cast<uint8_t>(val);
  }

  void set_hl(uint16_t val) {
    h = static_cast<uint8_t>(val >> 8);
    l = static_cast<uint8_t>(val);
  }

  void set_flag(uint8_t flag, bool value) {
    if (value) {
      f |= flag;
    } else {
      f &= ~flag;
    }

    f &= F_MASK; // ensure lower bits stay 0
  }

  [[nodiscard]] auto get_flag(uint8_t flag) const -> bool {
    return (f & flag) != 0;
  }

  void clear_flags() { f = 0; }

  void set_flags(bool z, bool n, bool h, bool c) {
    using namespace gb::mem;
    f = 0;
    if (z) {
      f |= FLAG_Z;
    }
    if (n) {
      f |= FLAG_N;
    }
    if (h) {
      f |= FLAG_H;
    }
    if (c) {
      f |= FLAG_C;
    }
  }

  uint16_t sp;
  uint16_t pc;
};

struct Memory {
  // 0000 -> 7FFF
  std::array<uint8_t, ROM_SIZE> rom;

  // 8000 -> 9FFF

  // A000 -> BFFF
  std::array<uint8_t, EXRAM_SIZE> exram;

  // C000 -> DFFF
  std::array<uint8_t, WRAM_SIZE> wram;

  // FE00 -> FE9F

  std::array<uint8_t, IO_SIZE> io;

  // FF80 -> FFFE
  std::array<uint8_t, HRAM_SIZE> hram;

  // FFFF
  uint8_t ie;
};
