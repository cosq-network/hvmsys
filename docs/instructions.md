# HVM Core Instruction Reference

Version: `1.5`
Profile: `hvm64-core-system` (Hardware Ready)
Normative sources:
- `docs/hvm/hvm_instruction_set.csv`
- `docs/hvm/hvm-spec.md`

This reference defines a **64-bit hardware-ready ISA**. All high-level VM constructs are handled via software lowering, standard library calls, or profile-gated HVM 1.5 runtime acceleration instructions with software fallback.

## 1. Scope

This reference defines the physical instructions supported by the HVM core and optional HVM 1.5 system profiles. It is sufficient to support the Hoo language through aggressive compiler-level lowering while preserving a 64-bit register and pointer ABI.

## 2. Register Convention Summary

- `r0`: hardwired zero
- `r1`: first argument and return-value register
- `r2..r3`: argument registers
- `r4`: thread pointer (`tp`)
- `r5..r8`: argument registers
- `r9..r15`: caller-saved temporaries
- `r16..r28`: callee-saved
- `r29`: link register (`RET` target)
- `r30`: frame pointer
- `r31`: stack pointer

## 3. Instruction Formats

- `R`: `opcode[7] rd[5] rs1[5] rs2[5] func[10]`
- `I`: `opcode[7] rd[5] rs1[5] imm[15]`
- `RI`: `opcode[7] rd[5] rs1[5] rs2[5] imm[10]`
- `B`: `opcode[7] rs1[5] rs2[5] imm[15]`
- `J`: `opcode[7] rd[5] imm[20]`

Opcode-space note:
- `0x00..0x7F` use the base 32-bit encodings directly.
- Opcodes `>= 0x80` use an escape-prefixed extended encoding (`0xFE` prefix).
- In `docs/hvm/hvm_instruction_set.csv`, `Opcode` is the logical opcode value and `Encoding` records whether the row is `base32` or `escape32`.
- Tooling must use `Encoding` to derive the emitted bytes; do not infer the wire format from `Opcode` alone.

Extended opcode wire format (always 8 bytes):
```
byte 0:      0xFE              (escape prefix)
byte 1..:    ULEB128 opcode    (variable bytes until high bit clear)
bytes..3:    0x00              (zero-padding to reach byte 4)
bytes 4..7:  32-bit payload    (same encoding as the base format: R/I/RI/B/J)
```
The payload's opcode field (bits 31:25) is ignored; the opcode is taken from the ULEB128 value.
Tooling must encode and decode instructions using this 8-byte layout for any opcode >= 0x80.

## 4. Core Instruction Set

### 4.1 Data movement
- `NOP` `MOV` `MOVZ` `LUI` `ADDI`

### 4.2 Integer arithmetic and shifts
- `ADD` `SUB` `MUL` `DIV` `DIVU` `REM`
- `SHL` `SHR` `SAR`

### 4.3 Bitwise/logical
- `AND` `OR` `XOR` `NOT`

### 4.4 Floating point
- `FADD` `FSUB` `FMUL` `FDIV`

### 4.5 Comparisons
- Integer: `CMPEQ` `CMPNE` `CMPLT` `CMPLE`
- Float: `FCMPEQ` `FCMPLT` `FCMPLE`

### 4.6 Branch/jump
- `BEQ` `BNE` `BLT` `BLE`
- `JMP` `JAL` `JALR` `RET`

### 4.7 Memory (Standard Load/Store)
- Loads: `LD.B` `LD.BU` `LD.H` `LD.HU` `LD.W` `LD.WU` `LD.D`
- Stores: `ST.B` `ST.H` `ST.W` `ST.D`
- Pair operations: `LD.P` `ST.P`
- Address: `LDA`

### 4.8 Atomic memory
- `LR.D` `SC.D`: Load-reserve / store-conditional for atomic synchronisation.

### 4.9 Stack/frame
- `PUSH` `POP` `ENTER` `LEAVE` `ADJSP` `FRAME`

### 4.10 Calls/linking
- `CALL` `TAILCALL` (J-format, 20-bit relative offset)

### 4.11 Hardware/System
- `SYSCALL`: Trigger a system call to the runtime.
- `BREAK`: Trap to debugger.

**SYSCALL calling convention**: The immediate field selects the service; arguments are
passed in `r2` (and `r3` for two-argument calls, `r4` for three-argument calls).
The result is written to `rd`. See `docs/hvm/hvm-spec.md` §7 for the full
syscall number table.

### 4.12 System/Trap (system profile; privileged)
- `ECALL`: Trap to supervisor mode (U-mode only; illegal in S-mode).
- `TRAPRET`: Return from supervisor trap (S-mode only; illegal in U-mode).
- `CSRRW`: Atomic read-write of CSR (S-mode only; traps in U-mode).
- `SFENCE.VMA`: TLB flush after page-table modification (S-mode only).

### 4.13 HVM 1.5 runtime and green-compute extensions
- `RETAIN` `RELEASE`: Non-trapping reference-count update helpers for managed Hoo objects.
- `ICACHE.RNG`: Invalidate instruction/JIT cache state for an address range after code generation.
- `LOOP.SET` `LOOP.DECBR`: Optional hardware-loop support for low-power counted loops.
- `PREFETCH.R` `PREFETCH.W` `PREFETCH.NTA` `MEMZERO.HINT`: Advisory memory-traffic hints. Legal implementations may ignore them.
- `ALLOC.BUMP`: Optional thread-local allocation-buffer fast path; zero return means fall back to runtime allocator.
- `RDPROF`: Read HVM profiling/performance counters, subject to privilege and policy.
- `CHK.B`: Optional bounds/tag check for capability-style memory safety.
- `LD.D.NZ`: Optional null-checking 64-bit load.
- `BR.HINT`: Optional branch/code-layout hint.
- `DOORBELL`: Optional accelerator doorbell dispatch for HVM-A.

### 4.14 HVM-V vector extension
- Configuration: `VSETVL`
- Memory: `VLD.V` `VST.V` `VLDS.V` `VSTS.V` `VLDX.V` `VSTX.V`
- Arithmetic: `VADD.VV` `VADD.VX` `VSUB.VV` `VSUB.VX` `VMUL.VV` `VMUL.VX` `VDIV.VV` `VDIV.VX` `VFMACC.VV` `VFMACC.VF`
- Compare/mask: `VCOMP.VV` `VCOMP.VX` `VMERGE.VVM` `VFIRST.M`
- Reductions: `VREDADD.VS` `VREDMIN.VS` `VREDMAX.VS`
- Bit/shift: `VSLL.VV` `VSLL.VX` `VSRL.VV` `VSRL.VX` `VAND.VV` `VOR.VV` `VXOR.VV`

Vector support is profile-gated. Implementations that expose HVM-V must save/restore vector state according to the ABI profile and feature flags.

## 5. Lowering Rules (Software Implemented)

Operations removed from the ISA and now lowered to the above set:

- **Objects**: `NEW` -> `CALL hoo_alloc`.
- **Arrays**: `NEWA` -> `CALL hoo_alloc`.
- **Field/Element Access**: `LDF`/`STF`/`LDELEM` -> Explicit pointer arithmetic + `LD.D`/`ST.D`.
- **Exceptions**: `TRY`/`THROW` -> `CALL hoo_push_handler` + control flow.

## 6. Canonical Opcode Table

The canonical machine-readable definition is `docs/hvm/hvm_instruction_set.csv`.
That CSV is normative.

**Redundancy notes**:
- `JAL` (base32, 16-bit offset) and `CALL` (escape32, 20-bit offset) are semantically identical. `CALL` provides a larger reachable range; `JAL` saves code space when the offset fits in 16 bits.
- `JMP` (base32, 16-bit offset) and `TAILCALL` (escape32, 20-bit offset) are semantically identical. `TAILCALL` provides a larger range; `JMP` saves code space when the offset fits.

Keep this document synchronized with that CSV and `docs/hvm/hvm-spec.md`.
