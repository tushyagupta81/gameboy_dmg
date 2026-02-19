#include "cpu.hpp"
#include <iostream>
#include <string>

auto main(int argc, char **argv) -> int {
  if (argc < 2) {
    std::cerr << "Correct usage:\n\t./Gameboy <rom_path>\n";
    return 1; // exit with error
  }

  std::string rom_path = argv[1];
  CPU cpu(rom_path); // initialize CPU with ROM

  // cpu.dump_opcode_table();

  // Optionally: run a simple CPU loop here
  while (true) {
    int cycles = cpu.step();
    cpu.timer_tick(cycles);
  }

  return 0;
}
