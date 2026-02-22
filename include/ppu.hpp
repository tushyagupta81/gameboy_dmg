#pragma once

#include <array>
#include <cstdint>

#define VRAM_SIZE 0x2000
#define OAM_SIZE 0xA0

inline constexpr std::array<uint32_t, 4> dmg_palette = {0xFFFFFFFF, 0xFFAAAAAA,
                                                        0xFF555555, 0xFF000000};

class Bus;

class PPU {
private:
  void set_mode(uint8_t new_mode);
  void evaluate_ly_lyc();
  void render_scanline();
  void render_bg();
  void render_window();
  void render_sprites();

  // Timing
  uint16_t dot; // 0–455 (current scanline cycle)
  uint8_t ly;   // FF44
  uint8_t mode; // 0–3

  // Registers (memory-mapped)
  uint8_t lcdc; // FF40
  uint8_t stat; // FF41
  uint8_t scy;  // FF42
  uint8_t scx;  // FF43
  uint8_t lyc;  // FF45
  uint8_t wy;   // FF4A
  uint8_t wx;   // FF4B
  uint8_t bgp;  // FF47
  uint8_t obp0; // FF48
  uint8_t obp1; // FF49

  std::array<uint32_t, 23040> framebuffer;

  std::array<uint8_t, VRAM_SIZE> vram;
  std::array<uint8_t, OAM_SIZE> oam;

  Bus *bus;

public:
  PPU(Bus &);
  void tick();
  [[nodiscard]] auto read(uint16_t addr) const -> uint8_t;
  void write(uint16_t addr, uint8_t val);

  [[nodiscard]] auto get_framebuffer() -> std::array<uint32_t, 23040> &;
};
