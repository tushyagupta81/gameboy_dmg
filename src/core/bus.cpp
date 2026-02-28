#include "bus.hpp"
#include <cstdint>
#include <cstdlib>
#include <iostream>

auto Bus::read(uint16_t addr) -> uint8_t {
  if (addr < 0x8000) {
    return rom[addr];
  }
  if (addr >= 0x8000 && addr < 0xA000) {
    return vram[addr - 0x8000];
  }
  if (addr >= 0xA000 && addr < 0xC000) {
    return exram[addr - 0xA000];
  }
  if (addr >= 0xC000 && addr < 0xE000) {
    return wram[addr - 0xC000];
  }
  if (addr >= 0xE000 && addr < 0xFE00) {
    std::cerr << "Access to echo ram\n";
    abort();
  }
  if (addr >= 0xFE00 && addr < 0xFF00) {
    std::cerr << "Access to unaccessable ram\n";
    abort();
  }
  if (addr >= 0xFF00 && addr < 0xFF80) {
    if (addr >= 0xFF04 && addr <= 0xFF07) {
      return timer.read(addr);
    }
    return io[addr - 0xFF00];
  }
  if (addr >= 0xFF80 && addr < 0xFFFF) {
    return hram[addr - 0xFF80];
  }
  if (addr == 0xFFFF) {
    return ie;
  }

  __builtin_unreachable();
}

void Bus::write(uint16_t addr, uint8_t val) {
  if (addr < 0x8000) {
    rom[addr] = val;
  }
  if (addr >= 0x8000 && addr < 0xA000) {
    vram[addr - 0x8000] = val;
  }
  if (addr >= 0xA000 && addr < 0xC000) {
    exram[addr - 0xA000] = val;
  }
  if (addr >= 0xC000 && addr < 0xE000) {
    wram[addr - 0xC000] = val;
  }
  if (addr >= 0xE000 && addr < 0xFE00) {
    std::cerr << "Access to echo ram\n";
    abort();
  }
  if (addr >= 0xFE00 && addr < 0xFF00) {
    std::cerr << "Access to unaccessable ram\n";
    abort();
  }
  if (addr >= 0xFF00 && addr < 0xFF80) {
    if (addr >= 0xFF04 && addr <= 0xFF07) {
      timer.write(addr, val);
      return;
    }
    io[addr - 0xFF00] = val;
  }
  if (addr >= 0xFF80 && addr < 0xFFFF) {
    hram[addr - 0xFF80] = val;
  }
  if (addr == 0xFFFF) {
    ie = val;
  }
}

void Bus::enable_interrupt(uint8_t bit) {
  write(0xFF0F, (read(0xFF0F) | (1 << bit)));
}
