#include "bus.hpp"
#include "memory.hpp"
#include "memory_map.hpp"
#include <algorithm>
#include <cstdint>
#include <iostream>

auto Bus::read(uint16_t addr) const -> uint8_t {
  // std::cout<<std::hex<<addr<<'\n';
  using namespace gb::mem;

  if (addr <= ROM_END) {
    return mem.rom[addr];
  }

  if (addr >= VRAM_START && addr <= VRAM_END) {
    return mem.vram[addr - VRAM_START];
  }

  if (addr >= EXRAM_START && addr <= EXRAM_END) {
    return mem.exram[addr - EXRAM_START];
  }

  if (addr >= WRAM_START && addr <= WRAM_END) {
    return mem.wram[addr - WRAM_START];
  }

  if (addr >= ECHO_START && addr <= ECHO_END) {
    return mem.wram[addr - ECHO_START]; // mirror
  }

  if (addr >= OAM_START && addr <= OAM_END) {
    return mem.oam[addr - OAM_START];
  }

  if (addr >= UNUSED_START && addr <= UNUSED_END) {
    return OPEN_BUS;
  }

  if (addr >= IO_START && addr <= IO_END) {
    return mem.io[addr - IO_START];
    // return OPEN_BUS; // not implemented yet
  }

  if (addr >= HRAM_START && addr <= HRAM_END) {
    return mem.hram[addr - HRAM_START];
  }

  if (addr == IE_REG) {
    return mem.ie;
  }

  return OPEN_BUS;
}

void Bus::write(uint16_t addr, uint8_t value) {
  // std::cout<<std::hex<<addr<<'\n';
  using namespace gb::mem;

  // 0000–7FFF : ROM (usually MBC control)
  if (addr <= ROM_END) {
    // TODO: route to cartridge / MBC
    return;
  }

  // 8000–9FFF : VRAM
  if (addr >= VRAM_START && addr <= VRAM_END) {
    mem.vram[addr - VRAM_START] = value;
    return;
  }

  // A000–BFFF : External RAM
  if (addr >= EXRAM_START && addr <= EXRAM_END) {
    mem.exram[addr - EXRAM_START] = value;
    return;
  }

  // C000–DFFF : WRAM
  if (addr >= WRAM_START && addr <= WRAM_END) {
    mem.wram[addr - WRAM_START] = value;
    return;
  }

  // E000–FDFF : Echo RAM (mirror of C000–DDFF)
  if (addr >= ECHO_START && addr <= ECHO_END) {
    mem.wram[addr - ECHO_START] = value;
    return;
  }

  // FE00–FE9F : OAM
  if (addr >= OAM_START && addr <= OAM_END) {
    mem.oam[addr - OAM_START] = value;
    return;
  }

  // FEA0–FEFF : Unusable
  if (addr >= UNUSED_START && addr <= UNUSED_END) {
    return; // writes ignored
  }

  // FF00–FF7F : IO Registers
  if (addr >= IO_START && addr <= IO_END) {
    mem.io[addr - IO_START] = value;

    if (addr == 0xFF02 && value == 0x81) {
      char c = static_cast<char>(mem.io[0xFF01 - IO_START]);
      std::cout << c << std::flush;
    }

    // TODO: delegate to IO subsystem
    return;
  }

  // FF80–FFFE : HRAM
  if (addr >= HRAM_START && addr <= HRAM_END) {
    mem.hram[addr - HRAM_START] = value;
    return;
  }

  // FFFF : Interrupt Enable
  if (addr == IE_REG) {
    mem.ie = value;
    return;
  }
}

void Bus::load_rom(const std::vector<uint8_t> &rom_data) {
  size_t size_to_copy = std::min(rom_data.size(), mem.rom.size());
  std::copy_n(rom_data.begin(), size_to_copy, mem.rom.begin());
}
