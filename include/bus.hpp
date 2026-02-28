#include <array>
#include <cstdint>
class Bus {
private:
  std::array<uint8_t, 0x8000> rom{};
  std::array<uint8_t, 0x2000> vram{};
  std::array<uint8_t, 0x2000> exram{};
  std::array<uint8_t, 0x2000> wram{};
  std::array<uint8_t, 0x2000> echo{};
  std::array<uint8_t, 0xA0> oam{};
  std::array<uint8_t, 0x80> io{};
  std::array<uint8_t, 0x7F> hram{};
  uint8_t ie = 0;

public:
  auto read(uint16_t) -> uint8_t;
  void write(uint16_t, uint8_t);
};
