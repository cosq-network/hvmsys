# HVM Simulator Roadmap and Phase 0 Implementation Plan

This document merges the previous `plan.md` and `plans.md` notes into a single
working roadmap for the HVM System Simulator project. It combines the near-term
Phase 0 implementation checklist with the broader multi-phase delivery schedule
for the simulator, tools, board profiles, firmware, JIT backend, and platform
polish work.

## Purpose

The HVM simulator is the reference implementation for the Hoo Virtual Machine
hardware/software model. The project is intended to provide:

- a modular C++ simulator with a clean build system
- a generated opcode table from the canonical instruction-set CSV
- CLI tools for system, image, and disk workflows
- unit test coverage for core simulator behavior
- a foundation for later CPU, MMU, device, board, firmware, display, and JIT
  work

This roadmap is written as an implementation-oriented document. It is not a
specification of the architecture itself; the canonical architecture references
remain the documents under `docs/`, especially `docs/hvm-spec.md` and
`docs/system/README.md`.

## Project Phases

| Phase | Time | Deliverable |
| :--- | :--- | :--- |
| Phase 0 | Week 1-2 | Build system, CLI stubs, CSV-to-opcode generator, and baseline CI-ready skeleton |
| Phase 1 | Week 3-6 | HvmCpuState, instruction decode/execute, precise exceptions, unit tests |
| Phase 2 | Week 7-9 | Memory, MMU, device model, page-table walker, TLB, interrupts, UART, timer, boot ROM |
| Phase 3 | Week 10-12 | Block layer, storage formats, disk image tooling |
| Phase 4 | Week 13-15 | Board profiles and device-tree generation |
| Phase 5 | Week 16-18 | Firmware, boot flow, and ABI integration |
| Phase 6 | Week 19-20 | Display, input, and networking support |
| Phase 7 | Week 21 | `hvmdisk` implementation |
| Phase 8 | Week 22-25 | LLVM ORC JIT and differential validation |
| Phase 9 | Week 26-27 | Robotics peripherals, fault injection, snapshots, and fuzzing |

## Phase 0 Goals

Phase 0 establishes the simulator skeleton. The target outcome is a repository
that configures, builds, and tests cleanly, while leaving the runtime logic for
later phases.

### Primary Deliverables

- top-level CMake build and common presets
- vcpkg dependency manifest
- formatting and static-analysis configuration
- version-header generation
- opcode-table generation from the instruction CSV
- library stub targets for each simulator subsystem
- command-line tool stubs
- unit test executable scaffold
- empty tracked directories for future implementation areas

### Required Files

#### Build System

1. `sim/CMakeLists.txt`
- C++17, position-independent code enabled
- options for JIT, UI, tools, tests, sanitizers, crypto, and opcode generation
- output directories for binaries and libraries
- version header configured from `version.hpp.in`
- sanitizer flags enabled conditionally
- warning flags for Clang/GCC and MSVC
- package discovery for `fmt`, `spdlog`, `CLI11`, `nlohmann_json`, `zlib`, and `gtest`
- conditional discovery for LLVM, SDL3, and OpenSSL
- subdirectories for core, memory, devices, board, block, firmware, monitor, host,
  optional JIT, tools, and tests
- inclusion of the opcode-table generator helper

2. `sim/vcpkg.json`
- package name `hvm-sim`
- version `0.1.0`
- license metadata
- core dependencies and optional features for JIT, UI, and crypto

3. `sim/CMakePresets.json`
- version 6 with minimum CMake 3.25
- preset sets for macOS, Linux, and Windows clang/clang-cl toolchains
- debug, release, and JIT variants where appropriate
- matching build and test presets

4. `sim/.clang-format`
- Google-style formatting
- column limit of 100
- indent width of 4
- C++17 language mode

5. `sim/.clang-tidy`
- clang diagnostics, analyzer, bugprone, modernize, performance, and readability
  checks
- warnings treated as errors

#### Code Generation

6. `sim/include/hvm-sim/version.hpp.in`
- CMake template for version metadata

7. `sim/cmake/GenerateOpcodeTable.cmake`
- locates Python 3
- reads `docs/hvm_instruction_set.csv`
- generates `hvm_opcode_table.hpp` in the build tree
- provides a helper function to attach generation dependencies and include paths

8. `sim/cmake/generate_opcode_table.py`
- parses the instruction-set CSV
- emits the instruction metadata table
- handles shared opcodes and function-selected sub-operations
- produces the generated opcode header used by simulator code and tests

#### Library Stubs

9. `sim/src/{core,mem,dev,board,block,fw,monitor,host,jit}/CMakeLists.txt`
- each subsystem builds as its own library target
- each target includes a stub source file
- library linking follows the intended dependency direction
- the block layer links `zlib::zlib`
- public include paths point at `sim/include`

10. `sim/src/{core,mem,dev,board,block,fw,monitor,host,jit}/stub.cpp`
- minimal compilation units with a placeholder include and TODO marker

#### Tools

11. `sim/tools/CMakeLists.txt`
- adds the three tool subdirectories

12. `sim/tools/hvmsys/main.cpp`
- system simulator CLI
- machine selection, memory, SMP, drives, display, BIOS, kernel, initrd, DTB,
  append string, JIT, debug, monitor, and version options
- prints a summary and exits successfully

13. `sim/tools/hvmimg/main.cpp`
- image utility CLI
- create, info, check, convert, snapshot, and overlay subcommands
- reports intended behavior and exits successfully

14. `sim/tools/hvmdisk/main.cpp`
- disk utility CLI
- size, filesystem, and output arguments
- reports intended behavior and exits successfully

#### Tests

15. `sim/tests/CMakeLists.txt`
- adds the unit-test subtree
- registers the test executable with CTest

16. `sim/tests/unit/CMakeLists.txt`
- builds `hvm-sim-tests`
- links GoogleTest and the subsystem libraries

17. `sim/tests/unit/main.cpp`
- standard GoogleTest entry point

#### Empty Markers

- `.gitkeep` in `sim/include/hvm-sim/{core,mem,dev,board,block,fw,monitor,host}/`
- `.gitkeep` in `sim/generated/`
- `.gitkeep` in `sim/tests/integration/`
- `.gitkeep` in `sim/tests/images/`

## Phase 0 Acceptance Criteria

Phase 0 is complete when all of the following are true:

- CMake configures successfully on the supported preset set
- the generated version header is produced in the build tree
- the opcode generator reads the CSV and emits `hvm_opcode_table.hpp`
- all subsystem libraries build from their stub targets
- all three CLI tools build
- the unit test executable builds and runs under CTest
- the repository layout is ready for later implementation phases

## Verification Plan

The initial verification sequence is:

```bash
# Check file presence
ls -la sim/CMakeLists.txt sim/vcpkg.json sim/CMakePresets.json
ls -la sim/tools/hvmsys/main.cpp sim/tools/hvmimg/main.cpp sim/tools/hvmdisk/main.cpp
ls -la sim/tests/unit/main.cpp
ls -la sim/src/*/CMakeLists.txt
ls -la sim/cmake/GenerateOpcodeTable.cmake sim/cmake/generate_opcode_table.py
ls -la sim/include/hvm-sim/version.hpp.in

# Run opcode generation
python3 sim/cmake/generate_opcode_table.py docs/hvm_instruction_set.csv

# Verify the generated header
grep -c "HvmMnemonic" sim/build/test/generated/hvm_opcode_table.hpp

# Configure and build
cmake -S sim -B sim/build/test -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake --build sim/build/test

# Run tests
cd sim/build/test && ctest
```

## Later Phases

### Phase 1: CPU Interpreter

Deliver `HvmCpuState`, full instruction decode and execution, precise exception
handling, and unit tests for the core ISA.

### Phase 2: Memory, MMU, and Devices

Deliver physical memory mapping, MMU support, HVM-39 page walking, TLB behavior,
local and platform interrupts, UART, timer, and boot-ROM support.

### Phase 3: Block Layer and Storage Tools

Deliver the block-device interface, raw and native storage formats, and a more
complete `hvmimg` feature set.

### Phase 4: Board Profiles

Deliver machine profiles for mobile, desktop, server, and robotics targets
plus device-tree generation.

### Phase 5: Firmware

Deliver the boot-ROM contract, firmware ABI, U-Boot path, coreboot/EDK II
integration where appropriate, and direct kernel boot support.

### Phase 6: Display, Input, and Networking

Deliver SDL3 framebuffer support, debug UI, USB input stubs, and user-mode
networking.

### Phase 7: `hvmdisk`

Deliver image creation with filesystem selection and output generation.

### Phase 8: LLVM ORC JIT

Deliver JIT block caching, lowering to LLVM IR, runtime helpers for MMIO and
privileged operations, cache invalidation, and differential testing between the
interpreter and JIT.

### Phase 9: Robotics and Polish

Deliver CAN-FD support, PWM, ADC, QEI, lockstep handling, safe-state behavior,
fault scripting, VM snapshot support, and fuzzing.

## Dependencies and Constraints

- The simulator code should remain consistent with the canonical HVM
  documentation.
- Generated files must stay in the build tree unless a specific source-tree
  artifact is intentionally committed.
- Library and tool stubs should preserve the eventual ownership boundaries of
  the simulator subsystems.
- New generated outputs should be accompanied by matching ignore rules and
  build-system updates.

## Notes

- Phase 0 is the scaffolding phase, not the runtime implementation phase.
- The instruction set CSV and register-set CSV are authoritative inputs to the
  simulator build.
- Future phases should refine this roadmap rather than replacing it with a new
  one-off checklist.
