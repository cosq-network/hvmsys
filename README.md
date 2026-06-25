# HVM Simulator Workspace

This repository contains the HVM system simulator, supporting tools, tests, and the
design/specification documents that define the architecture being modeled.

The codebase is organized around a CMake-based simulator under `src/` and a
documentation set under `docs/`. The simulator is currently split into modular
subsystems for core execution, memory, devices, boards, firmware, host
integration, monitoring, and optional JIT support.

## What This Project Is

HVM stands for **Hoo Virtual Machine**. The project provides:

- a 64-bit HVM CPU and system simulator
- an HVM C compiler (`hvmc`) and assembler toolchain
- a documented instruction set and register model
- host-side tooling for system, image, disk, and compiler workflows
- unit tests for the simulator core and compiler
- reference specifications for CPU, SoC, board, firmware, ABI, and system behavior

The architecture and execution model are documented in:

- [HVM specification](docs/hvm-spec.md)
- [System design book](docs/system/README.md)
- [Instruction set CSV](docs/hvm_instruction_set.csv)
- [Register set CSV](docs/hvm_register_set.csv)

## Repository Layout

- `docs/` - canonical architecture and platform documentation
- `src/` - simulator source tree, build system, tools, tests, and generated files
- `CMakeLists.txt` - top-level simulator build
- `CMakePresets.json` - common configure/build/test presets
- `vcpkg.json` - vcpkg dependency manifest
- `src/include/` - public simulator headers
- `src/{hvm_sim_core,hvm_sim_mem,hvm_sim_dev,hvm_sim_board,hvm_sim_block,hvm_sim_fw,hvm_sim_monitor,hvm_sim_host,hvm_sim_jit}/` - subsystem implementations
- `src/tools/` - command-line utilities (hvmsys, hvmimg, hvmdisk, hvmc)
- `src/tests/` - test targets
- `src/cmake/` - local CMake helpers, including opcode-table generation
- `src/generated/` - generated headers in some local workflows
- `docs/roadmaps/` - implementation roadmaps for the simulator and toolchain

## Build Requirements

The simulator uses:

- CMake 3.25 or newer
- a C++17 compiler
- Ninja for the provided presets
- vcpkg-managed dependencies

Required packages from `vcpkg.json`:

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

## Build Targets

The simulator is split into small targets so each subsystem can evolve
independently.

### Core Libraries

| Target | Purpose |
| :--- | :--- |
| `hvm_sim_core` | Core CPU simulation support. This is where execution, decoding, register handling, and common runtime utilities converge. |
| `hvm_sim_mem` | Memory subsystem scaffolding for RAM, ROM, MMIO, address translation, and low-level access patterns. |
| `hvm_sim_dev` | Device-layer plumbing for timers, interrupt controllers, UARTs, and other platform devices. |
| `hvm_sim_board` | Board-profile composition and machine wiring for the supported HVM platform variants. |
| `hvm_sim_block` | Block-device and image-format support. The target already links Zlib for compression-related work. |
| `hvm_sim_fw` | Firmware and boot-chain support, including boot ROM and firmware-loading paths. |
| `hvm_sim_monitor` | Monitor and debug control surface. This target links `nlohmann_json` for structured control/response handling. |
| `hvm_sim_host` | Host-side integration layer for display, input, and other platform-facing services. |
| `hvm_sim_jit` | Optional LLVM-backed JIT integration for future block compilation and runtime acceleration. |

### CLI Tools

| Target | Purpose |
| :--- | :--- |
| `hvmsys` | System simulator front end. It accepts a machine profile, memory size, drives, firmware/kernel inputs, display selection, JIT/debug toggles, and monitor settings, then prints the assembled configuration. |
| `hvmimg` | Disk image manager. It exposes `create`, `info`, `check`, `convert`, `snapshot`, and `overlay` commands and currently reports the requested action. |
| `hvmdisk` | Disk image formatter. It accepts a size, filesystem type, and output path, then reports the intended disk creation task. |
| `hvmc` | HVM C compiler and assembler toolchain. Compiles C source (`.c`) and assembles HVM assembly (`.s`/`.asm`) into ELF64-HVM object files or executables. Supports `-S` (assembly output), `-c` (object only), `-E` (preprocess only), and `-I` (include paths). Uses the `hvmc_lib` static library for encoder, assembler, parser, codegen, and ELF generation. |

### Test Targets

| Target | Purpose |
| :--- | :--- |
| `hvm-sim-tests` | GoogleTest-based unit test binary covering CPU execution (60+ tests for all instruction categories), hvmc encoder/assembler/ELF writer, and integration tests. |

## Configure, Build, Test

The presets in `CMakePresets.json` cover common host/toolchain combinations.

### macOS

```bash
cmake --preset macos-clang-debug
cmake --build --preset macos-clang-debug
ctest --preset macos-clang-debug
```

### Linux

```bash
cmake --preset linux-clang-debug
cmake --build --preset linux-clang-debug
ctest --preset linux-clang-debug
```

### Windows with clang-cl

```bash
cmake --preset windows-clangcl-debug
cmake --build --preset windows-clangcl-debug
ctest --preset windows-clangcl-debug
```

### JIT-enabled build

```bash
cmake --preset macos-clang-jit
cmake --build --preset macos-clang-jit
```

### Build specific targets

```bash
cmake --build build --target hvmc              # HVM compiler only
cmake --build build --target hvmc_lib          # HVM compiler static library
cmake --build build --target hvm-sim-tests     # Tests only
```

## Generated Files

The build generates derived headers under the CMake build directory, including:

- `generated/hvm-sim/version.hpp`
- `generated/hvm_opcode_table.hpp`

Opcode-table generation is driven from `docs/hvm_instruction_set.csv` through
`src/cmake/generate_opcode_table.py`.

## Notes For Contributors

- Build artifacts should stay out of version control.
- If you add new generated outputs, update `.gitignore` and the relevant CMake
  logic together.
- Keep the documentation in `docs/` aligned with the simulator behavior and
  instruction-set changes.

## License

This project is licensed under GPL-2.0-only.
