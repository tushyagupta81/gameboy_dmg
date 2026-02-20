#include "bus.hpp"
#include "memory.hpp"
#include "memory_map.hpp"
#include <algorithm>
#include <cstdint>
#include <iostream>
#include <vector>

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
    if (addr >= 0xFF04 && addr <= 0xFF07) {
      return timer.read(addr);
    }
    if (is_unused_io(addr)) {
      return OPEN_BUS; // or OPEN_BUS if you prefer
    }
    return mem.io[addr - IO_START];
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
    if (addr >= 0xFF04 && addr <= 0xFF07) {
      timer.write(addr, value);
      return;
    }
    if (is_unused_io(addr)) {
      return; // ignore write
    }

    mem.io[addr - IO_START] = value;
    if (addr == 0xFF02 && value == 0x81) {
      char c = static_cast<char>(mem.io[0xFF01 - IO_START]);
      std::cout << c << std::flush;
    }
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

Bus::Bus() {
  using namespace gb::mem;
  // Timer
  mem.io[0xFF05 - IO_START] = 0x00; // TIMA
  mem.io[0xFF06 - IO_START] = 0x00; // TMA
  mem.io[0xFF07 - IO_START] = 0x00; // TAC
  timer.write_div_raw(0xAB);        // DIV

  // Sound
  mem.io[0xFF10 - IO_START] = 0x80;
  mem.io[0xFF11 - IO_START] = 0xBF;
  mem.io[0xFF12 - IO_START] = 0xF3;
  mem.io[0xFF14 - IO_START] = 0xBF;
  mem.io[0xFF16 - IO_START] = 0x3F;
  mem.io[0xFF17 - IO_START] = 0x00;
  mem.io[0xFF19 - IO_START] = 0xBF;
  mem.io[0xFF1A - IO_START] = 0x7F;
  mem.io[0xFF1B - IO_START] = 0xFF;
  mem.io[0xFF1C - IO_START] = 0x9F;
  mem.io[0xFF1E - IO_START] = 0xBF;
  mem.io[0xFF20 - IO_START] = 0xFF;
  mem.io[0xFF21 - IO_START] = 0x00;
  mem.io[0xFF22 - IO_START] = 0x00;
  mem.io[0xFF23 - IO_START] = 0xBF;
  mem.io[0xFF24 - IO_START] = 0x77;
  mem.io[0xFF25 - IO_START] = 0xF3;
  mem.io[0xFF26 - IO_START] = 0xF1;

  // PPU
  mem.io[0xFF40 - IO_START] = 0x91;
  mem.io[0xFF41 - IO_START] = 0x81; // FIXED
  mem.io[0xFF42 - IO_START] = 0x00;
  mem.io[0xFF43 - IO_START] = 0x00;
  mem.io[0xFF44 - IO_START] = 0x00;
  mem.io[0xFF45 - IO_START] = 0x00;
  mem.io[0xFF47 - IO_START] = 0xFC;
  mem.io[0xFF48 - IO_START] = 0xFF;
  mem.io[0xFF49 - IO_START] = 0xFF;
  mem.io[0xFF4A - IO_START] = 0x00;
  mem.io[0xFF4B - IO_START] = 0x00;

  // Interrupts
  mem.io[0xFF0F - IO_START] = 0xE1;
  mem.ie = 0x00;

  timer.connect_interrupt_flag(&mem.io[0xFF0F - gb::mem::IO_START]);
}

void Bus::timer_tick(int cycles) { timer.tick(cycles); }

auto Bus::is_unused_io(uint16_t addr) const -> bool {
  static const std::vector<uint16_t> unused = {
      0xFF03, 0xFF08, 0xFF0C, 0xFF0E, 0xFF0F, 0xFF10, 0xFF11, 0xFF12,
      0xFF14, 0xFF16, 0xFF17, 0xFF19, 0xFF1A, 0xFF1B, 0xFF1C, 0xFF1E,
      0xFF20, 0xFF21, 0xFF22, 0xFF23, 0xFF24, 0xFF25, 0xFF26};

  return std::find(std::begin(unused), std::end(unused), addr) !=
         std::end(unused);
}
