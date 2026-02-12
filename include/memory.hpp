#pragma once

#include <array>
#include <cstdint>
#include <vector>

#define BYTE_SIZE 8
#define F_MASK 0xF0
#define ROM_SIZE 0x8000
#define VRAM_SIZE 0x2000
#define EXRAM_SIZE 0x2000
#define WRAM_SIZE 0x2000
#define OAM_SIZE 0xA0
#define HRAM_SIZE 0x7F

struct Register {
  uint8_t a, f;
  uint8_t b, c;
  uint8_t d, e;
  uint8_t h, l;

  [[nodiscard]] auto af() const -> uint16_t {
    return (a << BYTE_SIZE) | (f & F_MASK);
  }
  [[nodiscard]] auto bc() const -> uint16_t { return (b << BYTE_SIZE) | c; }
  [[nodiscard]] auto de() const -> uint16_t { return (d << BYTE_SIZE) | e; }
  [[nodiscard]] auto hl() const -> uint16_t { return (h << BYTE_SIZE) | l; }

  uint16_t sp;
  uint16_t pc;
};

struct Memory {
  // 0000 -> 7FFF
  std::vector<uint8_t> rom;

  // 8000 -> 9FFF
  std::array<uint8_t, VRAM_SIZE> vram;

  // A000 -> BFFF
  std::array<uint8_t, EXRAM_SIZE> exram;

  // C000 -> DFFF
  std::array<uint8_t, WRAM_SIZE> wram;

  // FE00 -> FE9F
  std::array<uint8_t, OAM_SIZE> oam;

  // FF80 -> FFFE
  std::array<uint8_t, HRAM_SIZE> hram;

  // FFFF
  uint8_t ie;
};
