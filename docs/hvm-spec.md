# Hoo Virtual Machine (HVM) Specification

Version: `1.5`

This is the **64-bit HVM core and system profile** required to implement the current Hoo grammar in `src/parsing/Hoo.g4` and to support the documented HVM CPU, SoC, board, firmware, ABI, and simulator work. The base profile remains a physical-hardware-compatible RISC ISA. HVM 1.5 adds profile-gated green-compute and runtime extensions without changing the 64-bit register machine or public 64-bit ABI.

## 1. Scope

This profile is limited to the physical ISA required to support Hoo. All high-level language constructs (objects, arrays, exceptions) are lowered to standard memory operations and function calls to a runtime library.

Anything not required by the mandatory core remains profile-gated. Optional extensions must be discoverable through feature flags and must have software fallbacks unless the platform profile explicitly requires them.

## 2. Execution Model

- 64-bit register machine
- 64-bit public pointer ABI and LP64 C/C++ data model
- byte-addressable little-endian memory
- downward-growing stack
- flat physical memory model (paged/protected via MMU if present)
- 32-bit base instruction words with escape-prefixed extended opcodes for ops >= 0x80
- Extended (>= 0x80) opcodes use an 8-byte encoding: escape byte `0xFE`, ULEB128-encoded opcode,
  zero-padding to offset 4, then the 32-bit base-format payload (R/I/RI/B/J). The payload
  encoding is identical to base-format instructions but the opcode field in the payload is ignored
  (the opcode is taken from the ULEB128 value).

## 3. Register Set

The core profile uses 32 general-purpose 64-bit registers (`r0..r31`):
- `r0`: hardwired zero
- `r1`: first argument and return-value register
- `r2..r3`: argument registers
- `r4`: thread pointer (`tp`)
- `r5..r8`: argument registers
- `r9..r15`: caller-saved temporaries
- `r16..r28`: callee-saved
- `r29`: link register (`lr`, return address)
- `r30`: frame pointer (`fp`)
- `r31`: stack pointer (`sp`)

(See `docs/hvm/hvm_register_set.csv`.)

## 4. Encoding

Base instruction formats:
- `R`: `opcode[7] rd[5] rs1[5] rs2[5] func[10]`
- `I`: `opcode[7] rd[5] rs1[5] imm[15]`
- `RI`: `opcode[7] rd[5] rs1[5] rs2[5] imm[10]`
- `B`: `opcode[7] rs1[5] rs2[5] imm[15]`
- `J`: `opcode[7] rd[5] imm[20]`

Extended opcode encoding:
- opcodes `0x00..0x7F` use the base 32-bit formats above
- opcodes `>= 0x80` use an escape-prefixed encoding (`0xFE` prefix)
- branch/jump immediates are instruction offsets: `pc += sign_extend(imm) * 4`

## 5. Minimal Instruction Set

The normative list is `docs/hvm/hvm_instruction_set.csv`.

### 5.1 Required Families (`hvm64-core-system`)

- Data movement: `NOP`, `MOV`, `MOVZ`, `LUI`, `ADDI`
- Integer arithmetic: `ADD`, `SUB`, `MUL`, `DIV`, `DIVU`, `REM`, `SHL`, `SHR`, `SAR`
- Bitwise/logical: `AND`, `OR`, `XOR`, `NOT`
- Floating-point: `FADD`, `FSUB`, `FMUL`, `FDIV`
- Comparisons: `CMPEQ`, `CMPNE`, `CMPLT`, `CMPLE`, `FCMPEQ`, `FCMPLT`, `FCMPLE`
- Branch/jump: `BEQ`, `BNE`, `BLT`, `BLE`, `JMP`, `JAL`, `JALR`, `RET`
- Memory: `LD.B`, `LD.BU`, `LD.H`, `LD.HU`, `LD.W`, `LD.WU`, `LD.D`, `ST.B`, `ST.H`, `ST.W`, `ST.D`, `LDA`
- Atomic memory: `LR.D`, `SC.D`
- Stack/frame: `PUSH`, `POP`, `ENTER`, `LEAVE`, `ADJSP`, `FRAME`
- Calls/linking: `CALL`, `TAILCALL`
- Hardware/System: `SYSCALL`, `BREAK`

### 5.2 HVM 1.5 Required Green-Compute Core Extensions

HVM 1.5 promotes the following low-complexity extensions into the standard HVM CPU profile for documented mobile, desktop, server, and robotics systems:

- Runtime atomics: `RETAIN`, `RELEASE`
- JIT cache coherency: `ICACHE.RNG`
- Pair memory operations: `LD.P`, `ST.P`

These instructions remain 64-bit operations. They do not change pointer width, register width, stack slot width, or the public ABI.

### 5.3 Optional Profile-Gated Extensions

The following extensions are optional unless required by a specific HVM platform profile:

- HVM-L hardware loops: `LOOP.SET`, `LOOP.DECBR`
- HVM-MEM advisory memory hints: `PREFETCH.R`, `PREFETCH.W`, `PREFETCH.NTA`, `MEMZERO.HINT`
- HVM-V vector operations: `VSETVL`, vector load/store, arithmetic, compare, merge, reduction, shift, bitwise, and `VFIRST.M`
- HVM-A accelerator dispatch: `DOORBELL`
- HVM-Alloc: `ALLOC.BUMP`
- HVM-ObjRef: compact managed object references represented by runtime metadata, not native ABI pointers
- HVM-Prof: `RDPROF`
- HVM-Cap: `CHK.B`
- HVM-NZ: `LD.D.NZ`
- Branch/code layout hints: `BR.HINT`

Advisory instructions may be implemented as no-ops. Runtime-specific instructions must have software fallback unless the platform profile says otherwise.

### 5.4 Explicitly Excluded from Core (Lowered to Software)

The following are **NOT** in the ISA and must be lowered by the compiler:
- `NEW`/`NEWA`: Lowered to `CALL hoo_alloc`.
- `LDF`/`STF`: Lowered to `ADDI` + `LD.D`/`ST.D`.
- `LDELEM`/`STELEM`: Lowered to pointer arithmetic + `LD.D`/`ST.D`.
- `TRY`/`CATCH`/`THROW`: Lowered to control flow + `CALL hoo_push_handler`.

### 5.5 Privileged Instructions (System Profile)

The following are **only** required for the system-level profile (e.g., running a kernel). Minimal embedded HVM64 implementations may omit them when they do not run a protected OS:

- Supervisor traps: `ECALL`, `TRAPRET`
- System register access: `CSRRW`
- TLB management: `SFENCE.VMA`

## 6. Mapping to Current Grammar

- `newExpression` -> `CALL hoo_alloc` + `CALL` constructor.
- member/index access -> explicit pointer arithmetic + `LD.D`/`ST.D`.
- `try/catch/finally` -> `CALL hoo_push_handler` + conditional control flow.

## 7. System Call (SYSCALL) Interface

`SYSCALL` dispatches to internal runtime services via the immediate field `imm15`:

| imm15 | Name           | Operation                              | Reads Register | Writes Register |
|-------|----------------|----------------------------------------|----------------|-----------------|
| 1     | `kSysAlloc`    | `rd = hoo_alloc(r2, r3)`              | r2 (size), r3 (typeId) | rd |
| 2     | `kSysRetain`   | `rd = hoo_retain(r2)`                 | r2 (object)   | rd |
| 3     | `kSysRelease`  | `rd = hoo_release(r2)`                | r2 (object)   | rd |
| 4     | `kSysRefcount` | `rd = hoo_refcount(r2)`               | r2 (object)   | rd |
| 5     | `kSysTypeId`   | `rd = hoo_typeid(r2)`                 | r2 (object)   | rd |
| 6     | `kSysException`| `rd = hoo_exception_runtime(0)`       | —             | rd |
| 7     | `kSysPushHandler`   | `rd = hoo_push_handler(r2)`      | r2 (handler PC) | rd |
| 8     | `kSysPopHandler`    | `rd = hoo_pop_handler()`          | —             | rd |
| 9     | `kSysThrowToHandler`| `rd = hoo_throw_handler(r2)`     | r2 (exception) | rd |
| 10    | `kSysRethrowToHandler`| `rd = hoo_rethrow_handler()`   | —             | rd |
| 11    | `kSysStringData`| `rd = hoo_string_data(r2)`            | r2 (string)   | rd |
| 12    | `kSysThreadCreate`   | `rd = thread_create(r2, r3)`     | r2 (entry), r3 (arg) | rd (TID) |
| 13    | `kSysThreadExit`     | `thread_exit(r2)`                 | r2 (retval)   | — |
| 14    | `kSysFutex`          | `rd = futex(r2, r3, r4)`          | r2 (uaddr), r3 (op), r4 (val) | rd |
| 15    | `kSysGetTid`         | `rd = get_tid()`                   | —             | rd |
| 16    | `kSysOpen`           | `rd = open(r2, r3, r4)`            | r2 (path), r3 (flags), r4 (mode) | rd (fd) |
| 17    | `kSysRead`           | `rd = read(r2, r3, r4)`            | r2 (fd), r3 (buf), r4 (count) | rd (bytes) |
| 18    | `kSysWrite`          | `rd = write(r2, r3, r4)`           | r2 (fd), r3 (buf), r4 (count) | rd (bytes) |
| 19    | `kSysClose`          | `rd = close(r2)`                   | r2 (fd)       | rd |
| 20    | `kSysLseek`          | `rd = lseek(r2, r3, r4)`           | r2 (fd), r3 (offset), r4 (whence) | rd (pos) |
| 21    | `kSysFstat`          | `rd = fstat(r2, r3)`               | r2 (fd), r3 (buf) | rd |
| 22    | `kSysClockGetTime`   | `rd = clock_gettime(r2, r3)`       | r2 (clk_id), r3 (ts_ptr) | rd |
| 23    | `kSysGetRandom`      | `rd = getrandom(r2, r3)`           | r2 (buf), r3 (len) | rd (bytes) |

Arguments are passed in registers `r2`, `r3`, and `r4` (for three-argument calls); the result is written to `rd`. Syscalls 1–11 are runtime-internal services. Syscalls 12–23 are platform OS services (threading, file I/O, clock); their presence depends on the host environment.

## 8. Notes

- This spec is a **pure hardware profile**, suitable for physical CPU design.
- The HVM backend now performs aggressive lowering to maintain this purity.
- HVM 1.5 remains a **64-bit architecture**. Compact object references are an optional managed-runtime representation and are not native pointers at C/C++ ABI boundaries.
- **RET implementation note**: The architectural semantics of `RET` are `pc = r29` (branch to link register). In the interpreter and JIT backends, `RET` is implemented via native C++ function return (`return r1`); this is equivalent because `CALL` stores the return address (`pc+4`) in `r29` before transferring control via a C++ function call. A physical hardware implementation must execute `pc = r29` directly.
- **JAL / CALL redundancy**: `JAL` (base32, 16-bit offset) and `CALL` (escape32, 20-bit offset) are semantically identical — both set `rd = pc+4; pc += offset`. `CALL` provides a larger reachable range; `JAL` saves code space when the offset fits in 16 bits.
- **JMP / TAILCALL redundancy**: `JMP` (base32, 16-bit offset) and `TAILCALL` (escape32, 20-bit offset) are semantically identical — both perform `pc += offset` without saving a return address. `TAILCALL` provides a larger range; `JMP` saves code space when the offset fits.

## 9. Privileged Architecture (System Profile)

This section defines the system-level extensions required to run a general-purpose OS (e.g., Linux). These extensions are orthogonal to the core profile — an implementation may omit them for embedded/RTOS use.

### 9.1 Privilege Levels

Two privilege modes:
- **S-mode** (Supervisor, 1): kernel, can execute all instructions and access all CSRs.
- **U-mode** (User, 0): applications, restricted to non-privileged instructions. Traps on attempted privileged operation.

The current privilege level is stored in `sstatus.SPP`. Hardware resets into S-mode.

### 9.2 Control and Status Registers (CSRs)

CSRs are addressed by a 12-bit immediate field in the `CSRRW` instruction.

| Address | Name       | Description                                                        |
|---------|------------|--------------------------------------------------------------------|
| 0x000   | `sstatus`  | Supervisor status (SPP, SIE, SPIE, UPIE)                           |
| 0x001   | `stvec`    | Trap handler PC (4-byte aligned, mode bits in low 2 bits)          |
| 0x002   | `sepc`     | Exception program counter (address of trapping instruction)        |
| 0x003   | `scause`   | Exception cause (bit 63 = interrupt flag, low bits = cause code)   |
| 0x004   | `stval`    | Trap value (faulting address for page faults)                     |
| 0x005   | `satp`     | Supervisor address translation (mode + ASID + page-table PPN)      |
| 0x006   | `stime`    | Cycle counter (read-only, 64-bit, increments every cycle)          |
| 0x007   | `stimecmp` | Timer compare value; when `stime >= stimecmp`, a timer interrupt fires |

**`sstatus` field layout** (64-bit):
- Bit 0: `SIE`   — Supervisor Interrupt Enable (0 = masked, 1 = enabled)
- Bit 1: `SPIE`  — Previous SIE value (saved/restored on trap/return)
- Bit 8: `SPP`   — Previous privilege mode (0 = U, 1 = S)
- All other bits reserved (read as 0, ignore writes).

**`scause` encoding**:
- Bit 63: interrupt flag (1 = interrupt, 0 = exception)
- Bits 62–0: cause code

| scause      | Description                          |
|-------------|--------------------------------------|
| 0           | Instruction page fault               |
| 1           | Load page fault                      |
| 2           | Store/AMO page fault                 |
| 8           | Environment call from U-mode (ECALL) |
| 9           | Illegal instruction                  |
| 10          | Breakpoint (BREAK)                   |
| 0x8000_0000_0000_0000 | Supervisor timer interrupt  |

**`stvec` encoding**:
- Bits 1–0: mode (0 = direct, all traps jump to BASE)
- Bits 63–2: BASE (trap handler PC, must be 4-byte aligned)

**`satp` encoding** (64-bit):
- Bits 63–60: MODE (0 = Bare, no translation; 8 = HVM-39)
- Bits 59–44: ASID (16-bit address-space identifier)
- Bits 43–0: PPN (physical page number of root page table, shifted right by 12)

### 9.3 Trap and Interrupt Handling

**Trap entry** (hardware on ECALL, BREAK, page fault, or interrupt):
1. `sepc` = PC of trapping instruction (for interrupts: PC of interrupted instruction)
2. `scause` = cause code (bit 63 set for interrupts)
3. `stval` = fault address (for page faults), 0 otherwise
4. `sstatus.SPIE` = `sstatus.SIE`; `sstatus.SIE` = 0 (disable interrupts)
5. `sstatus.SPP` = current privilege level (0 for U-mode traps)
6. Switch to S-mode
7. If `stvec.mode == 0` (direct): `PC = stvec.BASE`
8. If `stvec.mode == 1` (vectored): `PC = stvec.BASE + cause * 4` (exceptions) or `stvec.BASE + (interrupt_cause + 16) * 4` (interrupts)

**Trap return** (`TRAPRET`):
1. `SIE` = `sstatus.SPIE`
2. Privilege = `sstatus.SPP`
3. `PC` = `sepc`

Interrupts are taken when `sstatus.SIE == 1` and `sstatus.SPP == 0` (U-mode). In S-mode, interrupts are taken when `sstatus.SIE == 1`.

### 9.4 Timer

`CSRRW` with CSR address 0x006 reads `stime` (read-only, any write is ignored). `CSRRW` with CSR address 0x007 reads/writes `stimecmp`.

When `stime >= stimecmp`, a timer interrupt (scause = 0x8000_0000_0000_0000) is pending. It fires when interrupts are enabled and not masked.

### 9.5 Supervisor Address Translation (HVM-39)

HVM-39 provides a 39-bit virtual address space with 4 KiB pages using a 3-level radix tree.

**Address format:**
- Virtual address `VA[38:0]` is translated; `VA[63:39]` must equal `VA[38]` (canonical sign-extension).
- A 39-bit VA is split: `VPN[2]` (bits 38–30), `VPN[1]` (bits 29–21), `VPN[0]` (bits 20–12), `offset` (bits 11–0).

**Page table entry** (8 bytes, 64-bit):
- Bit 0: `V`    — Valid
- Bit 1: `R`    — Readable
- Bit 2: `W`    — Writable
- Bit 3: `X`    — Executable
- Bit 4: `U`    — User (accessible from U-mode)
- Bit 5: `G`    — Global (not flushed on ASID switch)
- Bit 6: `A`    — Accessed (set by hardware)
- Bit 7: `D`    — Dirty (set by hardware on write)
- Bits 9–8: `RSW` — Reserved for supervisor software
- Bits 53–10: `PPN` — Physical page number (44 bits, shifted right by 12)
- Bits 63–54: reserved

**Translation walk:**
1. If `satp.MODE == 0` (Bare), VA is used directly as PA.
2. Root = `satp.PPN * 4096` (physical address of root page table).
3. For each level `i` from 2 down to 0:
   a. PTE address = root + `VPN[i] * 8`
   b. Load PTE. If `PTE.V == 0`, raise page fault.
   c. If `PTE.R == 0` and `PTE.X == 0`, go to next level (non-leaf PTE).
   d. Otherwise (leaf PTE): check permissions. If violation, raise page fault.
   e. PA = `PTE.PPN * 4096 + offset`.

A non-leaf PTE that has `A` and all of `R,W,X` clear indicates a pointer to the next-level page table. Leaf PTEs must have at least one of `R,W,X` set.

**Page fault causes:**
- Instruction fetch: scause=0; checks X, U
- Load: scause=1; checks R, U
- Store: scause=2; checks W, U

`stval` is set to the faulting virtual address.

### 9.6 Atomic Memory Operations

`LR.D` (load-reserve) and `SC.D` (store-conditional) provide the primitives for implementing atomics and spinlocks.

**Reservation set:**
- `LR.D` creates a reservation on the cache line containing the loaded address.
- `SC.D` succeeds only if the reservation is still valid (no intervening store to the same cache line by any hart).
- The reservation is invalidated by:
  - Any store to the reserved address (by any hart)
  - Another `LR.D` by the same hart (new reservation replaces old)
  - Context switch or trap
- `SC.D` writes `0` to `rd` on success, a nonzero value on failure.
- ACPI-style spinlock: `loop: LR.D rd, (rs1); SC.D rd, rs1, rs2; bnez rd, loop`

### 9.7 CSR Access

`CSRRW rd, rs, imm15`: Atomically reads CSR at address `imm15` into `rd`, then writes the value of `rs` to the CSR. If `rs = r0`, the write is suppressed (behaves as a read-only access).

Attempted write to a read-only CSR (e.g., `stime`) is ignored. Access to an undefined CSR address traps as illegal instruction.

### 9.8 Memory Model

The HVM system profile uses a **RVWMO-compatible** memory model (RISC-V Weak Memory Ordering):
- Loads may be reordered with loads; stores may be reordered with stores.
- `LR.D`/`SC.D` provide acquire-release semantics sufficient for C11 atomics.
- `SFENCE.VMA` ensures all preceding page-table writes are visible to subsequent address translation (TLB flush).
