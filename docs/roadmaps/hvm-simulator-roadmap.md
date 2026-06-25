# HVM Simulator Roadmap

This document is the phased implementation roadmap for the HVM System Simulator
(`hvm-sim`). It describes what has been built, what remains, and the delivery
order for the remaining subsystems.

---

## Implementation Status Overview

 | Component | Status | Lines | Notes |
| :--- | :---: | ---: | :--- |
| **hvm_sim_core** | ✅ Substantially complete | ~1375 | CPU state, decoder, 90+ instructions, traps, vectors, timer IRQ |
| **hvm_sim_mem** | ✅ Substantially complete | ~430 | Multi-region RAM/ROM/MMIO, bounds checking, callbacks, header |
| **hvm_sim_dev** | ❌ Stub | 3 | Nothing implemented |
| **hvm_sim_board** | ❌ Stub | 3 | Nothing implemented |
| **hvm_sim_block** | ❌ Stub | 3 | Nothing implemented |
| **hvm_sim_fw** | ❌ Stub | 3 | Nothing implemented |
| **hvm_sim_monitor** | ❌ Stub | 3 | Nothing implemented |
| **hvm_sim_host** | ❌ Stub | 3 | Nothing implemented |
| **hvm_sim_jit** | ❌ Stub | 3 | Nothing implemented |
| **hvmsys** (CLI) | ⚠️ Args only | 81 | Parses options, prints config, exits |
| **hvmimg** (CLI) | ⚠️ Args only | 76 | Prints "Would ...", exits |
| **hvmdisk** (CLI) | ⚠️ Args only | 25 | Prints "Would ...", exits |
| **hvm-sim-tests** | ✅ CPU + hvmc | 1726 | 120 CPU tests, 31 hvmc tests, 1 integration |

Build system, vcpkg, presets, formatting, linting, opcode generation, version
header, and all CMake targets are complete.

The **hvmc** C compiler / assembler toolchain (not part of the simulator proper)
is also substantially complete at ~2350 lines — see `docs/roadmaps/hvmc-roadmap.md`.

---

## Phase 0: Build System and Scaffold — ✅ COMPLETE

Phase 0 established the repository skeleton. All deliverables are in place:

- [x] Top-level `CMakeLists.txt` with C++17, options, sanitizers, warnings
- [x] `CMakePresets.json` for macOS (clang), Linux (clang), Windows (clang-cl)
- [x] `vcpkg.json` with all dependencies
- [x] `.clang-format` (Google style) and `.clang-tidy` configuration
- [x] Version header generation (`version.hpp.in`)
- [x] Opcode table generation from `docs/hvm_instruction_set.csv`
- [x] Nine library stub targets (core, mem, dev, board, block, fw, monitor, host, jit)
- [x] Three CLI tool stubs (hvmsys, hvmimg, hvmdisk)
- [x] Unit test executable scaffold
- [x] Empty tracked directories for future work

---

## Phase 1: CPU Interpreter — ✅ SUBSTANTIALLY COMPLETE

### Delivered

- **CPU state**: 32 GPRs (r0 hardwired zero), 16 vector registers (4×64-bit lanes),
  PC, CSRs (sstatus, stvec, sepc, scause, stval, satp, stime, stimecmp),
  privilege levels (User/Supervisor), load-reserve reservation, loop counters
- **Instruction decoder**: base32 (4-byte) and escape32 (8-byte prefix + ULEB128),
  formats R, I, B, J
- **Executor** (~90 instruction cases):
  - Data: NOP, MOV, MOVZ, LUI, ADDI
  - Integer: ADD, SUB, MUL, DIV, DIVU, REM
  - Shifts: SHL, SHR, SAR
  - Bitwise: AND, OR, XOR, NOT, bitwise-rotate
  - Floating-point: FADD, FSUB, FMUL, FDIV, FCMPEQ/FCMPLT/FCMPLE
  - Comparisons: CMPEQ, CMPNE, CMPLT, CMPLE
  - Branches: BEQ, BNE, BLT, BLE
  - Jumps: JMP, JAL, JALR, RET, CALL, TAILCALL
  - Memory: LD.B/BU/H/HU/W/WU/D/P, ST.B/H/W/D/P, LDA
  - Stack: PUSH, POP, ENTER, LEAVE, ADJSP, FRAME
  - System: SYSCALL, BREAK, ECALL, TRAPRET, CSRRW
  - Atomics: LR.D, SC.D
  - Cache: SFENCE.VMA (flush)
  - Hardware loops: LOOP.SET, LOOP.DECBR
  - Hints: PREFETCH.R/W/NTA, MEMZERO.HINT, BR.HINT, DOORBELL
  - Green compute: RETAIN, RELEASE, ICACHE.RNG
  - Special: ALLOC.BUMP, RDPROF, CHK.B, LD.D.NZ
  - Vector: VSETVL, VLD/VST (unit/stride/indexed), VADD/VSUB/VMUL/VDIV,
    VFMACC, VCOMP, VMERGE, VFIRST, VREDADD/VREDMIN/VREDMAX,
    VSLL/VSRL, VAND/VOR/VXOR
- **Exception model**: InvalidInstruction, MemoryAccess, DivisionByZero,
  PrivilegeViolation, Breakpoint, SystemCall
- **Timer interrupt**: stime/stimecmp comparison each step
- **Trap mechanism**: writes sepc/scause/stval, redirects to stvec
- **Unit tests**: 120 test cases covering all instruction categories

### Remaining Work

| Item | Priority | Notes |
| :--- | :---: | :--- |
| Page table walk / address translation | Medium | satp is stored but MMU is bypassed |
| Physical memory protection | Low | PMP not implemented |
| Multi-hart / SMP | Medium | Single hart only |
| Debug/trace mode | Low | External monitor interface |

---

## Phase 2: Memory, MMU, and Devices — 🔶 PARTIALLY COMPLETE

### Delivered (Memory)

- [x] `MemoryAccess` abstract interface (read/write byte/half/word/dword)
- [x] `FlatMemory` — contiguous vector-backed memory with bounds checks
- [x] `MemorySystem` — multi-region model (RAM, ROM, MMIO, Reserved)
- [x] `add_ram()`, `add_rom()`, `add_mmio()`, `add_region()`
- [x] MMIO read/write callbacks per base address
- [x] Read-only region protection
- [x] Bulk `read_bytes()` / `write_bytes()`
- [x] `clear()` and `get_total_size()`

### Delivered (Tests)

- [ ] No memory subsystem tests

### Remaining Work

| Item | Priority | Notes |
| :--- | :---: | :--- |
| **UART (16550-compatible)** | **High** | Needed for console output in kernel development |
| **System timer device** | **High** | Platform timer (beyond stimecmp) |
| **HPIC (interrupt controller)** | **High** | External interrupt routing |
| **Boot ROM** | **High** | Reset vector and firmware load |
| Virtual memory (HVM-39 page walker) | Medium | Required for MMU tests |
| TLB | Medium | Caching for page translations |
| Interrupt wiring from devices to CPU | Medium | IRQ delivery path |
| Device-specific unit tests | Medium | UART, timer, HPIC tests |
| Memory subsystem tests | Medium | FlatMemory, MemorySystem tests |

---

## Phase 3: Block Layer and Storage Tools

| Item | Priority | Notes |
| :--- | :---: | :--- |
| `BlockDevice` abstract interface | High | Required by hvmsys and hvmimg |
| Raw disk image support | High | Simple flat file |
| Sparse file images | Medium | Holes for unallocated blocks |
| `hvmimg` implementation | Medium | create, info, check, convert, snapshot, overlay |
| HSD / QCOW2 / VHDX format support | Low | Interoperability with other tools |
| Block cache / I/O scheduling | Low | Performance |

---

## Phase 4: Board Profiles and Device Tree

| Item | Priority | Notes |
| :--- | :---: | :--- |
| Board profile: hvm-mobile | High | Low-end phone/tablet |
| Board profile: hvm-desktop | High | Standard workstation |
| Board profile: hvm-server | Medium | Server / cloud |
| Board profile: hvm-robot | Low | Robotics target |
| Device tree generation (FDT) | High | Required for kernel boot |
| Memory map definitions per board | High | Per-board layout |
| CPU topology / hart count | Medium | SMP topology |

---

## Phase 5: Firmware and Boot Flow

| Item | Priority | Notes |
| :--- | :---: | :--- |
| Boot ROM / reset vector | High | Entry point for kernel boot |
| ELF loading for kernel images | High | Parse ELF64-HVM, place segments |
| Raw binary loading | High | Alternative to ELF |
| SFI (Standard Firmware Interface) ABI | Medium | Firmware service calls |
| Device tree passing to kernel | High | DTB pointer in boot registers |
| Boot handoff convention | High | Register state per ABI |
| U-Boot / coreboot integration paths | Low | External bootloader support |

---

## Phase 6: Display, Input, and Networking

| Item | Priority | Notes |
| :--- | :---: | :--- |
| `hvm_sim_host` — host interface | Medium | Wiring to platform backends |
| SDL3 framebuffer | Medium | Display output |
| SDL3 input (keyboard/mouse) | Medium | Console interaction |
| Debug UI (ImGui) | Low | In-simulator debug panel |
| USB host controller stub | Low | USB device passthrough |
| User-mode networking (tap/slirp) | Low | Network connectivity |

---

## Phase 7: hvmsys — Simulator Frontend Integration

The `hvmsys` CLI currently parses arguments and prints the configuration.
This phase wires it to the actual simulator libraries.

| Item | Priority | Notes |
| :--- | :---: | :--- |
| Machine configuration from CLI flags | High | Construct board profile |
| CPU + memory initialization | High | Create HvmCpuState, MemorySystem |
| Firmware / kernel loading | High | ELF loading into memory |
| Simulation loop (step CPU) | High | Run until halt or breakpoint |
| Monitor / debug interface | Medium | GDB stub or JSON monitor |
| JIT enablement | Low | Toggle LLVM JIT path |
| Display / input integration | Low | SDL3 event loop |

---

## Phase 8: hvmdisk Implementation

| Item | Priority | Notes |
| :--- | :---: | :--- |
| Filesystem image creation | Medium | ext4, FAT32, exFAT |
| Partition table (MBR/GPT) | Medium | Disk layout |
| Integration with block layer | Medium | Image creation via hvm_sim_block |

---

## Phase 9: LLVM ORC JIT Backend

| Item | Priority | Notes |
| :--- | :---: | :--- |
| LLVM dependency integration | Low | Conditional CMake support (exists) |
| Basic block compilation | Low | HVM opcodes → LLVM IR → native |
| Code cache management | Low | Block invalidation |
| Differential testing (interpreter vs JIT) | Low | Correctness validation |

---

## Phase 10: Robotics and Platform Polish

| Item | Priority | Notes |
| :--- | :---: | :--- |
| CAN-FD support | Low | Robotics peripheral |
| PWM / ADC / QEI | Low | Sensor and motor control |
| Fault injection scripting | Low | Testing hardened systems |
| VM snapshot / restore | Low | Save/load simulator state |
| Fuzz testing | Low | Random instruction sequences |

---

## Dependency Graph

```
hvmsys ───────────────────────────────────────────┐
  ├── hvm_sim_core  ──── hvm_sim_mem               │
  ├── hvm_sim_board  ──┬── hvm_sim_dev              │
  │                     └── hvm_sim_block           │
  ├── hvm_sim_fw  ──────── hvm_sim_core             │
  ├── hvm_sim_monitor ──── hvm_sim_core             │
  ├── hvm_sim_host ─────── hvm_sim_dev              │
  └── hvm_sim_jit ──────── hvm_sim_core ─── hvm_sim_mem
```

---

## Recommendation: Immediate Next Action

Wire `hvmsys` to the existing `hvm_sim_core` and `hvm_sim_mem` libraries. The CPU
and memory subsystems are functional — `hvmsys` should:

1. Parse `--kernel` ELF and load it into the memory system
2. Create a `HvmCpuState` configured for the requested machine
3. Enter a step loop until the CPU halts
4. Route MMIO writes (especially UART at 0x04000000) to stdout

This gives a working "boot and run" simulator with the current codebase, even
without device models for everything else.
