cmake_minimum_required(VERSION 4.2)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

project(Gameboy)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")

add_executable(Gameboy)

target_sources(Gameboy
  PRIVATE
   src/main.cpp
   src/core/cpu.cpp
   src/core/bus.cpp
)

target_compile_definitions(Gameboy PRIVATE TRACE)

target_include_directories(Gameboy
   PRIVATE
   include)
