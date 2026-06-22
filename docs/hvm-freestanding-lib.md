# Writing a Freestanding Library for HVM Kernel Development

Version: `1.0`

This document is a comprehensive guide for creating a **freestanding C/C++ library** — a runtime environment with no hosted libc, no OS kernel underneath, and no standard startup — targeting the **HVM 64-bit platform**. A freestanding library is the foundation upon which every HVM kernel, bootloader, and firmware project is built.

---

## 1. What Is a Freestanding Environment?

A freestanding implementation provides only a subset of the standard C/C++ headers and must supply its own:

- **Startup sequence** (reset vector → early init → kernel entry)
- **Memory model** (stack pointer, BSS zeroing, segment setup)
- **Compiler runtime** (division, shifting, `memcpy`, `memset` replacements)
- **Interrupt/exception vector table**
- **Synchronization primitives** (atomics, spinlocks, LR/SC macros)
- **CPU and CSR access wrappers** (privilege control, timer, MMU)

The C/C++ standard mandates that the following headers work in freestanding mode:

| C Header | C++ Equivalent | Contents |
| :--- | :--- | :--- |
| `<float.h>` | `<cfloat>` | Floating-point limits |
| `<iso646.h>` | `<ciso646>` | Alternative operator spellings |
| `<limits.h>` | `<climits>` | Integer type limits |
| `<stdalign.h>` | `<cstdalign>` | `alignas`/`alignof` macros |
| `<stdarg.h>` | `<cstdarg>` | Variadic argument handling |
| `<stdbool.h>` | `<cstdbool>` | `bool`/`true`/`false` macros |
| `<stddef.h>` | `<cstddef>` | `size_t`, `NULL`, `offsetof` |
| `<stdint.h>` | `<cstdint>` | Fixed-width integer types |
| `<stdnoreturn.h>` | `<cstdnoreturn>` | `noreturn` macro |

Everything else (stdio, stdlib, string, time, threads, etc.) must be provided by the freestanding library or left unimplemented.

---

## 2. HVM Platform ABI Summary

Every freestanding library for HVM must conform to the architectural ABI defined in `docs/system/12-hvm-memory-map-and-abi.md`.

### 2.1 Register Usage

| Register | ABI Name | Role | Preserved? |
| :---: | :--- | :--- | :---: |
| `r0` | `zero` | Hardwired zero | N/A |
| `r1` | `a0` / `ret` | Argument 0 / return value | No |
| `r2` | `a1` | Argument 1 | No |
| `r3` | `a2` | Argument 2 | No |
| `r4` | `tp` | Thread pointer / TLS base | Yes |
| `r5` | `a3` | Argument 3 | No |
| `r6` | `a4` | Argument 4 | No |
| `r7` | `a5` | Argument 5 | No |
| `r8` | `a6` | Argument 6 / service selector | No |
| `r9`–`r15` | `t0`–`t6` | Temporaries | No |
| `r16`–`r28` | `s0`–`s12` | Callee-saved | Yes |
| `r29` | `lr` | Link register | No |
| `r30` | `fp` | Frame pointer | Yes |
| `r31` | `sp` | Stack pointer | Yes |

Key rules:

- `r0` always reads as zero; writes are ignored.
- The return address is in `r29`; executed with `RET` or a direct jump to `r29`.
- The stack pointer `r31` must be 16-byte aligned at public call boundaries.
- `sp`, `tp`, `fp`, and `s0`–`s12` must be preserved across function calls.

### 2.2 Calling Convention

**Integer/pointer arguments:**

| Argument | Register |
| :---: | :---: |
| 1st | `r1` (a0) |
| 2nd | `r2` (a1) |
| 3rd | `r3` (a2) |
| 4th | `r5` (a3) |
| 5th | `r6` (a4) |
| 6th | `r7` (a5) |
| 7th | `r8` (a6) |
| 8th+ | Stack |

Integer return values use `r1` (a0). Two-word return values may use `r1` and `r2`.

**Stack layout at function entry:**

```
high addresses
  caller's stack arguments (if any)
  return spill area (optional)
  [sp] → saved r29 (lr)     ← ENTER saves this
  [sp+8] → saved r30 (fp)
  [sp+16] → local variables
low addresses
```

**Frame setup** (`ENTER` instruction):
```asm
enter imm15        ; sp -= 16, mem[sp]=r29, mem[sp+8]=r30, r30=sp, sp -= imm15
```

**Frame teardown** (`LEAVE` instruction):
```asm
leave              ; sp = r30, r29=mem[sp], r30=mem[sp+8], sp += 16
```

### 2.3 Data Model

- **Little-endian**, LP64.
- `char` = 8 bits, `short` = 16 bits, `int` = 32 bits, `long` = 64 bits, pointer = 64 bits.
- Natural alignment by type size up to 16 bytes.
- `size_t`, `uintptr_t`, `ptrdiff_t` are all 64-bit unsigned.

---

## 3. Toolchain Requirements

Before writing the freestanding library, you need an HVM target toolchain.

### 3.1 What You Need

| Tool | Purpose |
| :--- | :--- |
| HVM-aware assembler | Assembles HVM instruction encodings (base32 + escape32) |
| HVM-aware linker | ELF64-HVM linking with RELA relocations |
| `objcopy` | Convert ELF to raw binary for ROM images |
| `objdump` | Disassemble HVM binaries for debugging |

If the official HVM toolchain is not yet available, the freestanding library should be developed and tested inside the **HVM simulator** (`hvm-sim`) using hand-assembled opcodes, with a plan to migrate to assembler mnemonics when the toolchain matures.

### 3.2 Compiler Flags

For the freestanding kernel library:

```text
-ffreestanding
-nostdlib
-nostartfiles
-ffunction-sections
-fdata-sections
-mno-red-zone          (kernel mode: no red zone)
-mcmodel=large         (kernel may be loaded anywhere)
-fno-stack-protector
-fno-exceptions
-fno-rtti
-mabi=lp64
```

Linker flags:

```text
-nostdlib
-static
-Wl,-gc-sections
-Wl,-T,linker.ld
```

---

## 4. Freestanding Library Structure

A minimal freestanding library for HVM kernel development should be organised as follows:

```
hvm-freestanding/
  include/
    hvm/
      types.h          # Fixed-width types, bool, NULL, size_t
      csr.h            # CSR address constants and access macros
      cpu.h            # CPU state helpers, privilege levels
      mmu.h            # Page table entry structures and helpers
      sync.h           # Spinlocks, LR/SC atomics, memory barriers
      timer.h          # Cycle counter and timer compare access
      mem.h            # Memory operations (memset, memcpy, memmove)
      utils.h          # Utility macros (ARRAY_SIZE, ROUND_UP/DOWN, etc.)
  src/
    start.s            # Assembly startup / reset vector
    vectors.s          # Exception and interrupt vector table
    trap_entry.s       # Trap save/restore assembly
    cpu.c              # CPU init, CSR wrappers
    mem.c              # memset, memcpy, memmove, bzero
    sync.c             # Spinlock implementations
    timer.c            # Timer management
    log.c              # Early kernel logging (MMIO UART)
    printf.c           # Minimal formatted output
    div.c              # Compiler runtime (__udivdi3, __umoddi3, etc.)
    abort.c            # Abort and panic handlers
  linker.ld            # Kernel linker script
  Makefile             # Build rules
```

---

## 5. Startup Sequence

### 5.1 Reset Vector

On reset, the HVM CPU begins execution in **Supervisor mode** (S-mode) at the address specified by the firmware-reset vector. For a kernel that replaces the firmware boot path, the platform reset vector is at the address mapped in Boot ROM.

```asm
# start.s — Reset vector and early boot

.section .text.start,"ax"
.global _start
_start:
    # The CPU starts in S-mode with MMU disabled.
    # r0 is hardwired to zero. All other registers are undefined.

    # 1. Set up stack pointer (temporary boot stack)
    lda   sp, r0, __boot_stack_top

    # 2. Zero the BSS section
    lda   r1, r0, __bss_start
    lda   r2, r0, __bss_end
    call  r29, bzero              # bzero(r1=bss_start, r2=bss_end)

    # 3. Set up exception vector table
    lda   r1, r0, trap_vector_table
    csrrw r0, r1, 0x001           # stvec = trap_vector_table

    # 4. Set up initial page tables (if needed early)
    #    For now, run with MMU off (Bare mode).

    # 5. Call kernel_main
    call  r29, kernel_main

    # 6. If kernel_main returns, halt
halt_loop:
    jmp   halt_loop
```

### 5.2 BSS Zeroing Routine

```asm
# bzero — Zero a memory range
# r1 = start, r2 = end
bzero:
    sub   r3, r2, r1              # length
    ble   r3, r0, 2f             # if length <= 0, done
1:
    st.d  r0, r1, 0              # zero 8 bytes
    addi  r1, r1, 8
    addi  r3, r3, -8
    bne   r3, r0, 1b
2:
    ret
```

### 5.3 Boot Stack

The linker script reserves a small stack for early boot:

```ld
__boot_stack_start = .;
    . += 16K;
__boot_stack_top = .;
```

---

## 6. Exception and Interrupt Handling

### 6.1 Trap Vector Table

The `stvec` CSR points to the trap vector table. Use direct mode (mode = 0) for a single entry point:

```c
// cpu.h — Trap vector setup
#include <hvm/types.h>

#define CSR_SSTATUS   0x000
#define CSR_STVEC     0x001
#define CSR_SEPC      0x002
#define CSR_SCAUSE    0x003
#define CSR_STVAL     0x004
#define CSR_SATP      0x005
#define CSR_STIME     0x006
#define CSR_STIMECMP  0x007

static inline void csr_write(uint16_t addr, uint64_t val) {
    // CSRRW: Atomic read/write. Using r0 as rs suppresses write.
    // We need an inline assembly helper or a compiler built-in.
    asm volatile("csrrw r0, %0, %1" : : "i"(addr), "r"(val) : "memory");
}

static inline uint64_t csr_read(uint16_t addr) {
    uint64_t val;
    asm volatile("csrrw %0, %1, r0" : "=r"(val) : "i"(addr) : "memory");
    return val;
}

static inline void stvec_set(void (*handler)(void)) {
    uint64_t vec_addr = (uint64_t)handler & ~0x3ULL;
    csr_write(CSR_STVEC, vec_addr);
}
```

### 6.2 Trap Entry Assembly

```asm
# trap_entry.s — Unified trap handler entry

.section .text.trap,"ax"
.global trap_entry
trap_entry:
    # On trap:
    #   sepc = trapping PC
    #   scause = cause
    #   stval = fault address
    #   sstatus.SIE = 0, SPP = previous mode
    #   CPU switches to S-mode

    # Save all caller-saved registers
    st.d  r1,  sp,  -8            # push a0
    adjsp sp, -256                # allocate trap frame (32 regs + 6 extra)
    st.d  r1,  sp,  0
    st.d  r2,  sp,  8
    st.d  r3,  sp,  16
    st.d  r5,  sp,  24
    st.d  r6,  sp,  32
    st.d  r7,  sp,  40
    st.d  r8,  sp,  48
    st.d  r9,  sp,  56
    st.d  r10, sp,  64
    st.d  r11, sp,  72
    st.d  r12, sp,  80
    st.d  r13, sp,  88
    st.d  r14, sp,  96
    st.d  r15, sp,  104
    # Save callee-saved (s0-s12)
    st.d  r16, sp,  112
    st.d  r17, sp,  120
    st.d  r18, sp,  128
    st.d  r19, sp,  136
    st.d  r20, sp,  144
    st.d  r21, sp,  152
    st.d  r22, sp,  160
    st.d  r23, sp,  168
    st.d  r24, sp,  176
    st.d  r25, sp,  184
    st.d  r26, sp,  192
    st.d  r27, sp,  200
    st.d  r28, sp,  208
    st.d  r29, sp,  216            # lr
    st.d  r30, sp,  224            # fp
    st.d  r31, sp,  232            # sp

    # Read exception registers
    csrrw r1, CSR_SCAUSE, r0       # cause
    st.d  r1, sp, 240
    csrrw r2, CSR_SEPC, r0         # pc
    st.d  r2, sp, 248

    # Switch to kernel trap stack
    lda   r31, r0, __kernel_stack_top

    # Call C handler: trap_handler(cause, sepc, stval, frame)
    mov   r1,  r2                  # a0 = sepc
    csrrw r2, CSR_SCAUSE, r0       # a1 = cause
    csrrw r3, CSR_STVAL, r0        # a2 = stval
    mov   r5,  sp                  # a3 = frame pointer (original sp before switch)

    call  r29, trap_handler

    # After trap_handler returns, restore from saved frame
    # (trap_handler may modify the saved frame for signal delivery, etc.)

    # Restore registers from frame
    ld.d  r1,  sp,  0
    ld.d  r2,  sp,  8
    ...
    ld.d  r30, sp,  224
    ld.d  r31, sp,  232

    adjsp sp, 256

    trapret                        # return to trapped context
```

### 6.3 C Trap Handler

```c
// trap.c — Exception and interrupt dispatch

#include <hvm/types.h>
#include <hvm/csr.h>
#include <hvm/cpu.h>

void trap_handler(uint64_t sepc, uint64_t cause, uint64_t stval, uint64_t *frame) {
    if (cause & (1ULL << 63)) {
        // Interrupt
        uint64_t irq = cause & ~(1ULL << 63);
        switch (irq) {
        case 0: // Supervisor timer interrupt
            timer_irq_handler(frame);
            break;
        case 1: // Software/IPI interrupt
            ipi_irq_handler(frame);
            break;
        case 2: // External/HPIC interrupt
            hpic_irq_handler(frame);
            break;
        default:
            panic("Unknown interrupt: cause=%llx", cause);
        }
    } else {
        // Exception
        switch (cause) {
        case 0:  handle_page_fault_inst(sepc, stval, frame); break;
        case 1:  handle_page_fault_load(sepc, stval, frame); break;
        case 2:  handle_page_fault_store(sepc, stval, frame); break;
        case 3:  handle_illegal_instruction(sepc, frame); break;
        case 4:  handle_privileged_instruction(sepc, frame); break;
        case 8:  handle_syscall(sepc, frame); break;
        case 9:  handle_illegal_instruction(sepc, frame); break;
        case 10: handle_breakpoint(sepc, frame); break;
        default: panic("Unknown exception: cause=%llx at pc=%llx", cause, sepc);
        }
    }
}
```

---

## 7. Memory Operations

### 7.1 Standard Memory Functions

```c
// mem.c — Freestanding memory operations

void *memset(void *s, int c, size_t n) {
    unsigned char *p = (unsigned char *)s;
    uint64_t v = (unsigned char)c;
    v |= v << 8;
    v |= v << 16;
    v |= v << 32;
    // Word-aligned fast path
    while ((uintptr_t)p & 7) { if (!n--) return s; *p++ = (unsigned char)c; }
    size_t words = n >> 3;
    for (size_t i = 0; i < words; i++) { ((uint64_t *)p)[i] = v; }
    n -= words << 3;
    p += words << 3;
    while (n--) *p++ = (unsigned char)c;
    return s;
}

void *memcpy(void *dest, const void *src, size_t n) {
    // Copy loop — can be optimized with LD.P/ST.P for aligned pairs
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    while (n--) *d++ = *s++;
    return dest;
}

void *memmove(void *dest, const void *src, size_t n) {
    unsigned char *d = (unsigned char *)dest;
    const unsigned char *s = (const unsigned char *)src;
    if (d < s) {
        while (n--) *d++ = *s++;
    } else {
        d += n; s += n;
        while (n--) *--d = *--s;
    }
    return dest;
}
```

### 7.2 Atomic and Synchronisation Primitives

```c
// sync.h — Spinlocks and LR/SC atomics

typedef volatile uint64_t spinlock_t;

static inline void spin_init(spinlock_t *lock) { *lock = 0; }

static inline void spin_lock(spinlock_t *lock) {
    uint64_t tmp;
    do {
        // Load-reserve: monitor the lock word
        asm volatile("lr.d %0, (%1)" : "=r"(tmp) : "r"(lock) : "memory");
        if (tmp) continue; // already locked
        // Store-conditional: try to write 1
        asm volatile("sc.d %0, %1, %2"
                     : "=r"(tmp)
                     : "r"(lock), "r"((uint64_t)1)
                     : "memory");
    } while (tmp); // retry if SC failed
}

static inline void spin_unlock(spinlock_t *lock) {
    asm volatile("" ::: "memory");
    *lock = 0;
}

// Memory barrier (full)
static inline void mb(void) {
    // HVM uses RVWMO-compatible ordering; a full fence is:
    // For now, LR/SC provide acquire-release semantics;
    // a standalone fence intrinsic is needed when the ISA defines one.
    asm volatile("" ::: "memory"); // compiler barrier
}

// Atomic compare-and-swap using LR/SC
static inline uint64_t atomic_cas(volatile uint64_t *ptr,
                                   uint64_t expected, uint64_t desired) {
    uint64_t val, tmp;
    do {
        asm volatile("lr.d %0, (%1)" : "=r"(val) : "r"(ptr) : "memory");
        if (val != expected) return val;
        asm volatile("sc.d %0, %1, %2"
                     : "=r"(tmp)
                     : "r"(ptr), "r"(desired)
                     : "memory");
    } while (tmp);
    return expected;
}
```

---

## 8. CSR Access Layer

```c
// csr.h — CSR address definitions and access

#ifndef HVM_CSR_H
#define HVM_CSR_H

#include <hvm/types.h>

// Supervisor CSRs
#define CSR_SSTATUS   0x000
#define CSR_STVEC     0x001
#define CSR_SEPC      0x002
#define CSR_SCAUSE    0x003
#define CSR_STVAL     0x004
#define CSR_SATP      0x005
#define CSR_STIME     0x006
#define CSR_STIMECMP  0x007

// sstatus field masks
#define SSTATUS_SIE     (1ULL << 0)
#define SSTATUS_SPIE    (1ULL << 1)
#define SSTATUS_SPP     (1ULL << 8)
#define SSTATUS_UPIE    (1ULL << 9)

// scause encoding
#define SCAUSE_IRQ_FLAG (1ULL << 63)
#define SCAUSE_IRQ_TIMER  0
#define SCAUSE_IRQ_SOFT   1
#define SCAUSE_IRQ_EXT    2

// Exception causes
#define EX_CAUSE_INST_PAGE_FAULT   0
#define EX_CAUSE_LOAD_PAGE_FAULT   1
#define EX_CAUSE_STORE_PAGE_FAULT  2
#define EX_CAUSE_ILLEGAL_INST      3
#define EX_CAUSE_PRIV_INST         4
#define EX_CAUSE_MISALIGNED_FETCH  5
#define EX_CAUSE_MISALIGNED_LOAD   6
#define EX_CAUSE_MISALIGNED_STORE  7
#define EX_CAUSE_BREAKPOINT        8
#define EX_CAUSE_SYSCALL           8  // ECALL from U-mode
#define EX_CAUSE_ILLEGAL           9

// satp encoding
#define SATP_MODE_BARE  0ULL
#define SATP_MODE_HVM39 8ULL
#define SATP_ASID_SHIFT 44
#define SATP_PPN_SHIFT  0

static inline uint64_t satp_make(uint64_t mode, uint16_t asid, uint64_t ppn) {
    return (mode << 60) | ((uint64_t)asid << 44) | ppn;
}

#endif // HVM_CSR_H
```

---

## 9. Timer Primitives

```c
// timer.c — Cycle counter and timer compare

#include <hvm/csr.h>
#include <hvm/timer.h>

uint64_t timer_get_cycles(void) {
    return csr_read(CSR_STIME);
}

void timer_set_compare(uint64_t value) {
    csr_write(CSR_STIMECMP, value);
}

void timer_irq_handler(uint64_t *frame) {
    // Acknowledge by setting stimecmp in the future
    uint64_t now = timer_get_cycles();
    timer_set_compare(now + 100000);  // e.g., 100K cycles from now
    // Call scheduler tick
    scheduler_tick(frame);
}
```

---

## 10. MMU Primitives

```c
// mmu.h — Page table helpers for HVM-39

#include <hvm/types.h>
#include <hvm/csr.h>

#define PAGE_SIZE       4096
#define PAGE_SHIFT      12
#define PAGE_MASK       (PAGE_SIZE - 1)
#define PAGE_ALIGN(x)   (((x) + PAGE_MASK) & ~PAGE_MASK)

// HVM-39 page table entry
typedef volatile uint64_t pte_t;

// PTE flags
#define PTE_V      (1ULL << 0)
#define PTE_R      (1ULL << 1)
#define PTE_W      (1ULL << 2)
#define PTE_X      (1ULL << 3)
#define PTE_U      (1ULL << 4)
#define PTE_G      (1ULL << 5)
#define PTE_A      (1ULL << 6)
#define PTE_D      (1ULL << 7)
#define PTE_C      (1ULL << 8)   // Cacheable
#define PTE_IO     (1ULL << 9)   // Device/MMIO
#define PTE_RSW_SHIFT 8

#define PTE_PPN_SHIFT  10
#define PTE_PPN_MASK   0xFFFFFFFFFFFC00ULL  // bits 53:10

// Permissions for common page types
#define PTE_RW         (PTE_V | PTE_R | PTE_W | PTE_A | PTE_C)
#define PTE_RWX        (PTE_V | PTE_R | PTE_W | PTE_X | PTE_A | PTE_D | PTE_C)
#define PTE_RX          (PTE_V | PTE_R | PTE_X | PTE_A | PTE_C)
#define PTE_MMIO       (PTE_V | PTE_R | PTE_W | PTE_A | PTE_IO)
#define PTE_USER(p)    ((p) | PTE_U)

static inline uint64_t pa_to_ppn(uint64_t phys_addr) {
    return phys_addr >> PAGE_SHIFT;
}

static inline uint64_t ppn_to_pa(uint64_t ppn) {
    return ppn << PAGE_SHIFT;
}

static inline pte_t pte_create(uint64_t ppn, uint64_t flags) {
    return (ppn << PTE_PPN_SHIFT) | flags;
}

// HVM-39 virtual address split
// VA[38:30] = VPN[2], VA[29:21] = VPN[1], VA[20:12] = VPN[0]
#define VPN2(va) (((uint64_t)(va) >> 30) & 0x1FF)
#define VPN1(va) (((uint64_t)(va) >> 21) & 0x1FF)
#define VPN0(va) (((uint64_t)(va) >> 12) & 0x1FF)
#define VA_OFFSET(va) ((uint64_t)(va) & 0xFFF)

static inline void tlb_flush_all(void) {
    csr_write(CSR_SFENCE_VMA, 0);  // SFENCE.VMA
}
```

---

## 11. Linker Script

```ld
/* linker.ld — HVM kernel linker script */

OUTPUT_ARCH(hvm)
OUTPUT_FORMAT(elf64-littlehvm)
ENTRY(_start)

PHYS_BASE = 0x80000000;   /* DRAM base */
KERNEL_OFFSET = 0x100000; /* 1 MB offset for kernel image */

SECTIONS
{
    . = PHYS_BASE + KERNEL_OFFSET;

    .text : ALIGN(4096) {
        __text_start = .;
        *(.text.start)
        *(.text*)
        *(.trap*)
        __text_end = .;
    }

    .rodata : ALIGN(4096) {
        __rodata_start = .;
        *(.rodata*)
        *(.srodata*)
        __rodata_end = .;
    }

    .data : ALIGN(4096) {
        __data_start = .;
        *(.data*)
        *(.sdata*)
        __data_end = .;
    }

    .bss : ALIGN(4096) {
        __bss_start = .;
        *(.bss*)
        *(.sbss*)
        *(COMMON)
        __bss_end = .;
    }

    __boot_stack_start = .;
    . += 16K;
    __boot_stack_top = .;

    __kernel_stack_start = .;
    . += 64K;
    __kernel_stack_top = .;

    __end = .;
}
```

---

## 12. Compiler Runtime Support

A freestanding kernel cannot link against `libgcc` or `compiler-rt` unless they are cross-compiled for HVM. Provide your own runtime helpers for operations the compiler may emit:

```c
// div.c — Compiler runtime for 64-bit division

uint64_t __udivdi3(uint64_t a, uint64_t b) {
    uint64_t result = 0;
    // Software division if no DIVU instruction is available
    // (DIVU is part of HVM core ISA, but provide fallback)
    if (b == 0) return (uint64_t)-1;
    // Use HVM DIVU directly via inline assembly
    asm("divu %0, %1, %2" : "=r"(result) : "r"(a), "r"(b));
    return result;
}

int64_t __divdi3(int64_t a, int64_t b) {
    int64_t result;
    if (b == 0) return -1;
    asm("div %0, %1, %2" : "=r"(result) : "r"(a), "r"(b));
    return result;
}

uint64_t __umoddi3(uint64_t a, uint64_t b) {
    uint64_t result;
    if (b == 0) return a;
    asm("rem %0, %1, %2" : "=r"(result) : "r"(a), "r"(b));
    return result;
}

// Multi-word shifts
uint64_t __ashldi3(uint64_t a, uint32_t b) {
    return a << (b & 63);
}

uint64_t __lshrdi3(uint64_t a, uint32_t b) {
    return a >> (b & 63);
}
```

---

## 13. Early Console Output

Before the kernel has full device drivers, output goes through the **MMIO UART** or HVM-SFI console:

```c
// log.c — Early kernel logging via MMIO UART

#include <hvm/types.h>

#define UART_BASE        0x04000000   /* UART MMIO base */
#define UART_THR         0x00         /* Transmit Holding Register */
#define UART_LSR         0x14         /* Line Status Register */
#define UART_LSR_THRE    0x20         /* Transmitter Holding Register Empty */

static volatile uint8_t *uart = (volatile uint8_t *)UART_BASE;

void uart_putchar(char c) {
    // Wait for TX FIFO to be ready
    while (!(uart[UART_LSR] & UART_LSR_THRE)) {}
    uart[UART_THR] = (uint8_t)c;
    if (c == '\n') uart_putchar('\r');
}

void log_puts(const char *s) {
    while (*s) uart_putchar(*s++);
}

void log_printf(const char *fmt, ...) {
    // Minimal printf implementation for kernel boot messages
    // (Provided in printf.c — handles %s, %x, %d, %llx, %p)
}
```

---

## 14. Building and Testing

### 14.1 Makefile Template

```makefile
CC = hvm-elf-gcc
AS = hvm-elf-as
LD = hvm-elf-ld
OBJCOPY = hvm-elf-objcopy

CFLAGS = -ffreestanding -nostdlib -nostartfiles \
         -mno-red-zone -mcmodel=large -O2 -g \
         -Iinclude -Wall -Wextra -Werror

ASFLAGS = -g
LDFLAGS = -nostdlib -static -T linker.ld

SRCS_C = $(wildcard src/*.c)
SRCS_S = $(wildcard src/*.s)
OBJS = $(SRCS_C:.c=.o) $(SRCS_S:.s=.o)

all: kernel.elf kernel.bin

kernel.elf: $(OBJS) linker.ld
	$(LD) $(LDFLAGS) -o $@ $(OBJS)

kernel.bin: kernel.elf
	$(OBJCOPY) -O binary $< $@

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.s
	$(AS) $(ASFLAGS) -o $@ $<

run: kernel.bin
	hvm-sim run --machine hvm-desktop --kernel kernel.bin

debug: kernel.elf
	hvm-sim run --machine hvm-desktop --kernel kernel.bin --debug

clean:
	rm -f $(OBJS) kernel.elf kernel.bin
```

### 14.2 Simulator Testing

Test the freestanding library inside the HVM simulator:

```bash
# Boot the kernel ELF directly
hvm-sim run --machine hvm-mobile --kernel build/kernel.elf

# With initrd
hvm-sim run --machine hvm-desktop \
    --kernel build/kernel.elf \
    --initrd test_initrd.bin

# Enable trace for debugging early boot
hvm-sim run --machine hvm-mobile \
    --kernel build/kernel.elf \
    --trace cpu.exec,cpu.exception,mmio.write
```

---

## 15. Checklist for a Complete Freestanding Library

| Component | Status | Notes |
| :--- | :---: | :--- |
| Reset vector (`_start`) | Required | Sets sp, zeroes BSS, calls kernel_main |
| Trap vector + save/restore | Required | Must save all 32 GPRs, read sepc/scause/stval |
| CSR read/write helpers | Required | Inline asm macros for CSRRW |
| `memset` / `memcpy` / `memmove` | Required | Used by compiler and kernel |
| Spinlock (LR/SC) | Required | For SMP boot and driver locks |
| Atomic CAS (LR/SC) | Required | For reference counting, futexes |
| Timer read + compare | Required | For scheduler ticks |
| MMU page table helpers | Required | PTE creation, TLB flush |
| Early UART console | Required | MMIO 16550-compatible driver |
| Linker script | Required | Must define sections, stack, BSS symbols |
| Compiler runtime (`__udivdi3`, etc.) | Recommended | The compiler may emit calls to these |
| `printf` / `vprintf` | Recommended | Debug logging |
| `abort` / `panic` | Recommended | Fatal error handling |
| HVM-SFI client stubs | Optional | For firmware service calls |
| Stack unwinding / backtrace | Optional | For debugging panics |
| TLS setup | Optional | For per-CPU data |

The freestanding library defined in this document is the minimal foundation every HVM kernel project needs. Extend it with architecture-specific features as your kernel grows: slab allocators, VFS abstractions, device driver frameworks, and POSIX syscall layers all build on top of these primitives.

---

## 16. References

- [HVM Specification](hvm-spec.md) — ISA, privilege model, CSRs, trap handling
- [HVM System Memory Map and ABI](system/12-hvm-memory-map-and-abi.md) — Register ABI, boot handoff, calling convention, physical map
- [HVM Instruction Set CSV](hvm_instruction_set.csv) — All mnemonics and encodings
- [HVM Register Set CSV](hvm_register_set.csv) — Full register definitions
- [HVM Object File Format (HO)](ho-file-format.md) — ELF variant for HVM
- [HVM Simulator Design](system/10-hvm-lightweight-system-simulator-design.md) — Simulator architecture for testing
- [HVM Firmware and BIOS Options](system/09-hvm-open-source-firmware-bios-options.md) — Boot firmware landscape
