#pragma once
#include <cstdint>

namespace gb::mem {

constexpr uint16_t ROM_START = 0x0000;
constexpr uint16_t ROM_END = 0x7FFF;

constexpr uint16_t VRAM_START = 0x8000;
constexpr uint16_t VRAM_END = 0x9FFF;

constexpr uint16_t EXRAM_START = 0xA000;
constexpr uint16_t EXRAM_END = 0xBFFF;

constexpr uint16_t WRAM_START = 0xC000;
constexpr uint16_t WRAM_END = 0xDFFF;

constexpr uint16_t ECHO_START = 0xE000;
constexpr uint16_t ECHO_END = 0xFDFF;

constexpr uint16_t OAM_START = 0xFE00;
constexpr uint16_t OAM_END = 0xFE9F;

constexpr uint16_t UNUSED_START = 0xFEA0;
constexpr uint16_t UNUSED_END = 0xFEFF;

constexpr uint16_t IO_START = 0xFF00;
constexpr uint16_t IO_END = 0xFF7F;

constexpr uint16_t HRAM_START = 0xFF80;
constexpr uint16_t HRAM_END = 0xFFFE;

constexpr uint16_t IE_REG = 0xFFFF;

constexpr uint8_t OPEN_BUS = 0xFF;   // Default unmapped read value
constexpr uint8_t UNUSED_READ = 0xFF;

} // namespace gb::mem
