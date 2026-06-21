# HVM System Memory Map and ABI

This chapter defines the operating-system-facing memory map and ABI contract for the HVM ecosystem. It is intended for kernel developers, firmware developers, simulator implementers, board designers, and toolchain maintainers.

The goal is to make HVM systems bootable and portable across the documented profiles:

- `HVM-M1` mobile SoC
- `HVM-D1` desktop board
- `HVM-S1` server board
- `HVM-R1` robotics controller

This chapter should be treated as a platform ABI draft. Exact bit encodings and final register numbers must be synchronized with `docs/hvm/hvm_register_set.csv`, `docs/hvm/hvm_instruction_set.csv`, and the simulator implementation.

---

## 1. ABI Goals

The HVM system ABI must provide:

- a stable reset and boot handoff convention
- a stable C/C++ calling convention
- a stable supervisor/kernel entry ABI
- a stable interrupt and exception frame layout
- a stable device discovery mechanism
- a stable physical memory map contract
- a stable virtual memory model for OS kernels
- clear differences between mobile, desktop, server, and robotics boards
- enough compatibility for U-Boot, coreboot, EDK II payloads, Linux-like kernels, RTOS kernels, and the HVM simulator

Non-goals for the first ABI version:

- binary compatibility with x64, AArch64, or RISC-V
- POSIX syscall ABI definition
- hypervisor ABI
- final secure monitor ABI
- cycle-accurate memory or cache timing

---

## 2. Architectural Register ABI

### 2.1 General-Purpose Registers

HVM uses 32 general-purpose 64-bit registers:

| Register | ABI Name | Role | Preserved Across Calls |
| :---: | :--- | :--- | :---: |
| `r0` | `zero` | Constant zero; writes are ignored | N/A |
| `r1` | `a0` / `ret` | Argument 0 and primary return value | No |
| `r2` | `a1` | Argument 1 | No |
| `r3` | `a2` | Argument 2 | No |
| `r4` | `tp` | Thread pointer / TLS base | Yes |
| `r5` | `a3` | Argument 3 | No |
| `r6` | `a4` | Argument 4 | No |
| `r7` | `a5` | Argument 5 | No |
| `r8` | `a6` | Argument 6 / service selector where used | No |
| `r9` | `t0` | Temporary 0 | No |
| `r10` | `t1` | Temporary 1 | No |
| `r11` | `t2` | Temporary 2 | No |
| `r12` | `t3` | Temporary 3 | No |
| `r13` | `t4` | Temporary 4 | No |
| `r14` | `t5` | Temporary 5 | No |
| `r15` | `t6` | Temporary 6 | No |
| `r16` | `s0` | Saved register 0 | Yes |
| `r17` | `s1` | Saved register 1 | Yes |
| `r18` | `s2` | Saved register 2 | Yes |
| `r19` | `s3` | Saved register 3 | Yes |
| `r20` | `s4` | Saved register 4 | Yes |
| `r21` | `s5` | Saved register 5 | Yes |
| `r22` | `s6` | Saved register 6 | Yes |
| `r23` | `s7` | Saved register 7 | Yes |
| `r24` | `s8` | Saved register 8 | Yes |
| `r25` | `s9` | Saved register 9 | Yes |
| `r26` | `s10` | Saved register 10 | Yes |
| `r27` | `s11` | Saved register 11 | Yes |
| `r28` | `s12` | Saved register 12 | Yes |
| `r29` | `lr` | Link register / return address for `RET` | No |
| `r30` | `fp` | Frame pointer | Yes |
| `r31` | `sp` | Stack pointer | Yes |

Notes:

- `r0` is always zero.
- `sp` must remain 16-byte aligned at public ABI call boundaries.
- `fp` is optional in optimized code but must be valid when frame pointers are enabled.
- `tp` points at the active thread-local storage control block.
- `lr` holds the return address used by `RET`.
- User-space ABI code may use caller-saved temporaries `t0-t6`; trap entry code must save any user-visible register it clobbers.

### 2.2 Vector Registers

HVM-V defines 16 vector registers:

| Register | ABI Role |
| :---: | :--- |
| `v0` | Mask / temporary / vector return value |
| `v1-v7` | Caller-saved vector temporaries |
| `v8-v15` | Callee-saved vector registers where the ABI profile enables preserved vector state |

Vector control state:

- `vl`: active vector length
- `vtype`: vector element width/grouping/type metadata
- `sstatus.VS`-like HVM vector state field: Off, Initial, Clean, Dirty

Recommended ABI rule:

- User-space functions may clobber `v0-v7`.
- `v8-v15` are preserved only in ABI profiles that explicitly enable callee-saved vector state.
- OS kernels should lazily save vector state. If vector state is not Dirty, the kernel may skip vector register save/restore.
- Signal/trap frames must record vector state only when Dirty or when explicitly requested by debug APIs.

---

## 3. C/C++ Calling Convention

### 3.1 Integer and Pointer Arguments

- First seven integer/pointer arguments use `a0-a6` (`r1-r3`, then `r5-r8`). `r4` is reserved for `tp` and is not a normal public-call argument register.
- Additional arguments are passed on the stack.
- Integer return values use `a0` and `a1` for two-register returns.
- Pointers are 64-bit.
- `size_t`, `uintptr_t`, and `ptrdiff_t` are 64-bit.

### 3.2 Floating-Point and Vector Arguments

Until a dedicated scalar floating-point ABI is finalized:

- scalar floating-point values may be passed in integer registers or stack slots by the soft-float ABI profile
- vector values are passed by reference unless the vector ABI profile is enabled
- HVM-V optimized libraries may use internal vector calling conventions, but public ABI boundaries should remain stable

Future profiles may define:

- hard-float scalar ABI
- vector argument ABI
- homogeneous aggregate passing rules

### 3.3 Stack Layout

Stack grows downward.

At function entry:

```text
high addresses
  caller stack arguments
  return spill area / optional red zone policy
  saved return address
  saved frame pointer
  callee local variables
low addresses
```

Rules:

- `sp` is 16-byte aligned at public call boundaries.
- No red zone is guaranteed in kernel mode.
- User-space red zone is disabled by default for signal/interrupt safety.
- Stack probing is required for large allocations if guard pages are enabled.

### 3.4 Aggregate Return

- Small integer aggregates up to 128 bits may return in `a0/a1`.
- Larger aggregates are returned via hidden sret pointer in `a0`; user arguments shift right by one register.
- ABI-compatible compilers must agree on structure layout, padding, and alignment.

### 3.5 Endianness and Data Model

Default HVM ABI:

- little-endian
- LP64 data model
- `char`: 8 bits
- `short`: 16 bits
- `int`: 32 bits
- `long`: 64 bits
- `long long`: 64 bits
- pointer: 64 bits
- natural alignment by type size up to 16 bytes

---

## 4. Privilege and Execution Modes

The first HVM OS ABI uses three execution modes:

| Mode | Purpose | Typical Software |
| :--- | :--- | :--- |
| Machine/Firmware | Reset, platform bring-up, secure lifecycle, HVM-SFI provider | Boot ROM, FSBL, firmware |
| Supervisor/Kernel | OS kernel, drivers, memory management, scheduler | Linux-like kernel, RTOS kernel |
| User | Applications and runtime code | Hoo programs, native binaries |

Optional future modes:

- Hypervisor mode
- Secure monitor mode
- Safety island mode for `HVM-R1`

The OS-visible ABI must not assume hypervisor mode exists until the virtualization profile is finalized.

---

## 5. Boot Handoff ABI

### 5.1 Reset State

At reset:

- CPU starts in Machine/Firmware mode.
- MMU is disabled.
- Interrupts are disabled.
- `pc` points at the reset vector in Boot ROM.
- `r0` is zero.
- Other general registers are undefined unless firmware documents otherwise.
- The Boot ROM initializes enough SRAM or scratch memory to load FSBL.

### 5.2 Firmware to Kernel Handoff

Recommended handoff registers:

| Register | Value |
| :---: | :--- |
| `a0` (`r1`) | boot hart/core ID |
| `a1` (`r2`) | physical pointer to device tree blob or HVM boot info table |
| `a2` (`r3`) | physical pointer to HVM-SFI service table |
| `a3` (`r5`) | boot flags |
| `a4` (`r6`) | initrd physical base, or 0 |
| `a5` (`r7`) | initrd size, or 0 |
| `sp` (`r31`) | temporary boot stack |
| `pc` | kernel entry point |

Boot flags:

| Bit | Meaning |
| :---: | :--- |
| 0 | Device tree pointer is valid |
| 1 | HVM boot info table pointer is valid |
| 2 | Initrd is present |
| 3 | Firmware has enabled caches |
| 4 | Firmware has enabled MMU for handoff |
| 5 | Secure boot verified |
| 6 | Booted from simulator |

Default recommendation:

- Handoff with MMU disabled unless the OS profile explicitly requires virtual handoff.
- Caches may be enabled if firmware flushes and documents cache state.
- Device tree should be the first supported discovery method.
- ACPI/SMBIOS can be added for desktop/server OS compatibility.

### 5.3 HVM Boot Info Table

If not using device tree, firmware may pass a packed HVM boot info table:

```c
struct HvmBootInfo {
  uint32_t magic;                 // "HVMB"
  uint16_t major_version;
  uint16_t minor_version;
  uint32_t header_size;
  uint32_t total_size;
  uint64_t boot_core_id;
  uint64_t sfi_table_pa;
  uint64_t memory_map_pa;
  uint64_t memory_map_entries;
  uint64_t mmio_map_pa;
  uint64_t mmio_map_entries;
  uint64_t initrd_base;
  uint64_t initrd_size;
  uint64_t command_line_pa;
  uint64_t command_line_size;
  uint64_t reserved[8];
};
```

---

## 6. HVM-SFI: Supervisor Firmware Interface

HVM-SFI is the HVM-native firmware ABI for OS services that must cross from supervisor/kernel code into firmware or machine-owned platform code.

### 6.1 Service Calling Convention

Recommended convention:

| Register | Meaning |
| :---: | :--- |
| `a6` (`r8`) | HVM-SFI function ID |
| `a0-a5` (`r1-r3`, `r5-r7`) | arguments |
| `a0` | return value / status |
| `a1` | optional secondary return value |

Errors are negative signed 64-bit values:

| Error | Meaning |
| :--- | :--- |
| `0` | success |
| `-1` | not supported |
| `-2` | invalid parameter |
| `-3` | denied |
| `-4` | busy |
| `-5` | hardware failure |

### 6.2 Required HVM-SFI Calls

| Function ID | Name | Purpose |
| :---: | :--- | :--- |
| `0x0000` | `SFI_GET_SPEC_VERSION` | Return HVM-SFI major/minor |
| `0x0001` | `SFI_GET_IMPL_ID` | Return firmware implementation ID |
| `0x0002` | `SFI_CONSOLE_PUTCHAR` | Early debug console |
| `0x0003` | `SFI_CONSOLE_GETCHAR` | Early debug input |
| `0x0010` | `SFI_TIMER_SET` | Set per-core timer deadline |
| `0x0011` | `SFI_TIMER_CLEAR` | Clear per-core timer |
| `0x0020` | `SFI_IPI_SEND` | Send IPI to one or more cores |
| `0x0030` | `SFI_SYSTEM_RESET` | Reset system |
| `0x0031` | `SFI_SYSTEM_SHUTDOWN` | Power off or halt |
| `0x0040` | `SFI_FEATURE_PROBE` | Probe firmware/platform feature |
| `0x0050` | `SFI_RTC_GET` | Get wall-clock time if available |
| `0x0060` | `SFI_PERF_COUNTER_READ` | Read platform counter |

Profile-specific optional calls:

| Function ID Range | Profile | Purpose |
| :--- | :--- | :--- |
| `0x1000-0x10FF` | Mobile | PMIC, battery, thermal hints |
| `0x2000-0x20FF` | Desktop | firmware setup, boot order, wake policy |
| `0x3000-0x30FF` | Server | BMC mailbox, power caps, RAS injection |
| `0x4000-0x40FF` | Robotics | safety state, watchdog, fault latch |

---

## 7. Physical Address Map

### 7.1 Common Reference Map

The following map is the simulator and reference-board default. Real boards may move regions, but firmware must describe final layout through device tree or the HVM boot info table.

| Physical Range | Size | Region | Notes |
| :--- | :---: | :--- | :--- |
| `0x0000_0000` - `0x0000_FFFF` | 64 KiB | Reset ROM alias | Optional low alias for reset/debug |
| `0x0001_0000` - `0x000F_FFFF` | 960 KiB | Reserved | Trap vectors, firmware scratch, legacy guard |
| `0x0010_0000` - `0x00FF_FFFF` | 15 MiB | Boot ROM / FSBL window | Read-only after boot |
| `0x0100_0000` - `0x01FF_FFFF` | 16 MiB | Firmware SRAM / scratch | Early boot use |
| `0x0200_0000` - `0x02FF_FFFF` | 16 MiB | HLIC | Local interrupt/timer registers |
| `0x0300_0000` - `0x03FF_FFFF` | 16 MiB | HPIC | Platform interrupt controller |
| `0x0400_0000` - `0x04FF_FFFF` | 16 MiB | UART / low-speed debug | Early console |
| `0x0500_0000` - `0x05FF_FFFF` | 16 MiB | SPI / boot flash window | Firmware image storage |
| `0x0600_0000` - `0x06FF_FFFF` | 16 MiB | GPIO / I2C / SPI / PWM | Profile-specific low-speed peripherals |
| `0x0700_0000` - `0x07FF_FFFF` | 16 MiB | PMIC / reset / clock | Power/reset controls |
| `0x0800_0000` - `0x0BFF_FFFF` | 64 MiB | Profile-specific MMIO | MIPI, CAN, ADC, robotics |
| `0x0C00_0000` - `0x0FFF_FFFF` | 64 MiB | Reserved MMIO | Future platform devices |
| `0x1000_0000` - `0x1FFF_FFFF` | 256 MiB | PCIe ECAM / config | Desktop/server |
| `0x2000_0000` - `0x3FFF_FFFF` | 512 MiB | PCIe MMIO32 / device BARs | Desktop/server |
| `0x4000_0000` - `0x7FFF_FFFF` | 1 GiB | Framebuffer / accelerator aperture | Profile-specific |
| `0x8000_0000` - up | variable | DRAM | OS memory |

### 7.2 DRAM Base

Default DRAM base:

```text
HVM_DRAM_BASE = 0x8000_0000
```

The OS must not hard-code total DRAM size. It must read firmware-provided memory maps.

### 7.3 Memory Types

| Type | Cacheable | Executable | Typical Use |
| :--- | :---: | :---: | :--- |
| `RAM` | Yes | Yes | Kernel, user memory |
| `ROM` | Yes | Yes | Boot ROM, firmware |
| `MMIO` | No | No | Device registers |
| `DMA_COHERENT` | Yes | No by default | Shared DMA buffers |
| `DMA_NONCOHERENT` | Configurable | No | Device buffers requiring explicit sync |
| `RESERVED` | No | No | Firmware, secure memory, holes |
| `ACPI_RECLAIM` | Yes | No | ACPI tables after boot |
| `FRAMEBUFFER` | Write-combining preferred | No | Display |

---

## 8. Profile-Specific Physical Maps

### 8.1 `HVM-M1` Mobile

| Region | Requirement |
| :--- | :--- |
| DRAM | LPDDR5/LPDDR5X, starts at `0x8000_0000` by default |
| UFS | UFS-like block controller in profile MMIO window |
| MIPI-DSI | MMIO registers plus framebuffer bridge |
| MIPI-CSI | Camera control registers and DMA buffers |
| PMIC | Power-state, battery, thermal, wake event registers |
| Wireless | PCIe/SDIO stub or real device model depending board |

Mobile kernels should expect:

- aggressive suspend/resume
- non-removable storage
- display and camera through device tree
- no socketed memory

### 8.2 `HVM-D1` Desktop

| Region | Requirement |
| :--- | :--- |
| DRAM | DDR5 DIMM memory map from firmware |
| PCIe ECAM | Required |
| PCIe MMIO32/MMIO64 | Required |
| NVMe | PCIe device, not fixed MMIO in production profile |
| USB | MMIO or PCIe-attached controller |
| Firmware flash | SPI window for recovery and setup |

Desktop kernels should expect:

- ACPI/SMBIOS eventually
- PCIe enumeration
- user-selectable boot devices
- optional ECC depending board/SKU

### 8.3 `HVM-S1` Server

| Region | Requirement |
| :--- | :--- |
| DRAM | NUMA-aware DDR5 ECC RDIMM memory map |
| PCIe ECAM | Required, multiple segments possible |
| BMC bridge | Required |
| RAS registers | ECC, machine check, thermal, power cap |
| CXL/accelerator apertures | Optional/future |

Server kernels should expect:

- large core counts
- NUMA topology
- ECC/RAS event reporting
- BMC-managed reset/power
- possible reserved crash dump regions

### 8.4 `HVM-R1` Robotics

| Region | Requirement |
| :--- | :--- |
| RT SRAM | ECC-protected deterministic memory |
| App DRAM | LPDDR5/LPDDR5X or other board memory |
| CAN-FD | Fixed low-latency MMIO region |
| PWM | Fixed low-latency MMIO region |
| ADC | Fixed low-latency MMIO region |
| QEI/encoder | Fixed low-latency MMIO region |
| Safety island | Fault latch, watchdog, safe-state controls |

Robotics kernels should expect:

- RT memory separate from app memory
- safety outputs default to safe state
- interrupt latency matters
- deterministic mode may disable some dynamic power features

---

## 9. Virtual Address Layout

The default OS ABI uses a 39-bit virtual address space through HVM-39.

Recommended kernel layout:

| Virtual Range | Purpose |
| :--- | :--- |
| low canonical user range | user text, data, heap, stack |
| high canonical kernel range | kernel direct map, kernel text, vmalloc, modules |
| guard gaps | catch null and overflow accesses |

Suggested layout:

```text
0x0000_0000_0000_0000 - 0x0000_003F_FFFF_FFFF   user space
0x0000_0040_0000_0000 - 0x0000_007F_FFFF_FFFF   guard / future
0xFFFF_FF80_0000_0000 - 0xFFFF_FFBF_FFFF_FFFF   kernel direct map
0xFFFF_FFC0_0000_0000 - 0xFFFF_FFDF_FFFF_FFFF   kernel vmalloc/modules
0xFFFF_FFE0_0000_0000 - 0xFFFF_FFFF_FFFF_FFFF   kernel text/fixmap/vectors
```

The exact canonical-address rule must be finalized with the HVM-39 MMU specification. The simulator should trap noncanonical virtual addresses.

---

## 10. Page Table ABI

HVM-39 uses three page-table levels:

```text
VA[38:30] -> VPN[2]
VA[29:21] -> VPN[1]
VA[20:12] -> VPN[0]
VA[11:0]  -> page offset
```

Page size:

- 4 KiB base pages
- larger pages may be supported by leaf PTEs at higher levels once finalized

PTE fields:

| Field | Purpose |
| :--- | :--- |
| `V` | Valid |
| `R` | Readable |
| `W` | Writable |
| `X` | Executable |
| `U` | User accessible |
| `G` | Global mapping |
| `A` | Accessed |
| `D` | Dirty |
| `C` | Cacheable |
| `IO` | Device/MMIO mapping |
| `PPN` | Physical page number |

Rules:

- `W` without `R` is invalid unless explicitly defined otherwise.
- `X` on MMIO mappings is invalid.
- User mode cannot access supervisor mappings.
- Device mappings must be strongly ordered unless a weaker attribute is explicitly requested.
- The OS must issue TLB invalidation after changing live page tables.

---

## 11. Interrupt and Exception ABI

### 11.1 Trap Frame

Recommended kernel trap frame:

```c
struct HvmTrapFrame {
  uint64_t regs[32];
  uint64_t pc;
  uint64_t status;
  uint64_t cause;
  uint64_t tval;
  uint64_t badaddr;
  uint64_t hlic_state;
  uint64_t vector_state_flags;
};
```

### 11.2 Exception Causes

| Cause | Meaning |
| :---: | :--- |
| `0` | Instruction page fault |
| `1` | Load page fault |
| `2` | Store page fault |
| `3` | Illegal instruction |
| `4` | Privileged instruction |
| `5` | Misaligned instruction fetch |
| `6` | Misaligned load |
| `7` | Misaligned store |
| `8` | Breakpoint |
| `9` | Watchpoint |
| `10` | User syscall |
| `11` | Supervisor syscall |
| `12` | Machine check |
| `13` | Device bus fault |
| `14` | Vector disabled |
| `15` | HVM-ARC fault |
| `16` | Robotics safety fault |

### 11.3 Interrupt Classes

| Interrupt | Source |
| :--- | :--- |
| Local timer | HLIC timer compare |
| Software IPI | HLIC software interrupt |
| External platform IRQ | HPIC routed device interrupt |
| NMI | safety, machine check, watchdog, severe platform event |
| Debug | breakpoint, single-step, external debugger |

---

## 12. Syscall ABI

This chapter does not define a POSIX-compatible syscall table, but it reserves a standard syscall entry convention:

| Register | Meaning |
| :---: | :--- |
| `a6` (`r8`) | syscall number for OS ABIs that use register-selected syscalls |
| `a0-a5` (`r1-r3`, `r5-r7`) | syscall arguments |
| `a0` | return value |
| `a1` | secondary return value or error detail |

Recommended error convention:

- success: non-negative return in `a0`
- failure: negative errno-style value in `a0`

OS projects may define their own syscall tables, but they should preserve this register convention for toolchain and runtime compatibility.

The legacy `SYSCALL imm15` runtime interface in [hvm-spec.md](../hvm-spec.md) remains valid for Hoo runtime-internal services; it uses the immediate field to select the service and the core-runtime argument registers documented there.

---

## 13. ELF and Binary ABI

Recommended executable format:

- ELF64 little-endian
- HVM machine type to be assigned
- RELA relocations preferred
- DWARF debug info
- `.eh_frame` or equivalent unwind support

Required ABI metadata:

- HVM ISA level
- HVM extension bits required
- ABI profile: soft-float, hard-float, vector ABI
- minimum page size
- code model
- endianness

Suggested ELF note:

```text
Name: HVM
Type: HVM_ABI_TAG
Fields:
  abi_major
  abi_minor
  required_features
  optional_features
  vector_abi
  page_size
```

---

## 14. Device Discovery ABI

### 14.1 Device Tree

Device tree is required for:

- `HVM-M1`
- `HVM-R1`
- simulator-first bring-up of all profiles

Required nodes:

- `/cpus`
- `/memory`
- `/chosen`
- `/soc`
- interrupt controller nodes
- timer nodes
- UART node
- storage nodes
- profile-specific nodes

### 14.2 ACPI and SMBIOS

ACPI/SMBIOS are recommended for:

- `HVM-D1`
- `HVM-S1`

Minimum ACPI tables later:

- RSDP/RSDT or XSDT
- FADT
- MADT-equivalent HVM interrupt topology table
- MCFG-equivalent PCIe ECAM table
- SRAT/SLIT for server NUMA
- SPCR for serial console
- TPM2 where HSM/TPM exists

HVM-specific ACPI table names must be assigned before OS upstreaming.

---

## 15. Cache, DMA, and Coherency ABI

Rules for OS development:

- Normal RAM is cacheable and coherent unless firmware marks it otherwise.
- MMIO is uncached and strongly ordered.
- DMA buffers must be allocated from memory with attributes compatible with the device.
- Noncoherent devices require explicit cache clean/invalidate operations.
- `ICACHE.RNG` must be used after generating or modifying executable code.
- HVM-ARC atomics must be naturally aligned.

Recommended cache maintenance primitives:

| Operation | Purpose |
| :--- | :--- |
| `ICACHE.RNG base, size` | Invalidate instruction cache for modified code |
| `DCACHE.CLEAN base, size` | Clean data cache before device reads |
| `DCACHE.INV base, size` | Invalidate data cache after device writes |
| `PREFETCH.R base, offset` | Advisory read prefetch for likely future loads |
| `PREFETCH.W base, offset` | Advisory write-intent prefetch for likely future stores |
| `PREFETCH.NTA base, offset` | Advisory non-temporal prefetch for streaming data |
| `FENCE rw, rw` | Order memory operations |

Final mnemonics and encodings must be synchronized with the ISA table.

Prefetch rules:

- Prefetch operations are advisory. A conforming implementation may ignore them.
- Prefetch must not raise a page fault or device bus fault visible to software.
- Prefetch to MMIO has no required effect and should be suppressed by implementations where practical.
- JIT and simulator implementations may lower prefetch to host prefetch intrinsics or no-ops.

Cache policy attributes should primarily live in page-table entries, device tree, ACPI tables, or firmware memory maps. HVM should avoid adding mandatory per-instruction cache-policy controls until OS memory-type handling is fully specified.

---

## 16. Managed Runtime ABI Hooks

HVM runtime acceleration must not make the kernel responsible for understanding Hoo object layouts. The OS exposes standard memory, signal, trap, and TLS behavior; the Hoo runtime owns managed heap semantics.

### 16.1 Thread-Local Allocation Buffer ABI

If HVM-Alloc is enabled, the Hoo runtime may reserve fields in the thread-local storage block referenced by `tp`:

| Field | Purpose |
| :--- | :--- |
| `hvm_alloc_cursor` | Next free byte in the active thread-local allocation buffer |
| `hvm_alloc_limit` | End of the active thread-local allocation buffer |
| `hvm_alloc_epoch` | Optional runtime epoch for debug, safepoint, or allocator validation |
| `hvm_alloc_flags` | Runtime-owned flags for sanitizer, tracing, or slow-path forcing |

`ALLOC.BUMP` may read and update these fields directly or use implementation-defined allocation CSRs if a future ISA profile defines them. In either case:

- zero return means fast-path allocation failed and software must call the runtime allocator
- signal/trap delivery must preserve user-visible allocation state
- kernel code must not inspect object headers or TLAB contents
- debug/sanitizer runtimes may disable `ALLOC.BUMP` by clearing the feature bit

### 16.2 Compact Object References

Compact references are a managed-runtime representation, not a system pointer ABI. Public C/C++ ABI pointers remain 64-bit native virtual addresses.

Recommended runtime metadata:

| Field | Purpose |
| :--- | :--- |
| `heap_base` | Native virtual base address of the compact-reference heap window |
| `heap_shift` | Left shift applied before adding to `heap_base` |
| `heap_limit` | Maximum decoded native address or compact-reference count |
| `heap_generation` | Optional runtime generation for moving/compacting heaps |

Rules:

- The kernel treats compact references as integer payloads inside user memory.
- The JIT may keep references compressed in registers only inside managed-code regions.
- Transitions to native ABI calls must expand object references to native pointers unless the callee explicitly accepts compact references.
- The simulator should trap or report compact-reference decode violations when the configured heap window is exceeded.

---

## 17. Profile ABI Requirements

### 17.1 Minimum OS Port Requirements

To boot a minimal HVM OS, the platform must provide:

- one boot CPU
- RAM map
- UART console
- timer interrupt
- interrupt controller
- firmware handoff data
- page table format
- exception entry/return
- block device or initrd

### 17.2 Mobile OS Requirements

- suspend/resume
- PMIC events
- UFS storage
- framebuffer
- input/touch events
- thermal sensors
- device tree

### 17.3 Desktop OS Requirements

- PCIe enumeration
- NVMe
- USB input
- framebuffer/GPU path
- ACPI/SMBIOS eventually
- firmware boot variables

### 17.4 Server OS Requirements

- SMP scaling
- NUMA memory map
- ECC/RAS events
- BMC communication
- PCIe hot-plug later
- power cap/thermal telemetry
- crash dump reservation

### 17.5 Robotics OS/RTOS Requirements

- deterministic timer
- RT SRAM
- PWM
- ADC
- CAN-FD
- watchdog
- safety fault latch
- safe output defaults

---

## 18. Simulator Requirements for ABI Validation

The HVM simulator must validate this ABI through:

- boot tests using the common physical memory map
- device tree generation tests
- HVM boot info table tests
- HVM-SFI call tests
- trap frame round-trip tests
- syscall convention tests
- page table permission tests
- interrupt delivery tests
- cache invalidation tests for JIT code
- raw/HSD/VHDX disk boot tests
- profile-specific OS smoke tests

The simulator should provide:

```bash
hvm-sim abi-dump --machine hvm-mobile
hvm-sim abi-dump --machine hvm-desktop
hvm-sim abi-dump --machine hvm-server
hvm-sim abi-dump --machine hvm-robot
```

The output should include:

- memory map
- MMIO map
- interrupt map
- CPU topology
- firmware ABI version
- feature bits
- generated device tree or ACPI summary

---

## 19. Open ABI Decisions

- Final HVM ELF machine ID.
- Final syscall instruction encoding and syscall table ownership.
- Final `r31` architectural role.
- Final hard-float ABI.
- Final vector ABI preservation rules.
- Final HVM-39 PTE bit positions.
- Final cache maintenance instruction names and encodings.
- Final ACPI table IDs for HVM interrupt and topology information.
- Final secure monitor ABI.
- Final hypervisor ABI.
