# HVM Simulator Workspace

This repository contains the HVM system simulator, supporting tools, tests, and the
design/specification documents that define the architecture being modeled.

The codebase is organized around a CMake-based simulator under `sim/` and a
documentation set under `docs/`. The simulator is currently split into modular
subsystems for core execution, memory, devices, boards, firmware, host
integration, monitoring, and optional JIT support.

## What This Project Is

HVM stands for **Hoo Virtual Machine**. The project provides:

- a 64-bit HVM CPU and system simulator
- a documented instruction set and register model
- host-side tooling for system, image, and disk workflows
- unit tests for the simulator core
- reference specifications for CPU, SoC, board, firmware, ABI, and system behavior

The architecture and execution model are documented in:

- [HVM specification](docs/hvm-spec.md)
- [System design book](docs/system/README.md)
- [Instruction set CSV](docs/hvm_instruction_set.csv)
- [Register set CSV](docs/hvm_register_set.csv)

## Repository Layout

- `docs/` - canonical architecture and platform documentation
- `sim/` - simulator source tree, build system, tools, tests, and generated files
- `plan.md` and `plans.md` - project planning notes

Inside `sim/`:

- `CMakeLists.txt` - top-level simulator build
- `CMakePresets.json` - common configure/build/test presets
- `include/` - public simulator headers
- `src/` - subsystem implementations
- `tools/` - command-line utilities
- `tests/` - test targets
- `cmake/` - local CMake helpers, including opcode-table generation
- `generated/` - generated headers in some local workflows

## Build Requirements

The simulator uses:

- CMake 3.25 or newer
- a C++17 compiler
- Ninja for the provided presets
- vcpkg-managed dependencies

Required packages from `sim/vcpkg.json`:

- `cli11`
- `fmt`
- `spdlog`
- `nlohmann-json`
- `zlib`
- `gtest`

Optional features:

- `jit` - LLVM JIT backend
- `ui` - SDL3 and ImGui UI support
- `crypto` - OpenSSL crypto support

## Configure, Build, Test

The presets in `sim/CMakePresets.json` cover common host/toolchain combinations.

### macOS

```bash
cd sim
cmake --preset macos-clang-debug
cmake --build --preset macos-clang-debug
ctest --preset macos-clang-debug
```

### Linux

```bash
cd sim
cmake --preset linux-clang-debug
cmake --build --preset linux-clang-debug
ctest --preset linux-clang-debug
```

### Windows with clang-cl

```bash
cd sim
cmake --preset windows-clangcl-debug
cmake --build --preset windows-clangcl-debug
ctest --preset windows-clangcl-debug
```

### JIT-enabled build

```bash
cd sim
cmake --preset macos-clang-jit
cmake --build --preset macos-clang-jit
```

## Available Tools

The build produces three command-line tools:

- `hvmsys` - system simulator front end
- `hvmimg` - image utility
- `hvmdisk` - disk utility

The test target is:

- `hvm-sim-tests`

## Generated Files

The build generates derived headers under the CMake build directory, including:

- `generated/hvm-sim/version.hpp`
- `generated/hvm_opcode_table.hpp`

Opcode-table generation is driven from `docs/hvm_instruction_set.csv` through
`sim/cmake/generate_opcode_table.py`.

## Notes For Contributors

- Build artifacts should stay out of version control.
- If you add new generated outputs, update `.gitignore` and the relevant CMake
  logic together.
- Keep the documentation in `docs/` aligned with the simulator behavior and
  instruction-set changes.

## License

This project is licensed under GPL-2.0-only.
