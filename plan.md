implement Phase 0 of the HVM simulator

# HVM Simulator Phase 0 — Implementation Plan

## Instructions

Paste this into a new implementation-mode chat session to build the Phase 0 skeleton.

---

Implement Phase 0 of the HVM System Simulator project at `/Users/benoybose/Projects/hvmsim/sim/`

## Files to Create

### Build System (5 files)

**1. `sim/CMakeLists.txt`**
- C++17, PIC ON
- Options: HVM_SIM_ENABLE_JIT/OFF, ENABLE_UI/OFF, BUILD_TOOLS/ON, BUILD_TESTS/ON, ENABLE_ASAN/OFF, ENABLE_UBSAN/OFF, ENABLE_CRYPTO/OFF, GENERATE_OPCODES/ON
- Output dirs: ${CMAKE_BINARY_DIR}/bin, /lib
- Configure version header from version.hpp.in
- ASan/UBSan flags conditionally
- Warnings: -Wall -Wextra -Wpedantic -Wconversion -Wsign-conversion (Clang/GCC), /W4 (MSVC)
- Find packages: fmt, spdlog, CLI11, nlohmann_json, zlib (CONFIG, REQUIRED)
- Conditional: LLVM (JIT), SDL3 (UI), OpenSSL (CRYPTO)
- Subdirs: src/core, src/mem, src/dev, src/block, src/fw, src/monitor, src/host, src/board, optional src/jit, tools, tests
- Include cmake/GenerateOpcodeTable

**2. `sim/vcpkg.json`**
- name "hvm-sim", version "0.1.0", license "Apache-2.0"
- deps: cli11, fmt, spdlog, nlohmann-json, zlib, gtest
- features: jit (llvm), ui (sdl3 + imgui), crypto (openssl)

**3. `sim/CMakePresets.json`**
- Version 6, cmake min 3.25
- Presets: macos-clang-debug/release/jit, linux-clang-debug/release, windows-clangcl-debug/release
- Each with CMAKE_EXPORT_COMPILE_COMMANDS ON
- Matching build presets and test presets

**4. `sim/.clang-format`** — Google style, ColumnLimit 100, IndentWidth 4, C++17

**5. `sim/.clang-tidy`** — clang-diagnostic/analyzer/bugprone/modernize/performance/readability checks, warnings-as-errors

### Code Generation (2 files)

**6. `sim/include/hvm-sim/version.hpp.in`** — CMake @var@ template

**7. `sim/cmake/GenerateOpcodeTable.cmake`**
- Reads docs/hvm_instruction_set.csv
- Python3 find_package
- Custom command → generated/hvm_opcode_table.hpp
- Function generate_opcode_table(target) adds dep + include

**8. `sim/cmake/generate_opcode_table.py`**
- Parses CSV: Mnemonic, Opcode, Encoding, Format, Operands, Operation, Description, Func, Example
- Produces:
  - enum class HvmMnemonic (114 entries, e.g. VADD_VV for vector ops with func)
  - struct HvmInstInfo { mnemonic, opcode, encoding*, format*, operands*, func, mnemonic_str* }
  - constexpr array<HvmInstInfo, N> kHvmInstTable
  - constexpr auto kHvmInstCount
- Shared opcodes (0x10=ADD/SUB/MUL/DIV/DIVU/REM via func) get separate enum entries
- Vector sub-ops (0xE1 func 0-5, 0xE2 func 0-7, 0xE4 func 0-3 etc.) get separate entries
- Handle all 114 CSV rows

### Library Stubs (9 × CMakeLists.txt + 9 × stub.cpp)

**9-17. `sim/src/{core,mem,dev,board,block,fw,monitor,host,jit}/CMakeLists.txt`**
- Each creates library target hvm_sim_{name}
- Each has one stub.cpp source
- Link chain: core ← mem ← dev ← board; core ← block; core+block ← fw; core+json ← monitor; core ← host; core ← jit
- block links zlib::zlib
- Include dir: ${CMAKE_CURRENT_SOURCE_DIR}/../../include

**18. Each `stub.cpp`** — `#include <fmt/core.h>` with TODO comment

### Tools (3 executables)

**19. `sim/tools/CMakeLists.txt`** — add_subdirectory(hvmsys hvmimg hvmdisk)

**20. `sim/tools/hvmsys/main.cpp`**
- Target "hvmsys", links ALL hvm_sim_* libraries
- CLI11 args:
  --machine/-m REQUIRED (hvm-mobile|hvm-desktop|hvm-server|hvm-robot)
  --ram (default "4G"), --smp (default 1)
  --drive NAME=PATH (repeatable), --display (none|sdl), --bios, --kernel, --initrd, --dtb, --append
  --jit (flag), --debug (flag), --monitor (stdio|none), --version
- Print summary and exit 0

**21. `sim/tools/hvmimg/main.cpp`**
- Target "hvmimg", links hvm_sim_block + hvm_sim_core
- Subcommands: create (-f FORMAT -s SIZE PATH), info, check, convert (-f FORMAT IN OUT), snapshot {create|delete|list}, overlay create (-b BASE OVERLAY)
- Print what it WOULD do and exit 0

**22. `sim/tools/hvmdisk/main.cpp`**
- Target "hvmdisk", links hvm_sim_block + hvm_sim_core
- Args: --size (req), --filesystem ext4|fat32|exfat (req), --output (req)
- Print what it WOULD do and exit 0

### Tests (2 files)

**23. `sim/tests/CMakeLists.txt`**
- add_subdirectory(unit)
- add_test(hvm-sim-tests)

**24. `sim/tests/unit/CMakeLists.txt`**
- Target "hvm-sim-tests", sources main.cpp
- Link GTest::gtest, GTest::gtest_main, all hvm_sim_* libs

**25. `sim/tests/unit/main.cpp`**
- Standard gtest main with InitGoogleTest + RUN_ALL_TESTS

### Empty Markers
- `.gitkeep` in: include/hvm-sim/{core,mem,dev,board,block,fw,monitor,host}/, generated/, tests/integration/, tests/images/

## Verification

```bash
# 1. Check all files exist
ls -la sim/CMakeLists.txt sim/vcpkg.json sim/CMakePresets.json
ls -la sim/tools/hvmsys/main.cpp sim/tools/hvmimg/main.cpp sim/tools/hvmdisk/main.cpp
ls -la sim/tests/unit/main.cpp
ls -la sim/src/*/CMakeLists.txt
ls -la sim/cmake/GenerateOpcodeTable.cmake sim/cmake/generate_opcode_table.py
ls -la sim/include/hvm-sim/version.hpp.in

# 2. Run opcode generator
python3 sim/cmake/generate_opcode_table.py docs/hvm_instruction_set.csv

# 3. Verify generated header
grep -c "HvmMnemonic" generated/hvm_opcode_table.hpp

# 4. CMake configure + build
cmake -S sim -B sim/build/test -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build sim/build/test

# 5. Run tests
cd sim/build/test && ctest