#include "ppu.hpp"
#include <array>
#include "bus.hpp"
#include <cstdint>

void PPU::tick() {
  if ((lcdc & 0x80) == 0) {
    // LCD off behavior
    dot = 0;
    ly = 0;
    set_mode(0);
    return;
  }

  dot++;

  switch (mode) {
  case 2:
    if (dot == 80) {
      set_mode(3);
    }
    break;

  case 3:
    if (dot == 80 + 172) {
      render_scanline();
      set_mode(0);
    }
    break;

  case 0:
    if (dot == 456) {
      dot = 0;
      ly++;
      evaluate_ly_lyc();

      if (ly == 144) {
        set_mode(1);
      } else {
        set_mode(2);
      }
    }
    break;

  case 1:
    if (dot == 456) {
      dot = 0;
      ly++;

      if (ly == 154) {
        ly = 0;
        set_mode(2);
      }
    }
    break;
  default:
    __builtin_unreachable();
  }
}

auto PPU::get_framebuffer() -> std::array<uint32_t, 23040> & {
  return framebuffer;
}

PPU::PPU(Bus &bus)
    : bus(&bus), dot(0), ly(0), mode(2), lcdc(0), stat(0), scy(0), scx(0),
      lyc(0), wy(0), wx(0), bgp(0xFC), obp0(0xFF), obp1(0xFF), framebuffer{},
      oam{}, vram{} {
  framebuffer.fill(dmg_palette[0]);
}

auto PPU::read(uint16_t addr) const -> uint8_t {
  // =========================
  // VRAM (8000–9FFF)
  // =========================
  if (addr >= 0x8000 && addr <= 0x9FFF) {
    // Block during Mode 3
    if (mode == 3) {
      return 0xFF;
    }

    return vram[addr - 0x8000];
  }

  // =========================
  // OAM (FE00–FE9F)
  // =========================
  if (addr >= 0xFE00 && addr <= 0xFE9F) {
    // Block during Mode 2 & 3
    if (mode == 2 || mode == 3) {
      return 0xFF;
    }

    return oam[addr - 0xFE00];
  }

  // =========================
  // LCD Registers
  // =========================
  switch (addr) {
  case 0xFF40:
    return lcdc;

  case 0xFF41:
    // Mode bits (0–1) reflect current mode
    // Bit 2 = coincidence flag
    return (stat & 0xF8) | (mode & 0x03);

  case 0xFF42:
    return scy;
  case 0xFF43:
    return scx;

  case 0xFF44:
    // LY is read-only
    return ly;

  case 0xFF45:
    return lyc;

  case 0xFF47:
    return bgp;
  case 0xFF48:
    return obp0;
  case 0xFF49:
    return obp1;

  case 0xFF4A:
    return wy;
  case 0xFF4B:
    return wx;
  default:
    break;
  }

  __builtin_unreachable();
}

void PPU::write(uint16_t addr, uint8_t val) {
  // =========================
  // VRAM (8000–9FFF)
  // =========================
  if (addr >= 0x8000 && addr <= 0x9FFF) {
    // Block during Mode 3
    if (mode == 3) {
      return;
    }

    vram[addr - 0x8000] = val;
    return;
  }

  // =========================
  // OAM (FE00–FE9F)
  // =========================
  if (addr >= 0xFE00 && addr <= 0xFE9F) {
    // Block during Mode 2 & 3
    if (mode == 2 || mode == 3) {
      return;
    }

    oam[addr - 0xFE00] = val;
    return;
  }

  // =========================
  // LCDC (FF40)
  // =========================
  if (addr == 0xFF40) {
    bool was_enabled = (lcdc & 0x80) != 0;
    lcdc = val;

    bool now_enabled = (lcdc & 0x80) != 0;

    // LCD disable behavior
    if (was_enabled && !now_enabled) {
      dot = 0;
      ly = 0;
      mode = 0;
      stat = (stat & 0xFC); // clear mode bits
    }

    return;
  }

  // =========================
  // STAT (FF41)
  // Only upper 5 bits writable
  // =========================
  if (addr == 0xFF41) {
    stat = (val & 0xF8) | (stat & 0x07);
    return;
  }

  // =========================
  // Scroll Registers
  // =========================
  if (addr == 0xFF42) {
    scy = val;
    return;
  }
  if (addr == 0xFF43) {
    scx = val;
    return;
  }

  // =========================
  // LY (FF44)
  // Writing resets LY to 0
  // =========================
  if (addr == 0xFF44) {
    ly = 0;
    dot = 0;
    evaluate_ly_lyc();
    return;
  }

  // =========================
  // LYC (FF45)
  // =========================
  if (addr == 0xFF45) {
    lyc = val;
    evaluate_ly_lyc();
    return;
  }

  // =========================
  // Palettes
  // =========================
  if (addr == 0xFF47) {
    bgp = val;
    return;
  }
  if (addr == 0xFF48) {
    obp0 = val;
    return;
  }
  if (addr == 0xFF49) {
    obp1 = val;
    return;
  }

  // =========================
  // Window Position
  // =========================
  if (addr == 0xFF4A) {
    wy = val;
    return;
  }
  if (addr == 0xFF4B) {
    wx = val;
    return;
  }

  // =========================
  // FF46 DMA (optional later)
  // =========================
  if (addr == 0xFF46) {
    // TODO: implement OAM DMA
    return;
  }
}

void PPU::set_mode(uint8_t new_mode) {
  stat = (stat & 0xFC) | (new_mode & 0x03);

  bool request = false;

  switch (new_mode) {
  case 0: // HBlank
    request = ((stat & (1 << 3)) != 0);
    break;
  case 1: // VBlank
    request = ((stat & (1 << 4)) != 0);
    bus->request_interrupt(0); // VBlank interrupt
    break;
  case 2: // OAM
    request = ((stat & (1 << 5)) != 0);
    break;
  default:
    break;
  }

  if (request) {
    bus->request_interrupt(1); // LCD STAT interrupt
  }
}

void PPU::evaluate_ly_lyc() {
  if (ly == lyc) {
    stat |= (1 << 2);

    if ((stat & (1 << 6)) != 0) {
      bus->request_interrupt(1);
    }
  } else {
    stat &= ~(1 << 2);
  }
}

void PPU::render_scanline() {
  if ((lcdc & 0x80) == 0) {
    return; // LCD off
  }

  render_bg();
  render_window();
  render_sprites();
}

void PPU::render_bg() {
  if ((lcdc & 0x01) == 0) {
    for (int x = 0; x < 160; ++x) {
      framebuffer[(ly * 160) + x] = 0;
    }
    return;
  }

  uint16_t tilemap_base = ((lcdc & 0x08) != 0) ? 0x9C00 : 0x9800;
  uint16_t tiledata_base = ((lcdc & 0x10) != 0) ? 0x8000 : 0x8800;

  for (int x = 0; x < 160; ++x) {
    uint8_t scrolled_x = x + scx;
    uint8_t scrolled_y = ly + scy;

    uint16_t tilemap_index =
        tilemap_base + ((scrolled_y / 8) * 32) + (scrolled_x / 8);

    uint8_t tile_id = vram[tilemap_index - 0x8000];

    uint16_t tile_addr = 0;
    if ((lcdc & 0x10) != 0) {
      tile_addr = tiledata_base + (tile_id * 16);
    } else {
      auto signed_id = static_cast<int8_t>(tile_id);
      tile_addr = 0x9000 + (signed_id * 16);
    }

    uint8_t row = scrolled_y % 8;
    uint8_t low = vram[(tile_addr - 0x8000) + (row * 2)];
    uint8_t high = vram[(tile_addr - 0x8000) + (row * 2) + 1];

    uint8_t bit = 7 - (scrolled_x % 8);
    uint8_t color_id = ((high >> bit) & 1) << 1 | ((low >> bit) & 1);

    framebuffer[(ly * 160) + x] = (bgp >> (color_id * 2)) & 0x03;
  }
}

void PPU::render_window() {
  if ((lcdc & 0x20) == 0) {
    return;
  }
  if (ly < wy) {
    return;
  }

  uint16_t tilemap_base = ((lcdc & 0x40) != 0) ? 0x9C00 : 0x9800;
  uint16_t tiledata_base = ((lcdc & 0x10) != 0) ? 0x8000 : 0x8800;

  uint8_t window_y = ly - wy;

  for (int x = 0; x < 160; ++x) {
    if (x + 7 < wx) {
      continue;
    }

    uint8_t window_x = x - (wx - 7);

    uint16_t tilemap_index =
        tilemap_base + ((window_y / 8) * 32) + (window_x / 8);

    uint8_t tile_id = vram[tilemap_index - 0x8000];

    uint16_t tile_addr = 0;
    if ((lcdc & 0x10) != 0) {
      tile_addr = tiledata_base + (tile_id * 16);
    } else {
      auto signed_id = static_cast<int8_t>(tile_id);
      tile_addr = 0x9000 + (signed_id * 16);
    }

    uint8_t row = window_y % 8;
    uint8_t low = vram[(tile_addr - 0x8000) + (row * 2)];
    uint8_t high = vram[(tile_addr - 0x8000) + (row * 2) + 1];

    uint8_t bit = 7 - (window_x % 8);
    uint8_t color_id = ((high >> bit) & 1) << 1 | ((low >> bit) & 1);

    framebuffer[(ly * 160) + x] = (bgp >> (color_id * 2)) & 0x03;
  }
}

void PPU::render_sprites() {
  if ((lcdc & 0x02) == 0) {
    return;
  }

  uint8_t sprite_height = ((lcdc & 0x04) != 0) ? 16 : 8;
  int drawn = 0;

  for (int i = 0; i < 40 && drawn < 10; ++i) {
    uint8_t y = oam[static_cast<long>(i) * 4] - 16;
    uint8_t x = oam[(i * 4) + 1] - 8;
    uint8_t tile = oam[(i * 4) + 2];
    uint8_t attr = oam[(i * 4) + 3];

    if (ly < y || ly >= y + sprite_height) {
      continue;
    }

    drawn++;

    uint8_t line = ly - y;
    if ((attr & (1 << 6)) != 0) {
      line = sprite_height - 1 - line;
    }

    if (sprite_height == 16) {
      tile &= 0xFE;
    }

    uint16_t tile_addr = 0x8000 + (tile * 16) + (line * 2);

    uint8_t low = vram[tile_addr - 0x8000];
    uint8_t high = vram[tile_addr - 0x8000 + 1];

    for (int px = 0; px < 8; ++px) {
      int screen_x = x + px;
      if (screen_x < 0 || screen_x >= 160) {
        continue;
      }

      uint8_t bit = ((attr & (1 << 5)) != 0) ? px : (7 - px);

      uint8_t color_id = ((high >> bit) & 1) << 1 | ((low >> bit) & 1);

      if (color_id == 0) {
        continue;
      }

      uint8_t palette = ((attr & (1 << 4)) != 0) ? obp1 : obp0;
      uint8_t color = (palette >> (color_id * 2)) & 0x03;

      if (((attr & (1 << 7)) != 0) && framebuffer[(ly * 160) + screen_x] != 0) {
        continue;
      }

      framebuffer[(ly * 160) + screen_x] = color;
    }
  }
}
