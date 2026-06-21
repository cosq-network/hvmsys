# HVM Lightweight System Simulator Design

This document specifies a new lightweight HVM system simulator for the documented HVM CPU, SoC, and board profiles:

- `HVM-M1` mobile SoC and board
- `HVM-D1` desktop CPU and motherboard
- `HVM-S1` server CPU and motherboard
- `HVM-R1` robotics SoC and controller board

The simulator should feel operationally similar to QEMU system emulation: it boots firmware, instantiates a machine model, runs virtual CPUs, exposes memory-mapped devices, attaches disk images, routes interrupts, provides debug/trace tooling, and can be driven from command-line or scripts. It must not be a QEMU fork or QEMU system target. The implementation should be native C/C++, built with CMake, managed through vcpkg, compiled with Clang where possible, and use LLVM for optional dynamic translation.

---

## 1. Goals and Non-Goals

### 1.1 Goals

- Build a cross-platform simulator for Windows, Linux, and macOS.
- Support HVM machine profiles for mobile, desktop, server, and robotics devices.
- Simulate the HVM CPU ISA, HVM-39 MMU, HLIC local interrupts, HPIC platform interrupts, timers, firmware ROM, RAM, storage, UART, display, networking, and profile-specific peripherals.
- Provide an interpreter first, then an optional LLVM ORC JIT backend for hot translation.
- Support raw, QCOW, and QCOW2 disk image files.
- Prefer permissive or weak-permissive open-source dependencies suitable for commercial and open development.
- Keep QEMU compatibility at the interface level where useful, such as familiar CLI conventions and disk image semantics, without reusing GPL implementation code.
- Make the simulator deterministic enough for compiler, OS, firmware, board, and robotics-control validation.

### 1.2 Non-Goals

- Do not implement hardware virtualization through KVM, WHPX, HVF, or Hyper-V in the first version. HVM is a custom ISA, so CPU execution is emulation/JIT, not host virtualization.
- Do not use QEMU code internally.
- Do not depend on GPL-only libraries in core simulator binaries.
- Do not aim for cycle-accurate silicon simulation in the first release. The first target is functional system simulation with optional timing annotations.
- Do not emulate every possible industry peripheral. Use a compact set of HVM reference devices and virtio-like devices where practical.

---

## 2. Research Summary

### 2.1 QEMU Lessons to Reuse Conceptually

QEMU is a mature system emulator and virtualizer with broad architecture support, machine models, device models, block backends, and dynamic translation. For HVM, reuse the architectural lessons, not the code:

- Machine profiles describe board topology.
- CPUs and devices communicate through memory regions and I/O buses.
- Devices raise interrupts into platform interrupt controllers.
- Block backends abstract raw and copy-on-write disk formats.
- Debug stubs, monitor commands, traces, and snapshots are central to developer usability.

QEMU itself is GPL-family licensed and should not be used as source code for an HVM simulator that targets liberal licensing.

### 2.2 QCOW/QCOW2 Options

The simulator must support QCOW and QCOW2. There are three implementation choices:

| Option | License / Risk | Recommendation |
| :--- | :--- | :--- |
| Reuse QEMU block code | GPL-family | Reject for core simulator. Conflicts with liberal-license goal. |
| Use `libqcow` | LGPL-3.0-or-later / GPL-3.0, alpha status, read-only support | Do not use in the core. Acceptable only as an optional diagnostic import tool if licensing is explicitly approved. |
| Implement HVM-owned QCOW/QCOW2 backend from public format docs | Can be licensed with the HVM simulator | Recommended. Use clean-room implementation and compatibility tests against known images. |

The HVM simulator should implement its own QCOW/QCOW2 parser, allocator, metadata writer, and cluster cache. This avoids pulling GPL/LGPL storage code into the simulator while still supporting the documented formats.

### 2.2.1 Permissive-License Fallback Disk Formats

If QCOW/QCOW2 support is rejected for legal, maintenance, or schedule reasons, the simulator should use the following fallback policy:

1. **Primary baseline: raw sparse image**
   - Format: sector-for-sector raw disk image.
   - Library: no third-party library; implement in `hvm_block` using C++ file I/O and platform sparse-file calls.
   - License posture: fully owned by HVM simulator.
   - Strengths: trivial, portable, fastest path to boot, easiest to debug, compatible with many tools.
   - Weaknesses: no built-in metadata, no portable snapshot model, no backing files.

2. **Interoperable sparse format: VHDX**
   - Format: Microsoft Virtual Hard Disk v2 (`.vhdx`).
   - Specification posture: Microsoft publishes VHD/VHDX specifications under the Microsoft Open Specification Promise.
   - Library: implement an HVM-owned `VhdxBlockDevice` in `hvm_block`; do not depend on GPL/LGPL VHD libraries.
   - License posture: HVM implementation can be Apache-2.0/MIT/BSD because it is written from the public specification.
   - Strengths: cross-tool interoperability, dynamic allocation, 64 TB image size class, power-failure-resilience design, native Windows mount support.
   - Weaknesses: more complex than raw; snapshots/differencing require additional work; macOS/Linux support is less universal than raw.

3. **Native simulator format: HSD (HVM Sparse Disk)**
   - Format: new HVM-owned sparse disk container (`.hsd`).
   - Library: `hvm_block` owns `HsdBlockDevice`.
   - License posture: fully owned by HVM simulator.
   - Strengths: simple clean-room design, permissive implementation, predictable semantics, snapshots and backing files can be tailored to HVM tests.
   - Weaknesses: not interoperable until tooling exists; conversion tools are required.

Recommended fallback order:

| Priority | Format | Backend | Reason |
| :---: | :--- | :--- | :--- |
| 1 | raw sparse | `RawBlockDevice` | Minimal, fully permissive, required for bring-up |
| 2 | VHDX | `VhdxBlockDevice` | Best permissive interoperable sparse format |
| 3 | HSD | `HsdBlockDevice` | Best native replacement for QCOW2 snapshots/backing files |
| 4 | QCOW/QCOW2 | `QcowBlockDevice` / `Qcow2BlockDevice` | Keep only if clean-room implementation is approved |

Do not use mature GPL/LGPL implementations as linked libraries in the core simulator. If compatibility with existing QCOW2 images remains important, keep QCOW2 as an optional clean-room backend and make raw/VHDX/HSD the default formats.

### 2.3 LLVM ORC JIT Fit

LLVM ORC is a modular JIT API that supports LLVM IR compilation, object linking, eager/lazy compilation, concurrent compilation, and custom program representations. This is a good match for HVM dynamic translation:

- The interpreter remains the correctness oracle.
- The JIT translates HVM basic blocks to LLVM IR.
- ORC compiles hot translation units into host machine code.
- The simulator invalidates translated code on `ICACHE.RNG`, writes to executable guest pages, MMU map changes, or privilege boundary changes.

### 2.4 vcpkg and Cross-Platform Build Fit

vcpkg supports CMake integration and manifest-based dependency management across Windows, Linux, and macOS. The simulator should use `vcpkg.json` for third-party dependencies, CMake presets for host profiles, and avoid platform-specific build scripts except for CI wrappers.

---

## 3. Recommended License Policy

The simulator should use a permissive license such as Apache-2.0, MIT, or BSD-3-Clause. Because LLVM is Apache-2.0 WITH LLVM-exception, Apache-2.0 is a natural fit for the project, but MIT or BSD-3-Clause are also workable.

### 3.1 Core Dependency Rules

- Allowed in core: MIT, BSD-2-Clause, BSD-3-Clause, Apache-2.0, Apache-2.0 WITH LLVM-exception, zlib, BSL-1.0, ISC, 0BSD, public domain.
- Allowed with review: LGPL libraries loaded dynamically behind optional plugin boundaries.
- Avoid in core: GPL, AGPL, LGPL static linking, license-unclear code, abandoned code without active security posture.
- Do not copy QEMU implementation code. Format interoperability is acceptable; source reuse is not.

### 3.2 Candidate Libraries

| Area | Library | License | Use |
| :--- | :--- | :--- | :--- |
| Compiler/JIT | LLVM, Clang, LLD | Apache-2.0 WITH LLVM-exception | HVM DBT/JIT, disassembly support, test compilation, tooling |
| Build/deps | CMake, vcpkg | BSD-style / MIT tooling | Cross-platform build and dependency management |
| CLI | CLI11 | BSD-3-Clause | Command-line parser |
| Logging | spdlog + fmt | MIT | Structured logs and formatting |
| Config | toml++ or yaml-cpp | MIT | Machine profile and test config parsing |
| JSON | nlohmann/json | MIT | Trace metadata, monitor protocol, image metadata |
| Compression | zlib or miniz | zlib/MIT | QCOW/QCOW2 DEFLATE clusters |
| Compression | zstd | BSD/GPL dual; use BSD terms | Optional QCOW2 zstd-compressed clusters |
| Crypto | OpenSSL 3.x or mbedTLS | Apache-2.0 / Apache-2.0 | Optional AES-CBC compatibility, hashes, signatures |
| Event loop | libuv or Boost.Asio | MIT / BSL-1.0 | Timers, async I/O, monitor sockets |
| UI | SDL3 + Dear ImGui | zlib / MIT | Optional display window, input, debug UI |
| Tests | Catch2 or GoogleTest | BSL-1.0 / BSD-3-Clause | Unit and integration tests |
| Memory allocator | mimalloc | MIT | Optional high-performance allocation |

The first version can be smaller: LLVM, CLI11, fmt/spdlog, nlohmann/json, zlib/miniz, Catch2, and optional SDL3/ImGui are enough.

---

## 4. High-Level Architecture

```
+------------------------------------------------------------------+
| hvm-sim CLI / monitor / test harness                             |
+------------------------------------------------------------------+
       |
       v
+------------------------------------------------------------------+
| Machine Model                                                     |
|  HVM-M1 mobile | HVM-D1 desktop | HVM-S1 server | HVM-R1 robot    |
+------------------------------------------------------------------+
       |
       +----------------+----------------+----------------+
       |                |                |                |
       v                v                v                v
+-------------+  +--------------+  +--------------+  +--------------+
| CPU Engine  |  | Memory/MMU   |  | Device Bus   |  | Block Layer  |
| Interpreter |  | RAM/ROM/IOMM |  | MMIO/IRQ/DMA |  | raw/qcow/q2  |
| LLVM ORC JIT|  | HVM-39       |  | HLIC/HPIC    |  | cache/snap   |
+-------------+  +--------------+  +--------------+  +--------------+
       |
       v
+------------------------------------------------------------------+
| Host Platform Abstraction                                         |
| files | mmap | threads | sockets | clock | terminal | signals     |
+------------------------------------------------------------------+
```

### 4.1 Core Runtime Modules

| Module | Responsibility |
| :--- | :--- |
| `hvm_core` | CPU state, instruction decode, interpreter, exceptions, privilege modes |
| `hvm_jit` | LLVM IR generation, ORC compile cache, block invalidation |
| `hvm_mem` | Physical memory map, ROM/RAM/MMIO regions, DMA, HVM-39 MMU |
| `hvm_dev` | Device base classes, MMIO register API, IRQ lines, DMA endpoints |
| `hvm_board` | Machine profiles, SoC composition, board wiring |
| `hvm_block` | Raw/QCOW/QCOW2 disk backends, block cache, flush semantics |
| `hvm_fw` | Boot ROM loading, firmware image layout, reset vectors |
| `hvm_trace` | Event tracing, instruction trace, device trace, perf counters |
| `hvm_monitor` | Human monitor, JSON monitor, scripting control |
| `hvm_host` | Cross-platform filesystem, memory map, threads, sockets, time |

---

## 5. HVM CPU Simulation

### 5.1 CPU State

Each virtual CPU has:

- 32 general-purpose 64-bit registers `r0..r31`, with `r0` hardwired to zero.
- Program counter `pc`.
- HVM status/control registers: privilege mode, interrupt enable, exception cause, trap value, timer compare, MMU root PPN, feature flags.
- Optional HVM-V vector state: `v0..v15`, `vl`, `vtype`, dirty/clean state.
- HVM-ARC state and atomic memory hooks.
- Optional HVM-L loop state for active hardware-loop counters and backedge metadata.
- Optional HVM-Alloc state through runtime TLS fields or allocation CSRs, depending on finalized ABI.
- Debug state: breakpoints, watchpoints, single-step latch.
- Execution counters: retired instructions, branches, branch-miss estimates, cache invalidations, MMIO exits, JIT block hits, ARC slow paths, allocation slow paths, vector utilization, DRAM byte estimates, and sleep/throttle residency.

### 5.2 Execution Modes

| Mode | Purpose | Required |
| :--- | :--- | :--- |
| Interpreter | Correct, portable baseline. Easiest to debug. | Yes |
| Threaded interpreter | Faster dispatch using computed-goto where available or dispatch tables. | Optional |
| LLVM ORC JIT | Hot basic block translation to host code. | Phase 2 |
| Deterministic lockstep | `HVM-R1` RT core pair comparison. | Yes for robotics profile |
| Trace-only dry run | Decode and trace without mutating devices. | Optional |

### 5.3 Decode Strategy

The decoder should be table-driven:

- A generated opcode table from `docs/hvm/hvm_instruction_set.csv`.
- A compact C++ decode table for base32 instructions.
- Support for HVM-C 16-bit compressed instructions.
- Support for `escape32` and future extension spaces.
- Instruction metadata: mnemonic, operand layout, side effects, privilege requirement, memory access class, vector requirement, advisory-hint behavior, and feature-bit requirement.

Generated files:

| Generated File | Source |
| :--- | :--- |
| `generated/hvm_opcode_table.hpp` | CSV instruction definitions |
| `generated/hvm_decode.inc` | Packed decode masks |
| `generated/hvm_disasm.inc` | Mnemonic and operand printers |
| `generated/hvm_features.hpp` | Feature-bit mapping |

### 5.4 Exceptions and Interrupts

The simulator should use HVM-native names:

- `HVM-39` MMU, not RISC-V `Sv39`.
- `HLIC` local interrupt controller, not `CLINT`.
- `HPIC` platform interrupt controller, not `PLIC`.
- `hcause`, `htval`, `hstatus`, `hie`, `hip` style HVM registers.

Exception classes:

- Instruction page fault
- Load page fault
- Store page fault
- Illegal instruction
- Privileged instruction
- Misaligned fetch/load/store
- Breakpoint
- Watchpoint
- Device bus fault
- Machine check
- Robotics safety fault

### 5.5 HVM-39 MMU

Functional requirements:

- Three-level page table walk using `VA[38:0]`.
- 4 KB base pages.
- Optional large-page support once specified.
- Permission checks for read/write/execute/user/supervisor.
- Accessed/dirty bit behavior configurable by machine profile.
- TLB with deterministic invalidation.
- `ICACHE.RNG` invalidates translated instruction cache and JIT blocks overlapping a range.
- DMA access can bypass or use IOMMU depending on device model.

---

## 6. LLVM ORC JIT Design

### 6.1 Translation Unit

The JIT translates one guest basic block or trace into one host function:

```cpp
using JitBlockFn = HvmExitReason (*)(HvmCpuState*, HvmRuntime*);
```

Each JIT block:

- Starts at a physical address plus privilege/MMU context key.
- Contains a bounded number of guest instructions.
- Ends on branch, trap, MMIO access, page boundary, interrupt poll, or self-modifying-code hazard.
- Returns an exit reason and updates `cpu->pc`.

### 6.2 JIT Cache Key

```text
JitKey = {
  physical_page_id,
  pc_offset,
  privilege_mode,
  feature_mask,
  mmu_context_generation,
  code_page_generation
}
```

Any write to executable memory increments the code-page generation. `ICACHE.RNG` invalidates matching cache entries.

### 6.3 JIT Safety Rules

- Interpreter must remain the reference engine.
- Every JIT instruction lowering must have differential tests against the interpreter.
- JIT must preserve precise exception order for memory and privilege faults.
- MMIO reads/writes should call back to the simulator runtime instead of being inlined.
- Atomics must route through memory subsystem helpers to preserve HVM-ARC semantics.
- JIT is disabled by default for `HVM-R1` lockstep safety testing until deterministic behavior is proven.

### 6.4 HVM-V Lowering

Initial JIT support should lower vector operations to:

1. Scalar helper loops for correctness.
2. LLVM fixed vectors for known `VLEN` profiles.
3. LLVM scalable vector intrinsics later if host support and semantics are stable.

---

## 7. Machine Profiles

### 7.1 Common Machine Contract

Every machine profile must define:

- Machine name and version.
- CPU count, topology, and feature bits.
- Reset vector.
- ROM/RAM layout.
- MMIO region layout.
- Interrupt topology.
- Timer frequency.
- Storage attachments.
- Firmware images.
- Device tree and/or ACPI table generation.
- Optional display, network, serial, and input devices.

### 7.2 Mobile Device: `hvm-mobile`

Models `HVM-M1`.

| Component | Simulation Requirement |
| :--- | :--- |
| CPU | 6 cores: 2 big + 4 little |
| Vector | 128-bit HVM-V on big cores; optional on little cores |
| RAM | LPDDR5/LPDDR5X memory controller model |
| Storage | UFS-like controller backed by raw/QCOW/QCOW2 image |
| Display | MIPI-DSI logical framebuffer device |
| Camera | Stub MIPI-CSI sensor frames from host files or generated test pattern |
| PMIC | Power states, wake events, battery level, thermal throttling |
| Wireless | Stub modem and Wi-Fi device; user-mode network optional |
| Input | Touchscreen, buttons, accelerometer event injection |
| Firmware | HVM Boot ROM -> HVM FSBL -> U-Boot/mobile loader |

Suggested CLI:

```bash
hvm-sim run --machine hvm-mobile --ram 8G --drive ufs0=os.qcow2 --display sdl
```

### 7.3 Desktop Device: `hvm-desktop`

Models `HVM-D1`.

| Component | Simulation Requirement |
| :--- | :--- |
| CPU | 8 big cores |
| Vector | 256-bit HVM-V |
| RAM | DDR5 dual-channel functional model |
| Storage | NVMe-like PCIe block device backed by raw/QCOW/QCOW2 |
| GPU | Simple framebuffer plus optional virtio-like GPU |
| PCIe | Root complex, config space, BARs, MSI-like interrupts |
| USB | Minimal keyboard/tablet and storage stubs |
| Network | User-mode virtual NIC or socket-backed NIC |
| Firmware | HVM Boot ROM -> HVM-SFI -> U-Boot/coreboot/EDK II payload |

Suggested CLI:

```bash
hvm-sim run --machine hvm-desktop --smp 8 --ram 16G --drive nvme0=desktop.qcow2 --net user
```

### 7.4 Server Device: `hvm-server`

Models `HVM-S1`.

| Component | Simulation Requirement |
| :--- | :--- |
| CPU | Configurable 16, 32, 64, or 128 cores per socket; 1P first, 2P later |
| Vector | 512-bit HVM-V baseline; wider vector mode optional |
| RAM | DDR5 ECC RDIMM functional model, NUMA layout |
| Storage | Multiple NVMe/U.2 block devices with QCOW2 overlays |
| PCIe | Multi-root topology, accelerator placeholders, ATS/PRI flags |
| BMC | OpenBMC-like management controller model or external BMC stub |
| Network | 10/100 GbE logical NIC model |
| RAS | ECC injection, machine-check injection, thermal/power events |
| Firmware | HVM Boot ROM -> HVM-SFI -> server firmware -> OS/hypervisor |

Suggested CLI:

```bash
hvm-sim run --machine hvm-server --sockets 1 --smp 64 --ram 512G --drive nvme0=server.qcow2 --bmc
```

### 7.5 Robotics Device: `hvm-robot`

Models `HVM-R1`.

| Component | Simulation Requirement |
| :--- | :--- |
| CPU | 2 deterministic RT cores + 2 app cores |
| Lockstep | Dual-core lockstep comparator mode |
| RAM | App RAM plus ECC SRAM for RT island |
| Storage | eMMC/UFS/NVMe-like block backend |
| Fieldbus | CAN-FD, UART, SPI, I2C, GPIO |
| Motor control | PWM, ADC, QEI/encoder, safety fault pins |
| Ethernet | TSN/PTP timestamp model |
| Safety | Fault injection, safe-state latching, watchdog, brownout events |
| Firmware | Safety FSBL first, then RT firmware and app firmware |

Suggested CLI:

```bash
hvm-sim run --machine hvm-robot --ram 2G --drive emmc0=robot.qcow2 --fault-script tests/robot_faults.toml
```

---

## 8. Device Model

### 8.1 Device Base Classes

```cpp
class HvmDevice {
public:
  virtual std::string_view name() const = 0;
  virtual void reset() = 0;
  virtual HvmReadResult read(uint64_t offset, unsigned size) = 0;
  virtual HvmWriteResult write(uint64_t offset, uint64_t value, unsigned size) = 0;
  virtual void tick(HvmTime now) {}
};

class HvmDmaDevice {
public:
  virtual void attach_dma(HvmDmaBus& dma) = 0;
};
```

### 8.2 Required Devices

| Device | Profiles | Purpose |
| :--- | :--- | :--- |
| Boot ROM | All | Reset image and first instruction fetch |
| HLIC | All | Per-core timers, IPIs, local interrupts |
| HPIC | All | Platform interrupt routing |
| UART16550-compatible HVM UART | All | Early console and logs |
| HVM Timer | All | OS scheduler ticks and profiling |
| HVM Reset/Power Controller | All | Reset, shutdown, low-power events |
| HVM Block | All | UFS/NVMe/eMMC abstraction |
| HVM Framebuffer | Mobile/Desktop | Display output |
| HVM Input | Mobile/Desktop | Keyboard, pointer, touch |
| HVM NIC | Desktop/Server | Network boot and OS networking |
| HVM BMC bridge | Server | Management controller |
| HVM CAN-FD | Robotics | Fieldbus |
| HVM PWM/ADC/QEI | Robotics | Motor-control loop simulation |

### 8.3 MMIO Register Model

Device register declarations should be data-driven:

```cpp
struct RegisterSpec {
  uint32_t offset;
  uint32_t width;
  Access access;
  uint64_t reset_value;
  RegisterCallback on_read;
  RegisterCallback on_write;
};
```

Benefits:

- Easier documentation generation.
- Easier trace output.
- Easier reset and snapshot support.
- Easier fuzz testing of device register behavior.

---

## 9. Block Layer and QCOW/QCOW2 Support

### 9.1 Block API

```cpp
class BlockDevice {
public:
  virtual uint64_t size_bytes() const = 0;
  virtual void read(uint64_t offset, std::span<std::byte> out) = 0;
  virtual void write(uint64_t offset, std::span<const std::byte> in) = 0;
  virtual void flush() = 0;
  virtual void discard(uint64_t offset, uint64_t length) = 0;
};
```

Backends:

- `RawBlockDevice`
- `VhdxBlockDevice`
- `HsdBlockDevice`
- `Qcow1BlockDevice`
- `Qcow2BlockDevice`
- `OverlayBlockDevice`
- `SnapshotBlockDevice`

### 9.2 Raw Sparse Image Support

Raw sparse images are the required minimum backend:

- Read/write sector ranges directly from a host file.
- Use 64-bit offsets.
- Support sparse allocation where host filesystems permit it.
- Support explicit flush and discard.
- Support a sidecar metadata file only for simulator-specific annotations, never for required disk contents.

Raw images should be the first boot path for all machine profiles.

### 9.3 VHDX Support Scope

VHDX is the preferred permissive interoperable sparse format if QCOW/QCOW2 is not acceptable:

Phase 1:

- Header and region table parse.
- Log replay or safe rejection of dirty images.
- Block allocation table parse.
- Dynamic disk read/write.
- Sector bitmap handling.
- Metadata table parse for disk size, logical sector size, physical sector size, and parent locator detection.
- Flush semantics and image close consistency.

Phase 2:

- Differencing disks.
- Parent locator resolution.
- Image check and repair.
- Host sparse-file optimization.
- Conversion to/from raw and HSD.

Phase 3:

- Performance tuning with metadata cache.
- Optional read-only support for fixed VHD v1 as an import/export convenience.

### 9.4 HSD Native Sparse Disk Format

HSD is a simple HVM-owned sparse disk format to avoid dependency and licensing risk. It is designed for simulator correctness, predictable crash recovery, easy tooling, and permissive licensing. It should be easier to implement than QCOW2 while retaining the features HVM needs most: sparse allocation, backing images, external snapshots, integrity checks, and optional compression.

Recommended extension: `.hsd`.

Recommended MIME-like identifier: `application/vnd.hvm.sparse-disk`.

#### 9.4.1 Design Principles

- All multibyte integers are little-endian.
- All on-disk structure sizes are explicit and versioned.
- Every metadata block has a checksum.
- Metadata updates are journaled or copy-on-write committed.
- Data clusters are immutable while referenced by snapshots.
- Unknown incompatible feature bits must make the image fail to open.
- Unknown compatible feature bits may be ignored.
- No required external libraries except standard C++ file I/O and optional compression/checksum libraries.
- The format must support read-only open even when optional write features are not implemented.

#### 9.4.2 File Layout

```text
+------------------------------+
| HSD Header                   |  fixed at offset 0
+------------------------------+
| Header Extension Area        |  optional, aligned to 4 KiB
+------------------------------+
| Active Metadata Superblock A |  primary metadata root
+------------------------------+
| Active Metadata Superblock B |  backup metadata root
+------------------------------+
| L1 Mapping Table             |
+------------------------------+
| L2 Mapping Tables            |
+------------------------------+
| Refcount Table               |
+------------------------------+
| Refcount Blocks              |
+------------------------------+
| Snapshot Directory           |
+------------------------------+
| Backing File Descriptor      |
+------------------------------+
| Data Clusters                |
+------------------------------+
| Journal / Transaction Log    |  optional but recommended
+------------------------------+
```

Alignment:

- Header starts at byte 0.
- Metadata structures are aligned to 4 KiB.
- Data clusters are aligned to `cluster_size`.
- The default `cluster_size` is 2 MiB.
- Valid `cluster_size` values are powers of two from 64 KiB to 16 MiB.

#### 9.4.3 Header

```c
struct HsdHeader {
  char     magic[8];              // "HSDISK01"
  uint32_t header_size;           // sizeof(HsdHeader), initially 256
  uint32_t header_crc32c;         // checksum with this field zeroed
  uint16_t major_version;         // initially 1
  uint16_t minor_version;         // initially 0
  uint32_t compatible_features;
  uint32_t incompatible_features;
  uint32_t readonly_compatible_features;
  uint64_t virtual_size;          // bytes visible to guest
  uint32_t logical_sector_size;   // usually 512 or 4096
  uint32_t physical_sector_size;  // usually 4096
  uint64_t cluster_size;          // bytes per allocatable data unit
  uint64_t metadata_a_offset;
  uint64_t metadata_b_offset;
  uint64_t active_metadata_generation;
  uint8_t  image_uuid[16];
  uint8_t  parent_uuid[16];       // zero if no parent
  uint64_t create_time_unix_ns;
  uint64_t modify_time_unix_ns;
  uint8_t  reserved[128];
};
```

Initial feature assignments:

| Bit | Class | Name | Meaning |
| :---: | :--- | :--- | :--- |
| 0 | compatible | `HSD_FEATURE_LABELS` | Metadata labels may be present |
| 1 | compatible | `HSD_FEATURE_TRACE_HINTS` | Simulator trace hints may be present |
| 0 | readonly-compatible | `HSD_RO_COMPAT_ZSTD` | Data clusters may be zstd-compressed |
| 1 | readonly-compatible | `HSD_RO_COMPAT_BACKING_FILE` | Reads may fall through to parent image |
| 2 | readonly-compatible | `HSD_RO_COMPAT_SNAPSHOTS` | Snapshot directory may be present |
| 0 | incompatible | `HSD_INCOMPAT_JOURNAL` | Journal replay is required before write |
| 1 | incompatible | `HSD_INCOMPAT_ENCRYPTED` | Encrypted metadata/data is present |

#### 9.4.4 Metadata Superblock

Two metadata superblocks allow atomic root switching. Writers update inactive metadata roots first, flush, then atomically bump the active generation.

```c
struct HsdMetadataSuperblock {
  char     magic[8];              // "HSDMETA1"
  uint32_t size;
  uint32_t crc32c;
  uint64_t generation;
  uint64_t l1_offset;
  uint64_t l1_entries;
  uint64_t refcount_table_offset;
  uint64_t refcount_table_entries;
  uint64_t snapshot_dir_offset;
  uint64_t snapshot_dir_entries;
  uint64_t backing_desc_offset;
  uint64_t backing_desc_size;
  uint64_t journal_offset;
  uint64_t journal_size;
  uint64_t free_list_offset;
  uint64_t free_list_entries;
  uint8_t  reserved[128];
};
```

Open procedure:

1. Read header.
2. Validate magic, size, version, feature bits, and checksum.
3. Read both metadata superblocks.
4. Pick the valid superblock with the highest generation.
5. If journal feature is set, replay or reject according to open mode.
6. Load mapping and refcount metadata lazily.

#### 9.4.5 Cluster Mapping

HSD uses two-level mapping:

```text
guest_offset
  -> virtual_cluster_index = guest_offset / cluster_size
  -> l1_index = virtual_cluster_index / l2_entries_per_table
  -> l2_index = virtual_cluster_index % l2_entries_per_table
  -> L1[l1_index] gives L2 table host offset
  -> L2[l2_index] gives data cluster descriptor
```

```c
struct HsdL2Entry {
  uint64_t host_offset;           // data cluster offset, 0 if unallocated
  uint32_t stored_size;           // compressed size or cluster_size
  uint16_t compression;           // 0 none, 1 zstd
  uint16_t flags;
  uint32_t checksum;              // data checksum
  uint32_t refcount_hint;
};
```

L2 flags:

| Flag | Meaning |
| :--- | :--- |
| `ALLOCATED` | Cluster exists in this file |
| `ZERO` | Reads return zero without storage |
| `COMPRESSED` | Cluster is compressed |
| `PARENT` | Reads fall through to backing image |
| `DIRTY` | Cluster write was interrupted; repair required |

Read behavior:

1. If L2 table is missing, read from parent if present, otherwise return zeros.
2. If entry is `ZERO`, return zeros.
3. If entry is `PARENT`, read from backing file.
4. If entry is `COMPRESSED`, read full compressed cluster, decompress to a scratch cluster, then copy requested range.
5. Otherwise read requested range from `host_offset + cluster_offset`.

Write behavior:

1. Allocate L2 table if missing.
2. If writing a partial cluster that has parent data, materialize the full cluster first.
3. If overwriting a full cluster, allocate a new cluster or reuse when refcount is 1.
4. Update data cluster.
5. Update L2 entry.
6. Update refcount metadata.
7. Commit metadata generation.

#### 9.4.6 Refcounts, Snapshots, and Backing Files

Every allocated cluster has a refcount. A cluster with refcount greater than 1 is shared by a snapshot and must not be overwritten in place.

Rules:

- Refcount 0 means free.
- Refcount 1 means owned by active image.
- Refcount greater than 1 means shared by active image and snapshots.
- Refcount overflow is a hard image-check error.
- Metadata clusters also have refcounts.

External snapshots are preferred for v1:

```text
base.hsd      read-only base
overlay.hsd   parent_uuid = base.uuid, changed clusters only
```

Backing descriptor:

```json
{
  "format": "hsd-backing-v1",
  "path": "../base.hsd",
  "uuid": "01234567-89ab-cdef-0123-456789abcdef",
  "virtual_size": 34359738368,
  "required": true
}
```

Rules:

- Relative paths are resolved relative to the child image path.
- The parent UUID must match unless `--unsafe-parent` is explicitly passed.
- Parent image is opened read-only.
- Parent chain depth defaults to a maximum of 8.

#### 9.4.7 Journal and Crash Recovery

HSD v1 should start with copy-on-write metadata root switching:

1. Write new data clusters.
2. Write new metadata blocks.
3. Flush data and metadata.
4. Write inactive metadata superblock with incremented generation.
5. Flush again.
6. Update header active generation.

This ensures an interrupted update opens with either the old valid metadata root or the new valid metadata root. A richer journal can be added later if metadata churn becomes expensive.

Future journal record types:

- `BEGIN`
- `WRITE_METADATA`
- `WRITE_DATA`
- `UPDATE_REFCOUNT`
- `UPDATE_L2`
- `COMMIT`
- `ABORT`

#### 9.4.8 Integrity and Compression

Minimum integrity:

- CRC32C for headers and metadata blocks.
- Optional data-cluster checksum.

Recommended integrity:

- CRC32C for fast metadata corruption detection.
- BLAKE3 or SHA-256 for optional full-image verification manifests.
- External release manifest as `image.hsd.manifest.json`.

Compression rules:

- Compression is optional and per-cluster.
- Compression is only used for full clusters.
- Partial writes to compressed clusters materialize, modify, then optionally recompress.
- zstd is preferred for new compressed clusters.
- Uncompressed clusters remain the default for performance and debuggability.

#### 9.4.9 Allocation Policy

The allocator should support:

- Append-only allocation for simple writes.
- Free-list reuse after discard or snapshot deletion.
- Cluster zero detection to avoid allocating all-zero clusters.
- Optional preallocation for benchmark images.

Allocation modes:

| Mode | Behavior |
| :--- | :--- |
| `sparse` | Allocate clusters on first write |
| `prealloc-metadata` | Allocate metadata tables up front |
| `prealloc-full` | Allocate all data clusters up front |
| `append-only` | Never reuse freed clusters until compact |

#### 9.4.10 `hvm-img` HSD Commands

```bash
hvm-img create -f hsd -s 32G root.hsd
hvm-img create -f hsd -s 32G --cluster-size 2M root.hsd
hvm-img info root.hsd
hvm-img check root.hsd
hvm-img repair root.hsd
hvm-img convert -f raw -O hsd root.img root.hsd
hvm-img convert -f hsd -O raw root.hsd root.img
hvm-img snapshot create root.hsd clean-boot
hvm-img snapshot list root.hsd
hvm-img snapshot delete root.hsd clean-boot
hvm-img overlay create -b base.hsd work.hsd
hvm-img compact root.hsd
hvm-img verify root.hsd --manifest root.hsd.manifest.json
```

#### 9.4.11 Implementation Classes

```cpp
class HsdBlockDevice final : public BlockDevice {
public:
  static expected<std::unique_ptr<HsdBlockDevice>, HsdError>
  open(const HsdOpenOptions& options);

  uint64_t size_bytes() const override;
  void read(uint64_t offset, std::span<std::byte> out) override;
  void write(uint64_t offset, std::span<const std::byte> in) override;
  void flush() override;
  void discard(uint64_t offset, uint64_t length) override;

private:
  HsdHeader header_;
  HsdMetadataSuperblock meta_;
  HsdMappingCache mapping_cache_;
  HsdRefcountManager refcounts_;
  HsdAllocator allocator_;
  std::unique_ptr<BlockDevice> parent_;
};
```

Recommended helpers:

- `HsdHeaderReader`
- `HsdMetadataRoot`
- `HsdMappingTable`
- `HsdRefcountManager`
- `HsdAllocator`
- `HsdSnapshotManager`
- `HsdIntegrity`
- `HsdCompactor`

#### 9.4.12 Validation Matrix

| Test | Requirement |
| :--- | :--- |
| Empty image reads | Return zeros for entire virtual disk |
| Single cluster write | Reopen and read exact bytes |
| Partial cluster write | Preserve unwritten bytes from zero or parent |
| Backing file read | Child reads parent data when unallocated |
| Overlay write | Parent remains unchanged |
| Snapshot create/delete | Refcounts are correct before and after |
| Crash during metadata update | Opens with old or new root, never corrupt half-state |
| Corrupt header checksum | Image is rejected |
| Corrupt metadata checksum | Image is rejected or repaired from backup |
| Discard range | Reads return zero or parent data as specified |
| Compact | Image remains byte-identical at virtual block level |
| Cross-platform | Same image works on Windows, Linux, and macOS |

The first HSD implementation should remain intentionally simpler than QCOW2. The goal is predictable simulator behavior, not maximum feature parity with virtual machine ecosystems.

### 9.5 QCOW Support Scope

QCOW version 1 should be supported primarily for compatibility:

- Header parse
- Backing file reference
- Cluster lookup
- Read/write clusters
- Optional zlib compressed cluster read
- Big-endian metadata handling

No new HVM images should default to QCOW v1.

### 9.6 QCOW2 Support Scope

Phase 1:

- Header parse and validation.
- L1/L2 table lookup.
- Standard clusters.
- Refcount table and refcount blocks.
- Host offset allocation.
- Backing file chain read support.
- Read/write support for standard unencrypted images.
- zlib compressed cluster read.
- Metadata cache and flush.
- `qemu-img` compatibility tests.

Phase 2:

- Internal snapshots.
- Lazy refcounts.
- Zero clusters.
- Discard/TRIM.
- zstd compressed cluster read/write if selected.
- Image check and repair tool.

Phase 3:

- AES-CBC compatibility for legacy encrypted images if needed.
- LUKS only if a strong product requirement exists.
- External data file support only if explicitly required.

### 9.7 QCOW2 Clean-Room Rules

- Use public format documentation and independently written tests.
- Do not copy QEMU block driver code.
- Do not use QEMU test images if their licensing is unclear.
- Generate HVM-owned fixture images with a small compatibility generator, plus externally generated `qemu-img` images for black-box tests.
- Keep a spec citation trail in the source comments for metadata structures, without copying prose.

### 9.8 Disk Image Tooling

Provide:

```bash
hvm-img info disk.qcow2
hvm-img check disk.qcow2
hvm-img create -f qcow2 -s 32G disk.qcow2
hvm-img convert -f raw -O qcow2 rootfs.img rootfs.qcow2
hvm-img create -f vhdx -s 32G disk.vhdx
hvm-img create -f hsd -s 32G disk.hsd
hvm-img convert -f raw -O vhdx rootfs.img rootfs.vhdx
hvm-img convert -f qcow2 -O hsd rootfs.qcow2 rootfs.hsd
hvm-img snapshot create disk.qcow2 clean-boot
hvm-img rebase overlay.qcow2 base.qcow2
```

`hvm-img` should be a separate executable using the same `hvm_block` library as the simulator.

---

## 10. Firmware and Boot Flow

### 10.1 Boot Inputs

The simulator should accept:

- Boot ROM image.
- FSBL image.
- Firmware image.
- Kernel image.
- Initrd image.
- Device tree blob or generated device tree.
- Optional ACPI/SMBIOS generation for desktop/server.

### 10.2 Default Boot Flow

```
reset
  -> map Boot ROM at reset vector
  -> initialize CPU state and board straps
  -> Boot ROM loads FSBL from simulated SPI/UFS/NVMe
  -> FSBL initializes RAM and HVM-SFI
  -> firmware loads kernel or OS bootloader
  -> OS discovers devices through DT or ACPI
```

### 10.3 Firmware Development Hooks

- `--bios firmware.bin`
- `--kernel kernel.elf`
- `--dtb board.dtb`
- `--append "console=hvm0 root=/dev/hvblk0"`
- `--semihosting`
- `--break-reset`
- `--trace-fw`

---

## 11. Debugging, Tracing, and Monitor

### 11.1 Human Monitor

Commands:

```text
info machine
info cpus
info mem
info irq
info block
xp /16gx 0x80000000
regs cpu0
step cpu0 10
break 0x100000
watch 0x80001000 w
trace enable mmio
inject irq hpic 32
savevm checkpoint1
loadvm checkpoint1
quit
```

### 11.2 JSON Monitor

Provide a stable JSON monitor over stdio or TCP:

```json
{"execute":"query-cpus"}
{"execute":"stop"}
{"execute":"cont"}
{"execute":"human-monitor-command","arguments":{"command-line":"info block"}}
```

This does not need to match QMP exactly, but familiarity helps integration.

### 11.3 Trace Events

Trace categories:

- `cpu.decode`
- `cpu.exec`
- `cpu.exception`
- `jit.compile`
- `jit.invalidate`
- `mmu.walk`
- `mmio.read`
- `mmio.write`
- `irq.raise`
- `irq.lower`
- `block.read`
- `block.write`
- `qcow.alloc`
- `robot.safety`
- `robot.pwm`

Trace output formats:

- Human text
- JSON lines
- Chrome trace event format
- Optional SQLite trace database

---

## 12. Timing and Determinism

### 12.1 Functional Time

Each CPU executes instruction quanta. Devices advance on virtual time:

- Timer frequency is machine-profile dependent.
- Block latency can be zero, fixed, or configured.
- Network latency can be zero or configured.
- Robotics PWM/ADC timing must support deterministic scheduled events.

### 12.2 Deterministic Mode

`--deterministic` should:

- Use a fixed scheduler seed.
- Disable host wall-clock coupling.
- Use deterministic virtual timers.
- Serialize device events in a stable order.
- Force single-thread execution unless explicitly testing SMP races.

### 12.3 Parallel Mode

For performance:

- Run vCPUs on host threads.
- Use atomic interrupt lines.
- Use lock-free or sharded TLB/JIT caches.
- Keep device MMIO serialized through the machine event loop.
- Provide record/replay later for difficult bugs.

---

## 13. Host Platform Abstraction

### 13.1 Supported Hosts

| Host | Compiler | Notes |
| :--- | :--- | :--- |
| Windows 11 | clang-cl or MSVC-compatible Clang | Use Win32 file mapping, IOCP or libuv/Asio |
| Linux | clang++ | Use POSIX mmap, epoll through libuv/Asio |
| macOS | Apple Clang or upstream Clang | Use Mach/POSIX APIs, codesign handling for JIT pages |

### 13.2 JIT Page Permissions

The JIT must respect W^X:

- Allocate writable code buffers.
- Finalize and mark executable.
- On macOS ARM64, account for platform hardened runtime and code-signing constraints.
- Provide `--jit=off` fallback everywhere.

### 13.3 Filesystem and Large File Support

- Use `std::filesystem` for path handling.
- Use 64-bit offsets for images.
- Use platform-specific pread/pwrite wrappers, not shared seek state.
- Support sparse files where the host filesystem supports them.
- Keep flush semantics explicit for data integrity tests.

---

## 14. CMake and vcpkg Structure

### 14.1 Repository Layout

```text
sim/
  CMakeLists.txt
  vcpkg.json
  CMakePresets.json
  include/hvm-sim/
  src/
    core/
    jit/
    mem/
    dev/
    board/
    block/
    fw/
    host/
    monitor/
    trace/
  tools/
    hvm-sim/
    hvm-img/
    hvm-trace/
  tests/
    unit/
    integration/
    images/
```

### 14.2 CMake Targets

| Target | Type | Purpose |
| :--- | :--- | :--- |
| `hvm_sim_core` | static/shared lib | CPU, memory, devices, machine runtime |
| `hvm_sim_jit` | optional lib | LLVM ORC JIT |
| `hvm_sim_block` | lib | raw/QCOW/QCOW2 |
| `hvm_sim_ui` | optional lib | SDL/ImGui display/debug UI |
| `hvm-sim` | executable | system simulator |
| `hvm-img` | executable | disk image utility |
| `hvm-trace` | executable | trace inspection |
| `hvm-sim-tests` | executable | unit tests |

### 14.3 Example `vcpkg.json`

```json
{
  "name": "hvm-sim",
  "version-string": "0.1.0",
  "dependencies": [
    "cli11",
    "fmt",
    "spdlog",
    "nlohmann-json",
    "tomlplusplus",
    "zlib",
    "zstd",
    "catch2"
  ],
  "features": {
    "jit": {
      "description": "Enable LLVM ORC JIT",
      "dependencies": ["llvm"]
    },
    "ui": {
      "description": "Enable SDL/ImGui display and debug UI",
      "dependencies": ["sdl3", "imgui"]
    },
    "crypto": {
      "description": "Enable optional encrypted image compatibility",
      "dependencies": ["openssl"]
    }
  }
}
```

### 14.4 Example CMake Options

```cmake
option(HVM_SIM_ENABLE_JIT "Enable LLVM ORC JIT backend" ON)
option(HVM_SIM_ENABLE_UI "Enable SDL/ImGui UI" OFF)
option(HVM_SIM_ENABLE_QCOW_WRITE "Enable QCOW/QCOW2 write support" ON)
option(HVM_SIM_ENABLE_CRYPTO "Enable encrypted disk image compatibility" OFF)
option(HVM_SIM_BUILD_TOOLS "Build hvm-img and trace tools" ON)
option(HVM_SIM_BUILD_TESTS "Build tests" ON)
```

### 14.5 Build Commands

```bash
cmake --preset linux-clang-debug
cmake --build --preset linux-clang-debug
ctest --preset linux-clang-debug
```

Windows:

```powershell
cmake --preset windows-clangcl-debug
cmake --build --preset windows-clangcl-debug
ctest --preset windows-clangcl-debug
```

macOS:

```bash
cmake --preset macos-clang-debug
cmake --build --preset macos-clang-debug
ctest --preset macos-clang-debug
```

---

## 15. Testing Strategy

### 15.1 Unit Tests

- Instruction decode and encode.
- Interpreter instruction semantics.
- MMU page walks and permission faults.
- HLIC/HPIC interrupt priority and delivery.
- Device register reset/read/write behavior.
- QCOW/QCOW2 header parse.
- QCOW/QCOW2 cluster lookup and allocation.
- Block cache flush and crash consistency.
- JIT vs interpreter differential execution.

### 15.2 Integration Tests

- Boot minimal Boot ROM and write to UART.
- Boot HVM FSBL and initialize RAM.
- Boot mobile profile to firmware prompt.
- Boot desktop profile with framebuffer and block device.
- Boot server profile with multiple vCPUs and BMC stub.
- Boot robotics profile and run PWM/ADC/CAN-FD loop.
- Attach QCOW2 overlay with backing file.
- Run `hvm-img check` on all generated fixtures.

### 15.3 Differential Tests

For every instruction:

1. Generate random CPU and memory state.
2. Execute one instruction in interpreter.
3. Execute same instruction in JIT or independent reference helper.
4. Compare CPU state, memory state, exceptions, and device side effects.

For QCOW2:

1. Generate images with `hvm-img`.
2. Validate using HVM's parser.
3. Cross-check selected images with external `qemu-img info/check` in CI where licensing and environment permit.
4. Never link against QEMU.

### 15.4 Fuzzing

Fuzz targets:

- HVM instruction decoder.
- HVM-39 page table walker.
- MMIO register dispatcher.
- QCOW/QCOW2 metadata parser.
- Monitor JSON parser.
- Device tree/ACPI parser if included.

Use libFuzzer through Clang where available.

---

## 16. Performance Plan

### 16.1 Performance Milestones

| Milestone | Goal |
| :--- | :--- |
| M0 | Correct interpreter boots Boot ROM |
| M1 | Interpreter boots minimal firmware and runs tests |
| M2 | Threaded interpreter reaches usable firmware speed |
| M3 | LLVM JIT accelerates integer/control-flow workloads |
| M4 | JIT supports MMU, atomics, HVM-ARC, and HVM-C |
| M5 | JIT supports HVM-V hot vector loops |
| M6 | Parallel vCPU mode supports desktop/server workloads |

### 16.2 Hot Paths

- Instruction decode cache.
- TLB lookup.
- JIT block lookup.
- RAM read/write.
- MMIO dispatch.
- QCOW2 L2 table lookup and cluster cache.
- Interrupt polling.

### 16.3 Caches

| Cache | Key | Invalidation |
| :--- | :--- | :--- |
| Decode cache | physical PC + code generation | code write, `ICACHE.RNG` |
| JIT block cache | `JitKey` | code write, MMU context change, feature change |
| TLB | ASID + virtual page + privilege | page table update, TLB flush |
| Block cache | image cluster offset | writeback/flush/discard |
| QCOW metadata cache | L1/L2/refcount block offset | metadata write/flush |

---

## 17. Security and Robustness

- Treat all guest inputs and disk images as untrusted.
- Bounds-check all memory and image accesses.
- Use checked integer arithmetic for QCOW offsets and sizes.
- Reject malformed images with precise errors.
- Keep guest-to-host interaction behind explicit devices.
- Default network mode should be disabled or user-mode only.
- Avoid host device passthrough in early versions.
- Use sanitizers in CI: ASan, UBSan, TSan where practical.
- Use fuzzing for disk images and device register inputs.

---

## 18. Roadmap

The simulator should be implemented as a staged hybrid system:

```text
hvm-sim C++ framework
  |
  +-- CPU backend A: C++ interpreter       correctness and debug baseline
  +-- CPU backend B: Verilated RTL model   hardware-accurate validation backend
  +-- CPU backend C: LLVM ORC JIT          performance backend
```

The C++ interpreter must land first because it is the fastest way to debug firmware, devices, and disk images. The Verilated RTL backend should be introduced once the instruction subset and bus contracts are stable enough to compare against the interpreter. The LLVM JIT should come after interpreter semantics are covered by tests.

### Phase 0: Repository, Build, and Governance

Goal: create the simulator project skeleton and prevent early architectural drift.

Tasks:

1. Create the simulator source tree:
   - `sim/include/hvm-sim/`
   - `sim/src/core/`
   - `sim/src/mem/`
   - `sim/src/dev/`
   - `sim/src/board/`
   - `sim/src/block/`
   - `sim/src/rtl/`
   - `sim/src/jit/`
   - `sim/src/monitor/`
   - `sim/src/trace/`
   - `sim/tools/hvm-sim/`
   - `sim/tools/hvm-img/`
   - `sim/tests/`
2. Add top-level CMake build files with targets for `hvm_sim_core`, `hvm_sim_block`, `hvm_sim_rtl`, `hvm_sim_jit`, `hvm-sim`, and `hvm-img`.
3. Add `vcpkg.json` with required dependencies and optional features for `jit`, `ui`, `crypto`, and `rtl`.
4. Add `CMakePresets.json` for:
   - `windows-clangcl-debug`
   - `windows-clangcl-release`
   - `linux-clang-debug`
   - `linux-clang-release`
   - `macos-clang-debug`
   - `macos-clang-release`
5. Define license policy in `sim/LICENSES.md`.
6. Add CI jobs for Windows, Linux, and macOS.
7. Enable formatting, static analysis, and sanitizers:
   - `clang-format`
   - `clang-tidy`
   - ASan/UBSan on Linux/macOS
   - MSVC/clang-cl warnings on Windows
8. Generate opcode metadata from `docs/hvm/hvm_instruction_set.csv`.

Deliverables:

- Buildable empty `hvm-sim` and `hvm-img` executables.
- CI builds all supported host OSes.
- Generated opcode table checked into build artifacts or generated at configure time.

Exit criteria:

- `cmake --preset <host>` and `ctest --preset <host>` pass on all three platforms.
- No GPL/LGPL dependency is linked into the core simulator by default.

### Phase 1: Core CPU Interpreter

Goal: implement the first correct HVM CPU execution backend.

Tasks:

1. Define `HvmCpuState`:
   - `r0..r31`
   - `pc`
   - privilege mode
   - HVM control/status registers
   - exception registers
   - interrupt pending/enabled bits
   - optional vector state descriptor
2. Implement instruction fetch:
   - physical ROM/RAM fetch for reset mode
   - HVM-39 translated fetch for supervisor/user mode
   - 16-bit HVM-C compressed fetch
   - 32-bit base instruction fetch
   - extension/escape instruction fetch
3. Implement instruction decode:
   - table-driven decode
   - operand extraction
   - disassembly printer
4. Implement base integer instructions:
   - arithmetic
   - logical
   - shifts
   - compares
   - branches/jumps
   - loads/stores
   - atomics needed by HVM-ARC
5. Implement exception handling:
   - illegal instruction
   - page fault
   - misaligned access
   - privilege violation
   - breakpoint/watchpoint
6. Implement HVM-C decompression and execution.
7. Implement HVM-ARC `RETAIN` and `RELEASE`.
8. Implement `ICACHE.RNG` as decode/JIT-cache invalidation hook.
9. Implement `LD.P` and `ST.P` pair load/store instructions with precise exception behavior.
10. Decode HVM-L hardware-loop instructions and execute them functionally as counted branches.
11. Decode `PREFETCH.R`, `PREFETCH.W`, `PREFETCH.NTA`, branch hints, and `MEMZERO.HINT` as architecturally safe no-ops until timing/performance models exist.
12. Add instruction unit tests and random-state differential microtests against simple reference helpers.

Deliverables:

- `hvm_sim_core` can execute a small in-memory HVM program.
- `hvm-sim --machine minimal --rom uart_hello.bin` prints expected UART text once devices exist in Phase 2.

Exit criteria:

- All base ISA tests pass.
- `r0` hardwire behavior is verified.
- Exception ordering is deterministic and documented.

### Phase 2: Memory, MMU, Interrupts, and Minimal Devices

Goal: create a minimal bootable machine.

Tasks:

1. Implement physical memory map:
   - RAM region
   - ROM region
   - MMIO region
   - reserved/unmapped regions
2. Implement HVM-39 MMU:
   - page table walk
   - permissions
   - accessed/dirty policy
   - TLB
   - deterministic invalidation
3. Implement device bus:
   - MMIO read/write dispatch
   - register width validation
   - side-effect tracing
4. Implement HLIC:
   - per-core timer
   - software interrupts
   - inter-processor interrupts
5. Implement HPIC:
   - platform interrupt lines
   - priority and pending state
   - interrupt claim/complete registers
6. Implement minimal devices:
   - UART
   - reset/power controller
   - timer
   - ROM loader
7. Implement monitor basics:
   - `info machine`
   - `info cpus`
   - `regs`
   - `step`
   - `break`
   - `quit`
8. Implement trace basics:
   - CPU instruction trace
   - MMIO trace
   - interrupt trace

Deliverables:

- Minimal machine boots a ROM and writes to UART.
- Monitor can stop, inspect, step, and continue execution.

Exit criteria:

- Boot ROM smoke test passes.
- MMU tests cover mapped, unmapped, permission, and misalignment cases.
- Timer interrupt test passes on at least two simulated cores.

### Phase 3: Block Layer and Disk Image Tools

Goal: add production-quality storage abstractions before full board profiles rely on disks.

Tasks:

1. Implement `BlockDevice` interface.
2. Implement `RawBlockDevice`:
   - read/write
   - sparse file support
   - flush
   - discard
3. Implement HSD:
   - header parser/writer
   - metadata superblocks
   - L1/L2 mapping
   - cluster allocation
   - refcounts
   - external backing file support
   - copy-on-write metadata root switching
   - `hvm-img check`
4. Implement VHDX if QCOW/QCOW2 is rejected or deferred:
   - header and region table
   - BAT
   - dynamic disk read/write
   - basic log replay or safe dirty-image rejection
5. Implement QCOW/QCOW2 only after clean-room/legal approval:
   - clean-room parser
   - compatibility fixtures
   - no QEMU source reuse
6. Implement `hvm-img`:
   - `create`
   - `info`
   - `check`
   - `repair`
   - `convert`
   - `overlay create`
   - `snapshot list/create/delete`
7. Add malformed-image fuzz tests.

Deliverables:

- `hvm-img create -f hsd -s 32G root.hsd`
- `hvm-img check root.hsd`
- Simulator can attach raw and HSD disks.

Exit criteria:

- Raw and HSD read/write/reopen tests pass on Windows, Linux, and macOS.
- HSD crash-update tests prove old-or-new-root recovery.
- Fuzz tests reject malformed images without crashes.

### Phase 4: Board Profile Framework

Goal: instantiate documented HVM devices as named machine profiles.

Tasks:

1. Define `MachineProfile` schema:
   - name
   - CPU topology
   - RAM regions
   - ROM regions
   - MMIO regions
   - interrupts
   - firmware paths
   - block devices
   - generated platform description
2. Implement `hvm-mobile`:
   - 2 big + 4 little cores
   - LPDDR5/LPDDR5X RAM model
   - UFS-like block device
   - PMIC stubs
   - framebuffer
   - MIPI DSI/CSI stubs
   - GPIO/I2C/SPI stubs
3. Implement `hvm-desktop`:
   - 8 big cores
   - DDR5 memory model
   - PCIe root complex
   - NVMe-like block device
   - framebuffer/input
   - USB stubs
   - NIC stub
4. Implement `hvm-server`:
   - configurable SMP
   - server memory map
   - ECC injection stubs
   - PCIe/NVMe
   - NIC
   - BMC bridge stub
5. Implement `hvm-robot`:
   - RT core pair + app cores
   - ECC SRAM
   - CAN-FD
   - PWM
   - ADC
   - QEI/encoder
   - watchdog
   - fault input pins
6. Generate device tree for mobile and robotics.
7. Generate minimal ACPI/SMBIOS later for desktop and server.

Deliverables:

- `hvm-sim run --machine hvm-mobile`
- `hvm-sim run --machine hvm-desktop`
- `hvm-sim run --machine hvm-server`
- `hvm-sim run --machine hvm-robot`

Exit criteria:

- Every profile boots to a ROM or firmware stub and exposes the expected device map.
- `info machine` prints CPU topology, memory map, IRQ map, and attached block devices.

### Phase 5: Firmware Bring-Up

Goal: boot real HVM firmware paths, not just simulator test ROMs.

Tasks:

1. Define HVM Boot ROM contract:
   - reset vector
   - ROM memory layout
   - FSBL load location
   - handoff registers
   - error reporting
2. Define HVM-SFI:
   - timer service
   - IPI service
   - reset/shutdown service
   - console service
   - firmware feature discovery
3. Implement direct firmware loading:
   - `--bios`
   - `--fsbl`
   - `--kernel`
   - `--initrd`
   - `--dtb`
   - `--append`
4. Add firmware logging and early console.
5. Add U-Boot/HVM-native firmware boot path.
6. Add direct kernel boot shortcut for OS work.
7. Add ACPI/SMBIOS path for desktop/server when OS requirements demand it.

Deliverables:

- Minimal FSBL runs on all four machine profiles.
- Direct kernel boot path works for a test kernel or simulator fixture.

Exit criteria:

- Firmware can discover UART, timer, memory, and block devices from generated platform description.
- Firmware failure emits clear trace and monitor diagnostics.

### Phase 6: Verilog/SystemVerilog RTL Model

Goal: create a hardware-accurate HVM CPU model that can be converted to C++ with Verilator and used inside the simulator.

Tasks:

1. Create RTL source tree:
   - `rtl/hvm_core/`
   - `rtl/hvm_decode/`
   - `rtl/hvm_execute/`
   - `rtl/hvm_lsu/`
   - `rtl/hvm_mmu/`
   - `rtl/hvm_hlic/`
   - `rtl/hvm_vector/`
   - `rtl/hvm_arc/`
   - `rtl/hvm_lockstep/`
   - `rtl/common/`
2. Define RTL scope for v1:
   - base integer ISA
   - HVM-C decode
   - HVM-ARC atomics
   - HVM-39 MMU
   - HLIC interrupt entry
   - minimal HVM-V scalarized or lane-based model
3. Define external bus interface:
   - instruction fetch channel
   - load/store channel
   - MMIO channel
   - interrupt inputs
   - timer input
   - reset/debug inputs
4. Keep the RTL bus transaction-level, not tied to a specific DDR/AXI implementation.
5. Add Verilator build target:
   - `hvm_rtl_verilated`
   - generated C++ compiled as `hvm_sim_rtl`
6. Add C++ wrapper:
   - `RtlCpuBackend`
   - converts simulator memory/device accesses into RTL bus responses
   - handles clock/reset stepping
   - exports register snapshots for comparison
7. Add RTL trace support:
   - optional VCD/FST output
   - per-instruction commit trace
   - bus transaction trace
8. Add RTL tests:
   - single instruction tests
   - branch tests
   - load/store tests
   - MMU tests
   - interrupt tests
   - HVM-ARC tests

Deliverables:

- `hvm-sim run --cpu-backend rtl --machine minimal --rom uart_hello.bin`
- Verilated RTL backend can execute the same smoke programs as the interpreter.

Exit criteria:

- RTL and interpreter agree on architectural state for all Phase 1 instructions.
- RTL can run deterministically with a fixed clock count per instruction or commit event.
- Verilator-generated code builds on Linux first, then Windows/macOS as supported by toolchain availability.

### Phase 7: Differential Testing Across Backends

Goal: make the interpreter, Verilated RTL, and later JIT mutually validating.

Tasks:

1. Define architectural state comparison:
   - registers
   - `pc`
   - privilege state
   - exception state
   - memory writes
   - interrupt side effects
2. Create `hvm-difftest` harness.
3. Run one-instruction randomized tests.
4. Run basic-block randomized tests.
5. Run firmware smoke tests on both interpreter and RTL.
6. Add mismatch report format:
   - instruction bytes
   - disassembly
   - pre-state
   - interpreter post-state
   - RTL post-state
   - memory diff
7. Add CI lane for small RTL differential tests.
8. Add nightly lane for longer randomized tests.

Deliverables:

- `hvm-difftest --backend-a interp --backend-b rtl --case isa_random`
- Artifact logs for any mismatch.

Exit criteria:

- Base ISA differential suite passes.
- MMU and interrupt differential suite passes for deterministic cases.

### Phase 8: LLVM ORC JIT Backend

Goal: add a fast backend after correctness is established.

Tasks:

1. Implement JIT block cache.
2. Implement HVM instruction lowering to LLVM IR:
   - arithmetic
   - branches
   - loads/stores
   - pair loads/stores
   - atomics
   - exceptions
   - HVM-L hardware loops as counted host loops
   - prefetch and branch hints as LLVM hints or no-ops
   - HVM-Alloc fast path with slow-path fallback
3. Route MMIO, compact-reference decode checks, and privileged operations through runtime helpers.
4. Implement JIT invalidation:
   - `ICACHE.RNG`
   - writes to executable pages
   - MMU context changes
5. Add lazy/eager compile modes.
6. Add JIT trace events.
7. Add interpreter-vs-JIT differential tests.
8. Add performance benchmarks.

Deliverables:

- `hvm-sim run --cpu-backend jit --machine hvm-desktop ...`

Exit criteria:

- JIT passes all interpreter differential tests for supported instructions.
- JIT can be disabled at runtime.
- JIT never runs for unsupported instructions; it exits to interpreter cleanly.

### Phase 9: Advanced Devices and Robotics Timing

Goal: complete profile-specific devices enough to validate board behavior.

Tasks:

1. Add framebuffer and input.
2. Add user-mode NIC.
3. Add PCIe config-space model.
4. Add server BMC bridge model.
5. Implement CAN-FD message RAM and interrupts.
6. Implement PWM timing model.
7. Implement ADC scripted sample source.
8. Implement QEI/encoder scripted event source.
9. Implement robotics watchdog and safe-state latch.
10. Implement lockstep mismatch injection.
11. Add fault scripts using TOML/JSON.

Deliverables:

- Robotics safety scenario tests.
- Desktop/server boot can enumerate basic PCIe/NVMe/NIC devices.

Exit criteria:

- `hvm-robot` can run deterministic PWM/ADC/CAN-FD tests without deadline drift in deterministic mode.
- `hvm-server` can inject ECC and PCIe error events into firmware/OS-visible registers.

### Phase 10: Snapshots, Replay, and Release Hardening

Goal: make the simulator reliable enough for long-running firmware, OS, and board validation.

Tasks:

1. Add VM snapshot save/load:
   - CPU state
   - RAM state
   - device state
   - block overlay state
   - virtual time
2. Add deterministic event log.
3. Add replay runner.
4. Add trace compaction and Chrome trace export.
5. Add packaging:
   - Windows zip/MSI optional
   - macOS universal or per-arch package
   - Linux tar/deb/rpm optional
6. Add release SBOM and license report.
7. Add documentation:
   - user guide
   - machine profile guide
   - disk image guide
   - RTL backend guide
   - debugging guide

Deliverables:

- Versioned simulator release artifacts.
- Reproducible smoke-test suite.
- License/SBOM report.

Exit criteria:

- All profile support claims in the completeness matrix are either passed, explicitly stubbed, or marked future.
- Cross-platform release build passes on Windows, Linux, and macOS.

### Phase 11: Implementation Order Summary

| Order | Workstream | Why It Comes Here |
| :---: | :--- | :--- |
| 1 | CMake/vcpkg/CI skeleton | Avoids platform debt |
| 2 | Interpreter | Establishes correctness baseline |
| 3 | Memory/MMU/interrupts/minimal devices | Enables booting and firmware tests |
| 4 | Block/HSD/raw/VHDX | Required before realistic boards boot OS images |
| 5 | Machine profiles | Instantiates documented SoCs and boards |
| 6 | Firmware bring-up | Proves board descriptions and boot ABI |
| 7 | Verilated RTL backend | Validates hardware implementation against interpreter |
| 8 | Differential testing | Prevents backend drift |
| 9 | LLVM JIT | Adds speed after semantics stabilize |
| 10 | Advanced devices/robotics | Completes platform-specific behavior |
| 11 | Snapshots/replay/release | Makes simulator useful for long validation cycles |

---

## 19. Simulator Completeness Matrix

This matrix defines what it means for the simulator to be capable of simulating the documented HVM CPUs, SoCs, and boards. Each row maps a documented feature to the simulator support target.

Support levels:

| Level | Meaning |
| :--- | :--- |
| Required | Must be implemented for the simulator to claim support for the profile |
| Functional | Behaviorally correct, not cycle-accurate |
| Timing-Annotated | Functional plus configurable latency/timing model |
| Stub | Exposes registers/events enough for firmware or OS discovery, but not full behavior |
| Optional | Useful but not required for first profile support |
| Future | Out of first production simulator scope |
| Out of Scope | Should not be implemented unless goals change |

### 19.1 CPU and ISA Coverage

| Feature | Mobile `HVM-M1` | Desktop `HVM-D1` | Server `HVM-S1` | Robotics `HVM-R1` | Support Target | Phase |
| :--- | :---: | :---: | :---: | :---: | :--- | :---: |
| Base 64-bit HVM integer ISA | Required | Required | Required | Required | Functional interpreter | 1 |
| Instruction decode from CSV metadata | Required | Required | Required | Required | Generated decode/disasm tables | 0-1 |
| HVM-C compressed instructions | Required | Required | Required | Required | Functional decode and execution | 1 |
| HVM-ARC `RETAIN`/`RELEASE` | Required | Required | Required | Optional for RT cores | Functional atomics through memory subsystem | 1 |
| `ICACHE.RNG` | Required | Required | Required | Required | JIT/decode cache invalidation | 1, 5 |
| HVM-L hardware loops | Required | Optional | Optional | Required on RT cores | Functional counted-loop semantics; timing annotation later | 1, 6 |
| `LD.P` / `ST.P` pair memory ops | Required | Required | Required | Required | Functional with precise exception order | 1 |
| Prefetch and cache hints | Optional | Optional | Required for server perf studies | Optional | No-op legal; timing/perf model later | 1, 5 |
| Branch/code layout hints | Optional | Optional | Optional | Optional | No-op legal; JIT advisory lowering | 5 |
| `MEMZERO.HINT` | Optional | Optional | Optional | Optional | No-op legal; block helper later | 5 |
| HVM-V vector state | Required on big cores | Required | Required | Required on app cores, optional on RT cores | Functional scalar fallback first | 1 |
| HVM-V vector performance lowering | Optional | Required | Required | Optional | LLVM JIT vector lowering | 5 |
| HVM-A doorbell instruction | Stub | Required | Required | Optional | MMIO doorbell semantics | 3 |
| HVM-Alloc `ALLOC.BUMP` | Optional | Optional | Required for runtime-performance studies | Optional on app cores; prohibited in hard RT regions | Functional fast path plus runtime fallback | 5 |
| Compact object references | Optional | Optional | Optional | Optional | Runtime/JIT model with heap-window checks | Future |
| HVM-Cap tagged pointer checks | Optional | Optional | Optional | Optional | Functional if ISA finalized | Future |
| HVM-Prof counters | Optional | Required | Required | Optional | Simulator PMU counters | 5 |
| Power-aware HVM-Prof counters | Required for power studies | Required | Required | Required for safety/power studies | Simulator counters and `hvm-sim perf` output | 5 |
| HVM-NZ null-check loads | Optional | Optional | Optional | Optional | Functional if ISA finalized | Future |
| Deterministic RT subset enforcement | N/A | N/A | N/A | Required | Simulator rejects/profile-flags unbounded hard-RT instructions | 6 |
| Precise exceptions | Required | Required | Required | Required | Functional and deterministic | 1 |
| Debug breakpoints/watchpoints | Required | Required | Required | Required | Functional monitor support | 1 |
| LLVM ORC JIT | Optional | Required for performance | Required for performance | Optional, off by default for lockstep | Functional with interpreter differential tests | 5 |

### 19.2 CPU Topology and Timing Coverage

| Feature | Mobile `HVM-M1` | Desktop `HVM-D1` | Server `HVM-S1` | Robotics `HVM-R1` | Support Target | Phase |
| :--- | :---: | :---: | :---: | :---: | :--- | :---: |
| Big/little topology | Required | N/A | N/A | N/A | Functional scheduling domains | 3 |
| 8-core desktop topology | N/A | Required | N/A | N/A | Functional SMP | 3 |
| 64-128 core server topology | N/A | N/A | Required | N/A | Functional scalable SMP; default may use reduced core count | 3 |
| Dual-socket server model | N/A | N/A | Optional first, required later | N/A | NUMA and inter-socket interrupt/coherency model | 6 |
| RT + app core split | N/A | N/A | N/A | Required | Functional separate scheduling domains | 3 |
| Dual-core lockstep | N/A | N/A | N/A | Required | Deterministic compare and fault latch | 6 |
| DVFS and power states | Required | Optional | Required for power caps | Required for safety/brownout | Timing-annotated/stub depending profile | 6 |
| Cycle-accurate pipelines | Out of Scope | Out of Scope | Out of Scope | Out of Scope | Not required | - |
| Configurable instruction timing | Optional | Optional | Optional | Required for RT tests | Timing-annotated | 6 |

### 19.3 Memory, MMU, and Coherency Coverage

| Feature | Mobile `HVM-M1` | Desktop `HVM-D1` | Server `HVM-S1` | Robotics `HVM-R1` | Support Target | Phase |
| :--- | :---: | :---: | :---: | :---: | :--- | :---: |
| Physical RAM/ROM map | Required | Required | Required | Required | Functional | 1 |
| MMIO region dispatch | Required | Required | Required | Required | Functional | 1 |
| HVM-39 page tables | Required | Required | Required | Required | Functional | 1 |
| TLB model | Required | Required | Required | Required | Functional with deterministic invalidation | 1 |
| Cache hierarchy | Stub | Stub | Stub | Stub | Expose cache sizes; no data-cache timing initially | 3 |
| MOESI coherency | Stub | Functional enough for atomics | Required functional model | Stub for app cores | Functional memory consistency, not cycle-accurate | 6 |
| LPDDR5/LPDDR5X controller | Required | N/A | N/A | Required where populated | Functional/stub training registers | 3 |
| DDR5 UDIMM controller | N/A | Required | N/A | N/A | Functional/stub training registers | 3 |
| DDR5 ECC RDIMM controller | N/A | N/A | Required | N/A | Functional ECC injection and reporting | 6 |
| ECC SRAM | N/A | N/A | Optional | Required | Functional error injection | 6 |
| DMA | Required | Required | Required | Required | Functional through memory subsystem | 3 |
| IOMMU/ATS/PRI | Optional | Stub | Required later | Optional | Stub first, functional later | 6 |

### 19.4 Interrupt, Timer, and Firmware Coverage

| Feature | Mobile `HVM-M1` | Desktop `HVM-D1` | Server `HVM-S1` | Robotics `HVM-R1` | Support Target | Phase |
| :--- | :---: | :---: | :---: | :---: | :--- | :---: |
| HLIC local interrupts | Required | Required | Required | Required | Functional | 1 |
| HPIC platform interrupts | Required | Required | Required | Required | Functional | 1 |
| Per-core timers | Required | Required | Required | Required | Functional | 1 |
| Software IPIs | Required | Required | Required | Required | Functional | 1 |
| Boot ROM image loading | Required | Required | Required | Required | Functional | 4 |
| FSBL loading | Required | Required | Required | Required | Functional | 4 |
| HVM-SFI firmware ABI | Required | Required | Required | Required | Call table and ABI spec required | 4 |
| Direct kernel/initrd boot | Optional | Required | Required | Optional | Functional developer shortcut | 4 |
| Device tree generation | Required | Optional | Optional | Required | Functional | 3-4 |
| ACPI/SMBIOS generation | N/A | Required later | Required later | N/A | Functional for PC/server OS compatibility | 4 |
| Secure boot emulation | Stub | Stub | Stub | Stub | Register and signature-flow stubs first | Future |

### 19.5 Storage and Disk Image Coverage

| Feature | Mobile `HVM-M1` | Desktop `HVM-D1` | Server `HVM-S1` | Robotics `HVM-R1` | Support Target | Phase |
| :--- | :---: | :---: | :---: | :---: | :--- | :---: |
| Raw sparse images | Required | Required | Required | Required | Functional read/write | 2 |
| HSD native sparse images | Required | Required | Required | Required | Functional read/write/check/convert | 2 |
| VHDX images | Optional | Required if QCOW2 rejected | Required if QCOW2 rejected | Optional | Functional read/write | 2 |
| QCOW v1 | Optional | Optional | Optional | Optional | Compatibility only after approval | Future |
| QCOW2 | Optional | Optional | Optional | Optional | Clean-room backend only after approval | Future |
| Backing files/overlays | Required | Required | Required | Required | HSD first, VHDX/QCOW later | 2, 7 |
| Snapshots | Optional | Required | Required | Required for robotics test rewind | External snapshots first | 7 |
| UFS-like block controller | Required | N/A | N/A | Optional | Functional over block backend | 3 |
| NVMe-like block controller | Optional | Required | Required | Optional | Functional over block backend | 3 |
| eMMC-like block controller | Optional | N/A | N/A | Required for control board SKU | Functional over block backend | 3 |
| Hot-plug storage | N/A | Optional | Required later | N/A | Functional later | 6 |

### 19.6 Board Device Coverage

| Device / Subsystem | Mobile `HVM-M1` | Desktop `HVM-D1` | Server `HVM-S1` | Robotics `HVM-R1` | Support Target | Phase |
| :--- | :---: | :---: | :---: | :---: | :--- | :---: |
| UART console | Required | Required | Required | Required | Functional | 1 |
| GPIO | Required | Optional | Optional | Required | Functional register model | 3 |
| SPI | Required | Required for flash/TPM | Required for flash/BMC | Required | Functional register model | 3 |
| I2C/I3C | Required | Optional | Required for sensors/BMC | Required | Functional register model | 3 |
| USB controller | Required | Required | Optional | Optional | Stub first, functional later | 6 |
| Framebuffer/display | Required | Required | BMC display stub | Optional | Functional simple framebuffer | 6 |
| MIPI-DSI | Required | N/A | N/A | Optional | Stub registers + framebuffer bridge | 6 |
| MIPI-CSI camera | Required | N/A | N/A | Required for perception SKU | Stub frame source | 6 |
| PCIe root complex | Optional | Required | Required | Optional | Config-space and BAR model | 3 |
| Network NIC | Optional | Required | Required | Required for TSN/Ethernet SKU | User-mode NIC first | 6 |
| BMC bridge | N/A | N/A | Required | Optional for test rigs | Stub first, functional later | 6 |
| TPM/HSM | Stub | Stub | Stub | Stub | Register-level stub | Future |
| Audio | Optional | Optional | N/A | N/A | Out of first scope | Future |

### 19.7 Robotics-Specific Coverage

| Feature | Required for `HVM-R1` Claim? | Support Target | Phase |
| :--- | :---: | :--- | :---: |
| RT core pair | Yes | Functional CPU topology | 3 |
| Lockstep comparator | Yes | Deterministic compare, mismatch event, fault latch | 6 |
| PWM | Yes | Timing-annotated waveform/event model | 6 |
| ADC | Yes | Scripted analog sample source and conversion timing | 6 |
| QEI/encoder | Yes | Scripted position/velocity event source | 6 |
| CAN-FD | Yes | Functional message buffers and interrupts | 6 |
| Fault input pins | Yes | Immediate safe-state transition model | 6 |
| Watchdog | Yes | Functional timeout and reset/fault policy | 6 |
| Brownout/power-loss | Yes | Event injection and safe-state validation | 6 |
| IEEE 1588/PTP timestamping | Optional first, required later | Timing-annotated Ethernet timestamp model | Future |
| Motor physics model | No | Use external co-simulation later | Future |

### 19.8 Debug, Trace, and Validation Coverage

| Feature | Required? | Support Target | Phase |
| :--- | :---: | :--- | :---: |
| Human monitor | Yes | `info`, `regs`, `step`, `break`, `trace`, `quit` | 1 |
| JSON monitor | Yes | Automation interface | 1 |
| Instruction trace | Yes | Optional per-core trace | 1 |
| MMIO trace | Yes | Device read/write trace | 1 |
| IRQ trace | Yes | Interrupt lifecycle trace | 1 |
| Block trace | Yes | Read/write/flush/discard trace | 2 |
| Chrome trace export | Optional | Performance visualization | 5 |
| GDB remote stub | Optional first, required later | Source-level debug integration | Future |
| Deterministic mode | Yes | Stable event ordering and fixed seeds | 1 |
| Record/replay | Optional first, required later | Event log replay | 7 |
| VM snapshots | Optional first, required later | Save/load machine state | 7 |
| Differential JIT tests | Yes if JIT enabled | Interpreter vs JIT equivalence | 5 |
| Fuzz tests | Yes | Decoder, MMU, block parsers, monitor parser | 2 |

### 19.9 Minimum Support Claims

The simulator should only claim profile support when the following gates pass:

| Claim | Minimum Gate |
| :--- | :--- |
| `hvm-sim supports HVM CPU` | Base ISA, HVM-C, HVM-ARC, HVM-L, HVM-MEM, HVM-39, HLIC, HPIC, exceptions, UART boot smoke test |
| `hvm-sim supports hvm-mobile` | `HVM-M1` topology, LPDDR model, UFS block device, PMIC stubs, framebuffer, MIPI stubs, device tree, firmware boot |
| `hvm-sim supports hvm-desktop` | `HVM-D1` topology, DDR5 model, PCIe root complex, NVMe block, framebuffer/input, firmware boot, direct kernel boot |
| `hvm-sim supports hvm-server` | `HVM-S1` scalable SMP, DDR5 ECC model, PCIe/NVMe, NIC stub, BMC bridge stub, RAS injection basics, firmware boot |
| `hvm-sim supports hvm-robot` | `HVM-R1` RT/app topology, lockstep, PWM, ADC, CAN-FD, watchdog, fault injection, deterministic safety tests |
| `hvm-sim supports JIT` | All implemented JIT instruction lowerings pass differential tests against interpreter |
| `hvm-sim supports HSD` | `hvm-img check`, backing files, overlays, crash-update tests, cross-platform reopen tests pass |
| `hvm-sim supports QCOW2` | Clean-room backend approved, compatibility fixtures pass, malformed-image fuzzing passes |

---

## 20. Open Design Decisions

- Final simulator license: Apache-2.0, MIT, or BSD-3-Clause.
- Whether HVM firmware will use only device tree, or also ACPI for desktop/server.
- Whether `hvm-server` should simulate 128 cores functionally in one process or use a scaled model for default runs.
- Whether QCOW/QCOW2 should be required, optional, or dropped in favor of raw/VHDX/HSD.
- Whether QCOW2 encrypted images are required for product use if QCOW2 remains supported.
- Whether network support should start with user-mode sockets, TAP/TUN, or a simulated-only packet bus.
- Whether the UI is a separate process connected through the JSON monitor or linked into the simulator.
- Whether the simulator should expose a GDB remote stub in v1.

---

## 21. Source References

- QEMU QCOW2 format documentation: https://gitlab.com/qemu-project/qemu/-/blob/master/docs/interop/qcow2.rst
- QEMU project and system-emulation concepts: https://www.qemu.org/docs/master/system/
- Microsoft VHD/VHDX format specifications and Open Specification Promise context: https://learn.microsoft.com/en-us/openspecs/windows_protocols/ms-vhdx/
- libqcow README and license status: https://github.com/libyal/libqcow
- LLVM ORC JIT documentation: https://llvm.org/docs/ORCv2.html
- vcpkg CMake integration: https://learn.microsoft.com/en-us/vcpkg/users/buildsystems/cmake-integration
- LLVM license: https://llvm.org/LICENSE.txt
- zlib license: https://zlib.net/zlib_license.html
- Zstandard project: https://github.com/facebook/zstd
- SDL project: https://github.com/libsdl-org/SDL
- Dear ImGui project: https://github.com/ocornut/imgui
- CLI11 project: https://github.com/CLIUtils/CLI11
- spdlog project: https://github.com/gabime/spdlog
- nlohmann/json project: https://github.com/nlohmann/json
- Catch2 project: https://github.com/catchorg/Catch2
