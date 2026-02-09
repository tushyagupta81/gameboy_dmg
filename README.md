# GameBoy DMG Emulator

## Parts

1. CPU - 8-bit 8080-like Sharp CPU (speculated to be a SM83 core)
1. Master Clock - 4.194304 MHz
1. System Clock 1/4 frequency of Master Clock
1. Work RAM - 8KiB
1. Video RAM - 8KiB
1. Resolution - 160 x 144
1. OBJ(Sprites) - 8x8 or 8x16 - max 40 per screen and 10 per line
1. Palettes - BG: 1x4, OBJ: 2x3
1. Colors - 4 shades of green
1. Horizontal Sync - 9.198KHz
1. Vertical Sync - 59.73 Hz
1. Sound - 4 channel with stereo output
1. Power - DC 6V, 0.7W

[Source](https://gbdev.io/pandocs/Specifications.html)

## Memory map

1. `0000 - 3FFF`: 16KiB ROM Bank 00
1. `4000 - 7FFF`: 16KiB ROM Bank 01-NN
1. `8000 - 9FFF`: 8KiB VRAM
1. `A000 - BFFF`: 8KiB External RAM
1. `C000 - CFFF`: 4KiB Work RAM Bank 0
1. `D000 - DFFF`: 4KiB Work RAM Bank 1
1. `E000 - FDFF`: Echo RAM(copy of C000 - DDFF)
1. `FE00 - FE9F`: Object attribute memory(OAM)
1. `FEA0 - FEFF`: Not usable
1. `FF00 - FF7F`: IO Registers
1. `FF80 - FFFE`: High RAM
1. `FFFF - FFFF`: Interrupt enable register

[Source](https://gbdev.io/pandocs/Memory_Map.html)

### IO Ranges

1. `FF00`: Joypad input
1. `FF01 - FF02`: Serial transfer
1. `FF04 - FF07`: Timer and drivers
1. `FF0F`: Interrupts
1. `FF10 - FF26`: Audio
1. `FF30 - FF3F`: Wave Patterns
1. `FF40 - FF4B`: LCD Control, Status, Position, Scrolling, and Palettes
1. `FF46`: OAM DMA transfer
1. `FF50`: Boot ROM mapping control

### VRAM

- Bank first contains 384 tiles, of 16 bytes each -> 384\*16=6KiB
  - [Tiles are grouped in 3 "blocks" each, giving us 128 tiles](https://gbdev.io/pandocs/Tile_Data.html#vram-tile-data)
    - Basically 3 sections of 128 tiles each
- 2 Maps 32x32B (1KiB) each
  - `Bank 0`: Tile maps

### Jump Vectors in Bank 00

- RST instructions: 0000, 0008, 0010, 0018, 0020, 0028, 0030, 0038
- Interrupts: 0040, 0048, 0050, 0058, 0060

### Cartridge Headers(Important)

Lies at from `0100 - 014F`: Contains info about the program

- Entry point
- Checksum
- Info on the MBC Chip
- ROM, RAM size

### External memory and hardware

- The areas from `0000 - 7FFF` and `A000 - BFFF` address external hardware on the cartridge
- Can be the ROM, SRAM or a MBC(Memory Bank Controller)

### Echo RAM

> The range E000-FDFF is mapped to WRAM, but only the lower 13 bits of the address lines are connected, with the upper bits on the upper bank set internally in the memory controller by a bank swap register. This causes the address to effectively wrap around. All reads and writes to this range have the same effect as reads and writes to C000-DDFF.

> Nintendo prohibits developers from using this memory range. The behavior is confirmed on all official hardware. Some emulators (such as VisualBoyAdvance <1.8) don’t emulate Echo RAM. In some flash cartridges, echo RAM interferes with SRAM normally at A000-BFFF. Software can check if Echo RAM is properly emulated by writing to RAM (avoid values 00 and FF) and checking if said value is mirrored in Echo RAM and not cart SRAM.

### `FEA0 - FEFF`

- Writes ignored
- Reads during OAM block -> OAM corruption
- Else -> return `00`

[Source](https://gbdev.io/pandocs/Memory_Map.html)
