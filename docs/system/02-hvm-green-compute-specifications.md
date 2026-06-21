# HVM Green Compute Specifications

This document provides the finalized specifications for the HVM Green Compute extensions. All proposal language and suggestions have been removed, retaining only the technical specifications.

## 1. Core ISA Extensions

### 1.1 HVM‑C: 16‑Bit Compressed Opcodes

**Specification**:
```
Base Format (32‑bit):
+-----------+-------+-------+-------+---------------------+
| Opcode[7] | rd[5] | rs1[5]| rs2[5]| Func[10]            |
+-----------+-------+-------+-------+---------------------+

Compressed Format (16‑bit):
+-----------+-------+-------+-------+
| Opcode[4] | rd[4] | rs1[4]| imm[4]|
+-----------+-------+-------+-------+
```
* Reduces bytecode footprint by 30–40%.
* Halves fetch power consumption.
* Fully backwards compatible with existing 32‑bit encodings.

### 1.2 Instruction Fusion

**Fused Patterns** (hardware decoder merges these pairs):
1. **Compare and Branch**
   - `BEQ    r2, r3, label` (replaces `CMPEQ` + `BNE`).
2. **Address Scaling + Load**
   - `LD.D   r1, [r3 + r2 << 3]` (replaces `SHL` + `ADD` + `LD.D`).

**JIT Alignment Rule**: Emit fused instruction pairs consecutively without temporary register reuse.

### 1.3 HVM‑L: Zero‑Overhead Hardware Loops

**Instructions**:
* `LOOP.SET rs_count, imm_backedge` – loads per‑core loop counter and records back‑edge displacement.
* `LOOP.DECBR label` – decrements counter and branches while non‑zero.

### 1.4 HVM‑MEM: Pair Loads/Stores and Prefetch Hints

**Instruction Set**:
| Instruction | Description |
|-------------|-------------|
| `LD.P rd1, rd2, (rs1)` | Load two adjacent words from `rs1`.
| `ST.P rs1, rs2, (rd)` | Store two adjacent words to `rd`.
| `PREFETCH.R rs1, imm` | Advisory read prefetch.
| `PREFETCH.W rs1, imm` | Advisory write‑intent prefetch.
| `PREFETCH.NTA rs1, imm` | Non‑temporal prefetch.
| `MEMZERO.HINT rs_base, rs_size` | Hint for zero‑fill region.

### 1.5 HVM‑V: Vector‑Length Agnostic SIMD

**Configuration**: `vsetvl rd, rs1, rs2` – sets vector length (`vl`) and element type (`vtype`).

**Core Vector Instructions** (selected list):
* `VLD.V`, `VST.V` – unit‑stride load/store.
* `VLDS.V`, `VSTS.V` – strided load/store.
* `VLDX.V`, `VSTX.V` – indexed load/store.
* `VADD.VV`, `VADD.VX` – vector addition.
* `VSUB.VV`, `VSUB.VX` – vector subtraction.
* `VMUL.VV`, `VMUL.VX` – vector multiplication.
* `VDIV.VV`, `VDIV.VX` – vector division.
* `VFMACC.VV`, `VFMACC.VF` – fused multiply‑accumulate.
* `VCOMP.VV`, `VCOMP.VX` – element‑wise comparison.
* `VMERGE.VVM` – mask‑based merge.
* `VREDADD.VS`, `VREDMIN.VS`, `VREDMAX.VS` – reductions.
* `VSLL.VV`, `VSLL.VX`, `VSRL.VV`, `VSRL.VX` – shifts.
* `VAND.VV`, `VOR.VV`, `VXOR.VV` – bitwise logic.

**Context‑Switch Optimisation**: `sstatus.VS` field tracks vector state (Off/Initial/Clean/Dirty) to enable lazy saving.

## 2. Physical Silicon Specification

* **Process Node**: TSMC 4nm N4P.
* **Core Topology**: Modular chiplet design supporting 6‑core mobile clusters up to 128‑core server clusters.
* **Die Area**: ~95 mm² (6‑core mobile) to ~120‑138 mm² (8‑core desktop).
* **Transistor Count**: 10.5 B (mobile) – 15.5 B (desktop) – scalable for server.
* **Cache**: L1 I/D 64 KB per core, L2 512 KB per core, coherent L3 ring mesh (up to 512 GB/s).
* **Clock Domains**: `CLK_CORE` 2.4‑4.2 GHz, `CLK_SYS` 1.6 GHz, `CLK_MEM` 3.2 GHz.
* **Voltage Domains**: Core 0.70‑1.15 V, SRAM 0.90 V, I/O 1.8 V/1.1 V.
* **Thermal**: TjMax 105 °C, TDP 3‑15 W (mobile) 65‑125 W (desktop) 250‑800 W (server).

## 3. PCB & Motherboard Specification

### 3.1 PCB Stackup (10‑Layer)
```
[Layer 1]   Microstrip Signals (DDR5 / PCIe Gen 5)
[Layer 2]   Ground Plane (GND)
[Layer 3]   Stripline Signals (High‑speed)
[Layer 4]   Power Plane (VDD_Core / VDD_IO)
[Layer 5]   Ground Plane (GND)
[Layer 6]   Ground Plane (GND)
[Layer 7]   Power Plane (VDD_SRAM / VDD_1.8V)
[Layer 8]   Stripline Signals (Low‑speed)
[Layer 9]   Ground Plane (GND)
[Layer 10]  Microstrip Signals (Low‑speed / I/O)
```
* Impedance: 50 Ω ±10 % (single‑ended), 85 Ω ±10 % (PCIe Gen 5 diff), 90 Ω ±10 % (USB 4 diff).
* DDR5 routing: 4 mil width, 8 mil spacing, length‑matching ±10 mil, max 2 vias per trace.

### 3.2 Motherboard Reference (Desktop – HVM‑MB v1.0)
* Form Factor: Micro‑ATX (244 mm × 244 mm).
* Socket: LGA‑1700 (HVM‑D1).
* Power: 8+2‑phase VRM (0.75‑1.25 V range).
* Memory: Dual‑channel DDR5 UDIMM, up to 128 GB, ECC optional.
* Expansion: 1 × PCIe Gen 5 x16, 1 × PCIe Gen 4 x4, 2 × USB 4, 4 × USB 3.2 Gen 2, HDMI 2.1, DisplayPort 1.4a, 2.5 GbE LAN, Wi‑Fi 6E, Bluetooth 5.3.
* Firmware: HVM supervisor ROM bootloader, optional U‑Boot or coreboot second‑stage.

## 4. Runtime Extensions (Specification Only)

### 4.1 HVM‑ARC: Hardware‑Assisted Automatic Reference Counting
* **Instructions**:
  * `RETAIN rd, rs1` – atomic increment of ARC header.
  * `RELEASE rd, rs1` – atomic decrement, returns flag if count reaches zero.

### 4.2 ICACHE.RNG – Range Invalidation
* `ICACHE.RNG rs1, rs2` – invalidate cache lines covering the address range `[rs1, rs1+rs2)`.

### 4.3 HVM‑NZ – Null‑Check Folded Load
* `LD.D.NZ rd, rs1, imm15` – loads from `rs1+imm15`; traps on null `rs1`.

These specifications are the authoritative reference for the HVM Green Compute extensions.
