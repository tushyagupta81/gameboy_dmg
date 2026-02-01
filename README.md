# C++ Template using CMake
- docs - any documentation
- include - any header files you create
- libs - any external libraries
- src - your source code
- tests - any tests u write

## How to use
1. Build the project Makefile
```sh
cmake -B build
```
2. Build the actual project
```sh
cmake --build build
```
3. Run the project
```sh
./build/ProjectName
```

## How to make changes
1. Change the project name in CMakeLists.txt
2. For developer support symlink the build/compile_commands.json to project root
    - Windows ->
    ```sh
    (CMD)
    mklink compile_commands.json build\compile_commands.json
    (PowerShell)
    New-Item -ItemType SymbolicLink -Path compile_commands.json -Target build\compile_commands.json
    ```
    - Linux/Macos -> 
    ```sh
    ln -s /path/to/project/build/compile_commands.json /path/to/project
    ```
