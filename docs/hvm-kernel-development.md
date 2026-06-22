# Developing a Production-Quality Kernel for the HVM Platform

Version: `1.0`

This document is a comprehensive architectural guide for building a **production-quality operating system kernel** targeting the **HVM 64-bit platform** — from the first reset to a fully functional kernel with memory management, processes, scheduling, device drivers, SMP, and system calls.

The guide assumes you have a working freestanding library (see [hvm-freestanding-lib.md](hvm-freestanding-lib.md)) and an HVM toolchain. It is designed to be toolchain-agnostic: the kernel can be written in C, C++, or Rust with appropriate HVM backends.

---

## Table of Contents

1. [Kernel Architecture Overview](#1-kernel-architecture-overview)
2. [Boot Process](#2-boot-process)
3. [Memory Management](#3-memory-management)
4. [The HVM-39 MMU](#4-the-hvm-39-mmu)
5. [Process and Thread Management](#5-process-and-thread-management)
6. [Scheduling](#6-scheduling)
7. [Interrupt and Exception Handling](#7-interrupt-and-exception-handling)
8. [System Calls](#8-system-calls)
9. [Timer and Timekeeping](#9-timer-and-timekeeping)
10. [Device Driver Framework](#10-device-driver-framework)
11. [SMP Boot and Synchronisation](#11-smp-boot-and-synchronisation)
12. [Block I/O and Storage](#12-block-io-and-storage)
13. [File System Layer](#13-file-system-layer)
14. [HVM-SFI Client](#14-hvm-sfi-client)
15. [Process ELF Loading](#15-process-elf-loading)
16. [Signal and Trap Delivery](#16-signal-and-trap-delivery)
17. [Performance and Production Considerations](#17-performance-and-production-considerations)
18. [Testing and Validation](#18-testing-and-validation)
19. [Platform Profiles](#19-platform-profiles)
20. [References](#20-references)

---

## 1. Kernel Architecture Overview

### 1.1 Design Tenets

A production HVM kernel should follow these principles:

| Principle | Rationale |
| :--- | :--- |
| **Monolithic with modular drivers** | Best performance for a new platform; microkernel overhead is unjustified until the ecosystem is mature. |
| **SMP-capable from day one** | HVM platforms ship with 2–128 cores; retrofitting SMP is expensive. |
| **Device-tree based discovery** | Required by mobile and robotics profiles; usable by all. |
| **HPIC interrupt model** | Platform interrupts route through the HVM Platform Interrupt Controller. |
| **HVM-SFI for firmware services** | Timers, IPIs, reset, and early console use the Supervisor Firmware Interface. |
| **LR/SC for atomics** | Load-reserve / store-conditional provides lock-free primitives without dedicated atomic instructions. |
| **Tiered memory management** | Physical allocator → slab/small-object allocator → vmalloc for kernel mappings → user-space demand paging. |

### 1.2 Kernel Source Layout

```
kernel/
  arch/
    hvm/
      boot/          # Reset vector, early init, trap vectors
      cpu/           # CPU init, CSR management, cache ops
      mmu/           # HVM-39 page tables, TLB management
      irq/           # HLIC + HPIC interrupt controller drivers
      syscall/       # Syscall entry and dispatch
      timer/         # Cycle counter and timer compare
      smp/           # Secondary CPU boot and IPI handlers
  core/
    main.c           # kernel_main entry point
    panic.c          # Panic and assertion handling
    printf.c         # Kernel printf
  mm/
    pmm.c            # Physical memory (page frame) allocator
    vmm.c            # Virtual memory manager
    slab.c           # Slab allocator for kernel objects
  proc/
    task.c           # Task / process structure
    scheduler.c      # Scheduler (O(1) or CFS-like)
    context.c        # Context switch assembly
  ipc/
    futex.c          # Fast user-space mutex (futex)
  fs/
    vfs.c            # Virtual file system layer
    devfs.c          # Device file system
    initrd.c         # Initial ramdisk
    hvmfs.c          # HVM-native file system (optional)
  drivers/
    uart.c           # MMIO 16550-compatible UART
    timer.c          # HLIC timer
    hpic.c           # HPIC platform interrupt controller
    block/           # Block device abstraction
    virtio/          # Virtio block/net (optional)
    framebuffer.c    # Simple framebuffer driver (mobile/desktop)
  include/
    <kernel headers> # Internal kernel API headers
  lib/
    <freestanding>   # Freestanding runtime library
  build/
    linker.ld        # Kernel linker script
```

---

## 2. Boot Process

### 2.1 Boot Flow Overview

```
Power-on / Reset
     |
     v
[ Boot ROM ] — loads FSBL from SPI/UFS/NVMe
     |
     v
[ FSBL ] — initialises clocks, DRAM, basic platform
     |
     v
[ Firmware / Bootloader ] — discovers devices, creates device tree
     |
     v
[ Kernel entry (_start) ] -- raw binary or ELF loaded by bootloader
     |
     v
[ Early setup ] — SP, BSS, trap vectors, early MMIO UART
     |
     v
[ MMU init ] — identity-map kernel, enable HVM-39
     |
     v
[ Platform init ] — timer, HPIC, SMP bring-up, device tree scan
     |
     v
[ Kernel subsystems ] — scheduler, VFS, block, drivers
     |
     v
[ Init process ] — launch /sbin/init or equivalent
```

### 2.2 Reset Vector and Early Entry

The kernel entry point (`_start`) receives the following handoff from firmware/bootloader:

| Register | Value |
| :---: | :--- |
| `a0` (`r1`) | Boot hart/core ID |
| `a1` (`r2`) | Physical pointer to device tree blob or HVM boot info table |
| `a2` (`r3`) | Physical pointer to HVM-SFI service table |
| `a3` (`r5`) | Boot flags |
| `a4` (`r6`) | Initrd physical base (or 0) |
| `a5` (`r7`) | Initrd size (or 0) |
| `sp` (`r31`) | Temporary boot stack |
| `pc` | Kernel entry point (`_start`) |

Boot flags (`a3`):

| Bit | Meaning |
| :---: | :--- |
| 0 | Device tree pointer is valid |
| 1 | HVM boot info table pointer is valid |
| 2 | Initrd is present |
| 3 | Firmware has enabled caches |
| 4 | Firmware has enabled MMU for handoff |
| 5 | Secure boot verified |
| 6 | Booted from simulator |

### 2.3 Early Initialisation Sequence

```c
// kernel_main phase 1 — called from _start

#include <kernel/types.h>
#include <kernel/early.h>
#include <hvm/csr.h>
#include <hvm/mmu.h>

void kernel_main(uint64_t boot_hart_id, uint64_t dtb_ptr,
                  uint64_t sfi_table, uint64_t boot_flags,
                  uint64_t initrd_base, uint64_t initrd_size) {

    // Phase 1: CPU-local early init (on boot hart only, IRQs disabled)
    early_console_init();                    // MMIO UART at 0x04000000
    log_puts("HVM Kernel booting...\n");

    // Save boot parameters
    boot_info.boot_hart_id = boot_hart_id;
    boot_info.dtb_ptr = dtb_ptr;
    boot_info.sfi_table = sfi_table;
    boot_info.boot_flags = boot_flags;
    boot_info.initrd_base = initrd_base;
    boot_info.initrd_size = initrd_size;

    // Phase 2: Trap vector and early memory
    stvec_set(trap_vector_table);            // Set stvec
    early_page_table_init();                 // Minimal identity map

    // Phase 3: Enable MMU (HVM-39)
    mmu_enable(kernel_page_table);

    // Phase 4: Full subsystem init (now running with VM on)
    platform_init();                         // Timer, HPIC, SMP
    scheduler_init();
    vfs_init();
    if (initrd_base) initrd_mount(initrd_base, initrd_size);

    // Phase 5: Launch init process
    task_t *init = task_create_kernel("/sbin/init", NULL);
    scheduler_enqueue(init);

    // Phase 6: Become idle task
    sched_start();
    panic("sched_start returned");
}
```

### 2.4 Early Page Table Setup

Before enabling the MMU, the kernel must create an identity map for its own code:

```c
// early_mmu.c — Minimal early page tables

pte_t early_pgd[512] __attribute__((aligned(PAGE_SIZE)));
pte_t early_pmd[512] __attribute__((aligned(PAGE_SIZE)));
pte_t early_pte[512] __attribute__((aligned(PAGE_SIZE)));

void early_page_table_init(void) {
    // Identity-map the first 2 MB at 0x80000000 (kernel load area)
    uint64_t phys = 0x80000000;

    // Level 2 (PGD): VPN[2] = 0 (for 0x80000000, VA[38:30] = 0b1)
    // Level 2 index for 0x80000000: VA[38:30] = 0x100000000 >> 30 = 1
    int l2_idx = 1;
    early_pgd[l2_idx] = pte_create(pa_to_ppn((uint64_t)early_pmd),
                                   PTE_V);

    // Level 1 (PMD): single 2 MB block covering 0x80000000-0x801FFFFF
    int l1_idx = 0;
    early_pmd[l1_idx] = pte_create(pa_to_ppn((uint64_t)early_pte),
                                   PTE_V);

    // Level 0 (PTE): 512 × 4 KB pages
    for (int i = 0; i < 512; i++) {
        early_pte[i] = pte_create(pa_to_ppn(phys + i * PAGE_SIZE),
                                  PTE_RWX);
    }
}

void mmu_enable(pte_t *root_table) {
    uint64_t satp = satp_make(SATP_MODE_HVM39, 0,
                              pa_to_ppn((uint64_t)root_table));
    csr_write(CSR_SATP, satp);
    tlb_flush_all();
    log_puts("MMU enabled (HVM-39)\n");
}
```

---

## 3. Memory Management

### 3.1 Physical Memory Allocator

The physical memory manager (PMM) manages free page frames. A bitmap or buddy allocator is recommended.

```c
// pmm.h — Physical memory manager interface

#include <hvm/types.h>

#define PAGE_SIZE   4096
#define PAGE_SHIFT  12

void  pmm_init(uint64_t mem_start, uint64_t mem_end, uint64_t *reserved_regions);
void *pmm_alloc_page(void);             // Returns physical address
void  pmm_free_page(void *phys_addr);
void *pmm_alloc_zeroed_page(void);      // Allocate + memset(0)
void  pmm_mark_used(uint64_t phys, size_t pages);
void  pmm_mark_free(uint64_t phys, size_t pages);
```

The PMM initialisation reads the memory map from the device tree or HVM boot info table:

```c
// pmm.c — Physical memory allocator (buddy system)

#include <kernel/mm/pmm.h>
#include <kernel/early.h>

#define MAX_PAGES (512 * 1024)  // 2 GB / 4 KB
static uint64_t bitmap[MAX_PAGES / 64];
static uint64_t total_pages;

void pmm_init(uint64_t mem_start, uint64_t mem_end, uint64_t *reserved) {
    total_pages = (mem_end - mem_start) / PAGE_SIZE;

    // Mark all pages as free initially
    memset(bitmap, 0, sizeof(bitmap));

    // Mark reserved regions (kernel image, page tables, initrd, etc.)
    while (reserved) {
        uint64_t base = reserved[0];
        uint64_t size = reserved[1];
        if (base == 0 && size == 0) break;
        uint64_t start_page = (base - mem_start) / PAGE_SIZE;
        uint64_t end_page = start_page + size / PAGE_SIZE;
        for (uint64_t i = start_page; i < end_page; i++) {
            bitmap[i / 64] |= (1ULL << (i % 64));
        }
        reserved += 2;
    }

    log_printf("PMM: %llu MB free (%llu pages)\n",
               (total_pages * PAGE_SIZE) >> 20, total_pages);
}
```

### 3.2 Slab Allocator

Kernel objects (tasks, file handles, inodes) are allocated from slab caches:

```c
// slab.h — Slab allocator

typedef struct slab_cache {
    size_t  object_size;
    size_t  align;
    void   *free_list;    // Singly-linked free objects
    uint64_t num_objects;
    uint64_t num_pages;
} slab_cache_t;

slab_cache_t *slab_create(const char *name, size_t object_size, size_t align);
void  *slab_alloc(slab_cache_t *cache);
void   slab_free(slab_cache_t *cache, void *obj);
void   slab_destroy(slab_cache_t *cache);
```

### 3.3 Virtual Memory Manager

The VMM manages the kernel virtual address space and provides:

- `kmap()` / `kunmap()` — Map physical pages into kernel space
- `vmalloc()` / `vfree()` — Non-contiguous kernel virtual allocations
- User address-space management per process

```c
// vmm.h — Virtual memory manager

// Recommended HVM kernel virtual address layout
#define KERNEL_DIRECT_MAP_BASE  0xFFFFFF8000000000ULL
#define KERNEL_VMALLOC_BASE     0xFFFFFFC000000000ULL
#define KERNEL_TEXT_BASE        0xFFFFFFE000000000ULL
#define USER_SPACE_END          0x0000003FFFFFFFFFFFULL

// Kernel direct map: each physical page is mapped at
//   KERNEL_DIRECT_MAP_BASE + phys_addr
// This gives the kernel simple phys<->virt conversion:
static inline void *phys_to_virt(uint64_t phys) {
    return (void *)(KERNEL_DIRECT_MAP_BASE + phys);
}
static inline uint64_t virt_to_phys(void *virt) {
    return (uint64_t)virt - KERNEL_DIRECT_MAP_BASE;
}

void  vmm_init(void);
void *vmap(uint64_t phys, size_t size, uint64_t flags);
void  vunmap(void *virt, size_t size);
void *vmalloc(size_t size);
void  vfree(void *virt);
```

---

## 4. The HVM-39 MMU

### 4.1 Page Table Structure

HVM-39 is a three-level radix tree over 39-bit virtual addresses:

```
VA[63:39] must equal VA[38] (canonical sign-extension)

VA[38:30]  → Level 2 index (512 entries)
VA[29:21]  → Level 1 index (512 entries)
VA[20:12]  → Level 0 index (512 entries)
VA[11:0]   → Page offset (4 KiB)
```

Each page table is exactly 4 KiB (512 × 8-byte PTEs).

### 4.2 Page Table Entry Format

```
Bit  0: V    — Valid
Bit  1: R    — Readable
Bit  2: W    — Writable
Bit  3: X    — Executable
Bit  4: U    — User-accessible
Bit  5: G    — Global (not flushed on ASID switch)
Bit  6: A    — Accessed (set by hardware on access)
Bit  7: D    — Dirty (set by hardware on write)
Bit  8: C    — Cacheable
Bit  9: IO   — Device/MMIO
Bits 15:10 — Reserved for software (RSW)
Bits 53:10 — PPN (physical page number, 44 bits, shifted right by 12)
Bits 63:54 — Reserved
```

### 4.3 Page Table Walker

```c
// mmu.c — HVM-39 page table walker

#include <hvm/mmu.h>

// Walk the page table for a given virtual address.
// Returns the physical address or 0 if not mapped.
uint64_t mmu_walk(pte_t *root, uint64_t virt_addr) {
    uint64_t vpn2 = VPN2(virt_addr);
    uint64_t vpn1 = VPN1(virt_addr);
    uint64_t vpn0 = VPN0(virt_addr);
    uint64_t offset = VA_OFFSET(virt_addr);

    pte_t l2 = root[vpn2];
    if (!(l2 & PTE_V)) return 0;
    if ((l2 & (PTE_R | PTE_W | PTE_X)) == 0) {
        // Non-leaf: go to L1
        pte_t *l1_table = (pte_t *)ppn_to_pa((l2 >> PTE_PPN_SHIFT));
        pte_t l1 = l1_table[vpn1];
        if (!(l1 & PTE_V)) return 0;
        if ((l1 & (PTE_R | PTE_W | PTE_X)) == 0) {
            // Non-leaf: go to L0
            pte_t *l0_table = (pte_t *)ppn_to_pa((l1 >> PTE_PPN_SHIFT));
            pte_t l0 = l0_table[vpn0];
            if (!(l0 & PTE_V)) return 0;
            // Leaf PTE
            return ppn_to_pa(l0 >> PTE_PPN_SHIFT) + offset;
        }
        // Large page at L1 (2 MB)
        return ppn_to_pa(l1 >> PTE_PPN_SHIFT) + (virt_addr & 0x1FFFFF);
    }
    // Large page at L2 (1 GB)
    return ppn_to_pa(l2 >> PTE_PPN_SHIFT) + (virt_addr & 0x3FFFFFFF);
}
```

### 4.4 Address Space Management

```c
// Process address space

typedef struct address_space {
    pte_t       *pgd;           // Root page table (physical)
    spinlock_t   lock;
    uint64_t     lowest_brk;    // Heap start
    uint64_t     brk;           // Current heap end
    uint64_t     mmap_base;     // Base for mmap allocations
    region_t    *regions;       // Linked list of mapped regions
} address_space_t;

address_space_t *as_create(void);
void as_destroy(address_space_t *as);
int as_map(address_space_t *as, uint64_t virt, uint64_t phys,
           size_t size, uint64_t flags);
int as_unmap(address_space_t *as, uint64_t virt, size_t size);
int as_handle_page_fault(address_space_t *as, uint64_t fault_addr,
                          uint64_t cause, bool user_mode);
```

### 4.5 TLB Management

```c
// TLB flush operations

// Flush entire TLB
static inline void tlb_flush_all(void) {
    csr_write(CSR_SFENCE_VMA, 0);
}

// Flush TLB for a specific virtual address
static inline void tlb_flush_addr(uint64_t vaddr) {
    // SFENCE.VMA with rs1 = vaddr
    asm volatile("csrrw r0, %0, %1" :: "i"(CSR_SFENCE_VMA), "r"(vaddr));
}

// Flush TLB for an ASID
static inline void tlb_flush_asid(uint16_t asid) {
    uint64_t val = ((uint64_t)asid) << 2;  // ASID in bits [1:0]?
    asm volatile("csrrw r0, %0, %1" :: "i"(CSR_SFENCE_VMA), "r"(val));
}
```

---

## 5. Process and Thread Management

### 5.1 Task Structure

```c
// task.h — Kernel task/process structure

#define TASK_NAME_MAX   64
#define THREAD_STACK_SIZE 16384  // 16 KB per thread kernel stack

typedef enum {
    TASK_READY,
    TASK_RUNNING,
    TASK_BLOCKED,
    TASK_WAITING,
    TASK_ZOMBIE,
    TASK_DEAD
} task_state_t;

typedef struct task {
    // Core scheduling
    uint64_t           id;           // PID / TID
    char               name[TASK_NAME_MAX];
    task_state_t       state;
    int                priority;
    uint64_t           time_slice;   // Remaining scheduler time slice
    uint64_t           total_ticks;  // Total CPU time consumed

    // Context
    uint64_t           regs[32];     // Saved registers
    uint64_t           pc;           // Saved PC (sepc on trap)
    uint64_t           sstatus;      // Saved sstatus
    void              *kernel_sp;    // Kernel stack pointer

    // Address space (NULL for kernel threads)
    address_space_t   *addr_space;
    uint64_t           entry_point;
    uint64_t           stack_top;    // User-space stack top

    // Kernel stack (per-thread)
    uint8_t           *kstack;

    // File system
    struct file_desc  *fds[256];

    // Wait queue links
    struct task       *next;
    struct task       *prev;

    // Children / parent
    struct task       *parent;
    struct task       *children;
    struct task       *sibling;

    // Wait status
    int                exit_code;
    struct wait_queue *wait_queue;

    // TLS
    uint64_t           tls_base;     // r4 (tp) value
} task_t;
```

### 5.2 Context Switch

The context switch saves and restores all callee-saved registers plus the PC:

```c
// context.c — Context switch

// switch_to(&old_sp, new_sp)
// Saves current context, restores target context.
// Called from scheduler.
__attribute__((naked))
void switch_to(uint64_t *old_sp, uint64_t new_sp) {
    // This function saves callee-saved registers (r16-r28, r30, r31)
    // onto the current kernel stack, then switches sp to the new task's
    // kernel stack, and restores callee-saved registers from it.
    //
    // The HVM ABI expects:
    //   r16-r28 (s0-s12) — callee saved
    //   r29 (lr) — return address (saved/restored as part of frame)
    //   r30 (fp) — frame pointer (callee saved)
    //   r31 (sp) — stack pointer
    //
    // context switch pseudo-code:
    //
    //   // Push callee-saved onto current stack
    //   st.d r16, sp, -8
    //   st.d r17, sp, -16
    //   ...
    //   st.d r30, sp, -??
    //   st.d fp, sp, -??
    //   st.d lr, sp, -??
    //
    //   // Save current SP to *old_sp
    //   st.d sp, (old_sp)
    //
    //   // Restore new SP
    //   mov sp, new_sp
    //
    //   // Pop callee-saved from new stack
    //   ld.d lr, sp, offset
    //   ld.d fp, sp, offset
    //   ld.d r16, sp, offset
    //   ...
    //
    //   // Return to new task's execution
    //   ret
    asm volatile(
        // Save callee-saved registers
        "st.d  r16, sp, -8\n"
        "st.d  r17, sp, -16\n"
        "st.d  r18, sp, -24\n"
        "st.d  r19, sp, -32\n"
        "st.d  r20, sp, -40\n"
        "st.d  r21, sp, -48\n"
        "st.d  r22, sp, -56\n"
        "st.d  r23, sp, -64\n"
        "st.d  r24, sp, -72\n"
        "st.d  r25, sp, -80\n"
        "st.d  r26, sp, -88\n"
        "st.d  r27, sp, -96\n"
        "st.d  r28, sp, -104\n"
        "st.d  r30, sp, -112\n"
        "st.d  r29, sp, -120\n"
        "adjsp sp, -128\n"

        // Save SP to old_sp
        "st.d  sp, (%0)\n"

        // Load new SP
        "mov   sp, %1\n"

        // Restore callee-saved from new stack
        "ld.d  r29, sp, 120\n"
        "ld.d  r30, sp, 112\n"
        "ld.d  r28, sp, 104\n"
        "ld.d  r27, sp, 96\n"
        "ld.d  r26, sp, 88\n"
        "ld.d  r25, sp, 80\n"
        "ld.d  r24, sp, 72\n"
        "ld.d  r23, sp, 64\n"
        "ld.d  r22, sp, 56\n"
        "ld.d  r21, sp, 48\n"
        "ld.d  r20, sp, 40\n"
        "ld.d  r19, sp, 32\n"
        "ld.d  r18, sp, 24\n"
        "ld.d  r17, sp, 16\n"
        "ld.d  r16, sp, 8\n"
        "adjsp sp, 128\n"

        "ret\n"
        :
        : "r"(old_sp), "r"(new_sp)
        : "memory"
    );
}
```

### 5.3 Task Creation

```c
// task.c — Task creation

static uint64_t next_pid = 1;

task_t *task_create(void (*entry)(void *arg), void *arg,
                     const char *name, bool user_mode) {

    task_t *task = slab_alloc(task_cache);
    memset(task, 0, sizeof(task_t));

    task->id = atomic_fetch_add(&next_pid, 1);
    strncpy(task->name, name, TASK_NAME_MAX - 1);
    task->state = TASK_READY;
    task->priority = DEFAULT_PRIORITY;
    task->time_slice = DEFAULT_TIME_SLICE;

    // Allocate kernel stack
    task->kstack = pmm_alloc_zeroed_page();   // 16 KB (4 pages)
    task->kstack += (4 * PAGE_SIZE);          // Stack grows down

    if (user_mode) {
        task->addr_space = as_create();
        // Set up user stack and initial registers
        // r1 (a0) = arg
        // r29 (lr) = entry
        // sp = user stack top
        // pc = entry point
        // sstatus: SPP = 0 (user mode), SIE = 1
        task->regs[1] = (uint64_t)arg;
        task->regs[29] = (uint64_t)entry;
        task->regs[31] = task->stack_top;
        task->pc = (uint64_t)entry;
        task->sstatus = SSTATUS_SIE;  // User mode, interrupts enabled
    } else {
        // Kernel thread
        task->addr_space = kernel_address_space;
        task->kstack = alloc_kernel_stack();
        task->regs[1] = (uint64_t)arg;
        task->regs[29] = (uint64_t)entry;
        task->regs[31] = (uint64_t)(task->kstack + THREAD_STACK_SIZE);
        task->pc = (uint64_t)entry;
        task->sstatus = SSTATUS_SIE | SSTATUS_SPP;  // S-mode
    }

    return task;
}
```

---

## 6. Scheduling

### 6.1 Scheduler Architecture

A production kernel should implement a multi-level feedback queue or a CFS-like scheduler. For HVM's target profiles (mobile through server), a simple but fair O(1) scheduler is suitable for initial versions.

```c
// scheduler.h

#define MAX_PRIORITY     32
#define DEFAULT_PRIORITY 16
#define DEFAULT_TIME_SLICE 10000  // In timer ticks

typedef struct run_queue {
    task_t *head[MAX_PRIORITY];
    task_t *tail[MAX_PRIORITY];
    uint64_t bitmap;  // Bitmask of non-empty queues
    spinlock_t lock;
} run_queue_t;

void  scheduler_init(void);
void  scheduler_enqueue(task_t *task);
void  scheduler_dequeue(task_t *task);
task_t *scheduler_pick_next(void);
void  scheduler_tick(uint64_t *frame);  // Called from timer IRQ
void  scheduler_yield(void);
```

### 6.2 Scheduler Tick

```c
// scheduler.c — Timer tick and reschedule

void scheduler_tick(uint64_t *frame) {
    task_t *current = this_cpu()->current_task;

    // Save current task context from trap frame
    memcpy(current->regs, frame, 32 * sizeof(uint64_t));
    current->pc = frame[32];      // sepc from trap frame
    current->sstatus = frame[33]; // sstatus from trap frame

    // Decrement time slice
    current->time_slice--;
    if (current->time_slice == 0) {
        current->time_slice = DEFAULT_TIME_SLICE;
        current->priority = MIN(current->priority + 1, MAX_PRIORITY - 1);
        scheduler_enqueue(current);
        task_t *next = scheduler_pick_next();
        if (next != current) {
            this_cpu()->current_task = next;
            switch_to(&current->kernel_sp, next->kernel_sp);
        }
    }
}
```

### 6.3 Idle Task

```c
// idle.c — Per-CPU idle task

__attribute__((noreturn))
void idle_task(void *arg) {
    (void)arg;
    while (1) {
        // Halt until next interrupt
        // Use a WFI-like mechanism if available, otherwise spin
        asm volatile("nop");
        scheduler_yield();
    }
}
```

---

## 7. Interrupt and Exception Handling

### 7.1 Trap Vector Table

The HVM architecture uses `stvec` for trap dispatch. For production use, use **vectored mode** (mode = 1) so each interrupt cause jumps directly to its handler:

```c
// irq.c — Interrupt controller setup

void trap_vector_init(void) {
    // In direct mode (mode=0): all traps go to trap_entry
    // In vectored mode (mode=1): PC = stvec.BASE + cause * 4
    // The kernel places a jump table at stvec.BASE.

    extern char trap_vector_table[];
    uint64_t vec = (uint64_t)trap_vector_table;

    // Ensure 4-byte alignment and set vectored mode
    csr_write(CSR_STVEC, vec | 1);  // vectored mode
}
```

The vectored jump table in assembly:

```asm
# vectors.s — Vectored interrupt/exception jump table

.section .text.vectors,"ax"
.global trap_vector_table
trap_vector_table:
    jmp   trap_inst_page_fault      # cause 0
    jmp   trap_load_page_fault      # cause 1
    jmp   trap_store_page_fault     # cause 2
    jmp   trap_illegal_inst         # cause 3
    jmp   trap_privileged_inst      # cause 4
    jmp   trap_misalign_fetch       # cause 5
    jmp   trap_misalign_load        # cause 6
    jmp   trap_misalign_store       # cause 7
    jmp   trap_breakpoint           # cause 8
    jmp   trap_illegal              # cause 9
    jmp   trap_breakpoint           # cause 10 (BREAK)
    # ...
    # Interrupts (bit 63 set) start at offset 16
    . = trap_vector_table + 16 * 4
    jmp   trap_s_timer              # IRQ 0: Supervisor timer
    jmp   trap_s_soft               # IRQ 1: Software/IPI
    jmp   trap_s_ext                # IRQ 2: External (HPIC)
```

### 7.2 Interrupt Controller Drivers

**HLIC (Local Interrupt Controller):**

```c
// hlic.c — Per-core local interrupt controller

#define HLIC_BASE          0x02000000
#define HLIC_TIMER_CTRL    0x00
#define HLIC_SOFT_CTRL     0x08
#define HLIC_IPI_SEND      0x10
#define HLIC_STATUS        0x18

static volatile uint64_t *hlic = (uint64_t *)HLIC_BASE;

void hlic_init(void) {
    // Enable local timer interrupt
    hlic[HLIC_TIMER_CTRL] = 1;
    // Set sstatus.SIE to enable supervisor interrupts
    uint64_t sstatus = csr_read(CSR_SSTATUS);
    csr_write(CSR_SSTATUS, sstatus | SSTATUS_SIE);
}

void hlic_send_ipi(uint64_t target_hart) {
    hlic[HLIC_IPI_SEND] = target_hart;
}
```

**HPIC (Platform Interrupt Controller):**

```c
// hpic.c — Platform interrupt controller

#define HPIC_BASE           0x03000000
#define HPIC_PRIORITY(n)    (0x00 + (n) * 4)   // Per-IRQ priority
#define HPIC_PENDING        0x400
#define HPIC_ENABLE_SET     0x500
#define HPIC_ENABLE_CLR     0x600
#define HPIC_CLAIM          0x700
#define HPIC_COMPLETE       0x708

static volatile uint32_t *hpic = (uint32_t *)HPIC_BASE;

void hpic_init(void) {
    // Disable all interrupts
    for (int i = 0; i < 256; i++) {
        hpic[HPIC_ENABLE_CLR / 4] = (1u << i);
    }
}

void hpic_enable_irq(uint32_t irq) {
    hpic[HPIC_ENABLE_SET / 4] = (1u << irq);
}

uint32_t hpic_claim(void) {
    return hpic[HPIC_CLAIM / 4];
}

void hpic_complete(uint32_t irq) {
    hpic[HPIC_COMPLETE / 4] = irq;
}

// HPIC external interrupt handler (called from trap_s_ext)
void hpic_irq_handler(uint64_t *frame) {
    uint32_t irq = hpic_claim();
    if (irq == 0 || irq == 0xFFFFFFFF) return;  // Spurious

    // Dispatch to registered handler
    irq_handler_t handler = irq_table[irq];
    if (handler) handler(irq, frame);

    hpic_complete(irq);
}
```

### 7.3 Page Fault Handler

```c
// fault.c — Page fault handling

void handle_page_fault(uint64_t sepc, uint64_t cause,
                        uint64_t stval, uint64_t *frame) {

    task_t *current = this_cpu()->current_task;
    uint64_t fault_addr = stval;

    // Determine fault type
    bool is_user = !(frame[33] & SSTATUS_SPP);  // SPP in saved sstatus

    if (current && current->addr_space) {
        int result = as_handle_page_fault(
            current->addr_space, fault_addr, cause, is_user);

        if (result == 0) {
            // Page fault resolved (COW, demand-paging, etc.)
            tlb_flush_addr(fault_addr);
            return;
        }
    }

    // Unhandled page fault — deliver SIGSEGV or panic
    if (is_user) {
        signal_deliver(current, SIGSEGV, fault_addr);
        return;
    }

    panic("Kernel page fault at pc=%llx, vaddr=%llx, cause=%llu",
          sepc, fault_addr, cause);
}
```

---

## 8. System Calls

### 8.1 Syscall Convention

HVM kernel syscalls use the following register convention:

| Register | Purpose |
| :---: | :--- |
| `r8` (`a6`) | Syscall number |
| `r1` (`a0`) | Argument 0 |
| `r2` (`a1`) | Argument 1 |
| `r3` (`a2`) | Argument 2 |
| `r5` (`a3`) | Argument 3 |
| `r6` (`a4`) | Argument 4 |
| `r7` (`a5`) | Argument 5 |
| `r1` (`a0`) | Return value (negative = error) |

User programs invoke syscalls with `ECALL` from U-mode:

```asm
# User-space syscall wrapper
# syscall(number, arg0, arg1, arg2)
syscall:
    mov   r8, r1        # a6 = syscall number
    mov   r1, r2        # a0 = arg0
    mov   r2, r3        # a1 = arg1
    mov   r3, r5        # a2 = arg2
    ecall               # Trap to S-mode
    ret                 # Result in r1
```

### 8.2 Syscall Dispatch

```c
// syscall.c — Syscall entry and dispatch

// Called from trap_entry when scause == 8 (ECALL from U-mode)
void syscall_handler(uint64_t *frame) {
    task_t *current = this_cpu()->current_task;
    uint64_t nr = frame[8];   // r8 = syscall number
    uint64_t a0 = frame[1];
    uint64_t a1 = frame[2];
    uint64_t a2 = frame[3];
    uint64_t a3 = frame[5];
    uint64_t a4 = frame[6];
    uint64_t a5 = frame[7];

    int64_t result = -ENOSYS;

    if (nr < MAX_SYSCALLS && syscall_table[nr]) {
        result = syscall_table[nr](current, a0, a1, a2, a3, a4, a5);
    }

    frame[1] = (uint64_t)result;   // Return value in a0

    // Advance PC past the ECALL instruction
    frame[32] += 4;  // sepc = sepc + 4
}
```

### 8.3 Syscall Table

```c
// syscall_table.c — System call table definition

typedef int64_t (*syscall_fn)(task_t *task, uint64_t, uint64_t,
                               uint64_t, uint64_t, uint64_t, uint64_t);

#define SYSCALL(nr, fn) [nr] = fn

syscall_fn syscall_table[MAX_SYSCALLS] = {
    SYSCALL(0,  sys_restart_syscall),
    SYSCALL(1,  sys_exit),
    SYSCALL(2,  sys_fork),
    SYSCALL(3,  sys_read),
    SYSCALL(4,  sys_write),
    SYSCALL(5,  sys_open),
    SYSCALL(6,  sys_close),
    SYSCALL(7,  sys_wait4),
    SYSCALL(8,  sys_creat),
    SYSCALL(9,  sys_link),
    SYSCALL(10, sys_unlink),
    SYSCALL(11, sys_execve),
    SYSCALL(12, sys_chdir),
    SYSCALL(13, sys_mknod),
    SYSCALL(14, sys_chmod),
    SYSCALL(15, sys_lseek),
    SYSCALL(20, sys_getpid),
    SYSCALL(21, sys_mount),
    SYSCALL(22, sys_umount),
    SYSCALL(23, sys_gettimeofday),
    SYSCALL(24, sys_mmap),
    SYSCALL(25, sys_munmap),
    SYSCALL(26, sys_sched_yield),
    // ... additional syscalls as needed
};
```

### 8.4 Key System Calls

**`sys_write`** — Write to a file descriptor:

```c
int64_t sys_write(task_t *task, uint64_t fd, uint64_t buf,
                   uint64_t count, uint64_t a3, uint64_t a4, uint64_t a5) {
    (void)a3; (void)a4; (void)a5;
    if (fd >= 256 || !task->fds[fd]) return -EBADF;

    file_desc_t *file = task->fds[fd];
    uint64_t written = 0;

    // Validate user buffer access
    if (!access_ok(buf, count, USER_READ)) return -EFAULT;

    // If stdout/stderr and UART is available, write directly
    if (fd == 1 || fd == 2) {
        const char *str = (const char *)buf;
        for (uint64_t i = 0; i < count; i++) uart_putchar(str[i]);
        return count;
    }

    written = file_write(file, (const void *)buf, count);
    return written;
}
```

**`sys_mmap`** — Map memory into user space:

```c
int64_t sys_mmap(task_t *task, uint64_t addr, uint64_t length,
                  uint64_t prot, uint64_t flags,
                  uint64_t fd, uint64_t offset) {
    (void)offset;

    // Align to page boundary
    length = PAGE_ALIGN(length);

    if (flags & MAP_ANONYMOUS) {
        // Allocate zeroed physical pages and map into user space
        uint64_t pages = length / PAGE_SIZE;
        uint64_t virt = as_find_free_region(task->addr_space,
                                             length, addr);

        for (uint64_t i = 0; i < pages; i++) {
            uint64_t phys = (uint64_t)pmm_alloc_zeroed_page();
            as_map(task->addr_space, virt + i * PAGE_SIZE,
                   phys, PAGE_SIZE, PTE_USER(PTE_RW));
        }
        return virt;
    }

    // File-backed mmap
    file_desc_t *file = task->fds[fd];
    if (!file) return -EBADF;

    // ... file-backed mmap logic
    return -ENOSYS;
}
```

---

## 9. Timer and Timekeeping

### 9.1 Timer Initialisation

```c
// timer.c — System timer

#define TIMER_FREQUENCY   1000000  // 1 MHz reference (platform-dependent)

void timer_init(void) {
    // Set initial timer compare: fire in 100K cycles
    uint64_t now = csr_read(CSR_STIME);
    csr_write(CSR_STIMECMP, now + 100000);
}

void timer_irq_handler(uint64_t *frame) {
    // Acknowledge timer interrupt
    uint64_t now = csr_read(CSR_STIME);
    csr_write(CSR_STIMECMP, now + TIMER_INTERVAL);

    // Scheduler tick
    scheduler_tick(frame);

    // Network timers, etc.
    timer_softirq_check();
}
```

### 9.2 Timekeeping

```c
// time.c — Wall-clock timekeeping

typedef struct timespec {
    int64_t tv_sec;
    int64_t tv_nsec;
} timespec_t;

static uint64_t boot_cycles;
static uint64_t cycles_per_second = 1000000;  // Platform-dependent

void timekeeping_init(uint64_t cps) {
    cycles_per_second = cps;
    boot_cycles = csr_read(CSR_STIME);
}

uint64_t timekeeping_get_ns(void) {
    uint64_t elapsed = csr_read(CSR_STIME) - boot_cycles;
    return (elapsed * 1000000000ULL) / cycles_per_second;
}

int64_t sys_gettimeofday(task_t *task, uint64_t tv_ptr,
                          uint64_t tz_ptr, ...) {
    (void)task; (void)tz_ptr;
    uint64_t ns = timekeeping_get_ns();
    timespec_t ts;
    ts.tv_sec = ns / 1000000000ULL;
    ts.tv_nsec = ns % 1000000000ULL;
    if (copy_to_user(tv_ptr, &ts, sizeof(timespec_t))) return -EFAULT;
    return 0;
}
```

---

## 10. Device Driver Framework

### 10.1 Driver Model

```c
// device.h — Device driver interface

typedef struct device {
    char        name[64];
    uint64_t    mmio_base;
    uint64_t    mmio_size;
    uint32_t    irq;
    uint32_t    irq_count;
    void       *priv_data;        // Driver-specific state
    spinlock_t  lock;

    // Driver ops
    int (*init)(struct device *dev);
    int (*reset)(struct device *dev);
    void (*shutdown)(struct device *dev);

    // Power management
    int (*suspend)(struct device *dev);
    int (*resume)(struct device *dev);

    struct device *next;
} device_t;

int  device_register(device_t *dev);
int  device_trigger_probe(device_t *dev);
void device_enumerate_dtb(void *dtb_ptr);
```

### 10.2 Device Discovery

Devices are discovered from the device tree. The kernel parses the DTB passed in `a1`:

```c
// dtb.c — Flattened device tree parser

#define FDT_MAGIC           0xD00DFEED
#define FDT_BEGIN_NODE      0x00000001
#define FDT_END_NODE        0x00000002
#define FDT_PROP            0x00000003
#define FDT_END             0x00000009

typedef struct fdt_header {
    uint32_t magic;
    uint32_t totalsize;
    uint32_t off_dt_struct;
    uint32_t off_dt_strings;
    uint32_t off_mem_rsvmap;
    uint32_t version;
    uint32_t last_comp_version;
    uint32_t boot_cpuid_phys;
    uint32_t size_dt_strings;
    uint32_t size_dt_struct;
} fdt_header_t;

void dtb_parse(void *dtb, device_t **devices_out) {
    fdt_header_t *hdr = (fdt_header_t *)dtb;
    if (hdr->magic != __builtin_bswap32(FDT_MAGIC)) {
        log_puts("Invalid DTB magic\n");
        return;
    }

    // Walk the structure block, create device entries for
    // /soc nodes with "compatible" and "reg" properties.
    // Match against known HVM peripheral compat strings:
    //   "hvm,uart"  -> UART at MMIO base
    //   "hvm,hlic"  -> Local interrupt controller
    //   "hvm,hpic"  -> Platform interrupt controller
    //   "hvm,block" -> Block device
    //   "hvm,framebuffer" -> Display
    //   "hvm,nvme"  -> NVMe controller
    // ...
}
```

### 10.3 UART Driver

```c
// uart.c — 16550-compatible HVM UART driver

#define UART_RBR     0x00   // Receive Buffer Register (read)
#define UART_THR     0x00   // Transmit Holding Register (write)
#define UART_LSR     0x14   // Line Status Register
#define UART_LSR_THRE 0x20  // Transmitter Holding Register Empty
#define UART_LSR_DR   0x01  // Data Ready

int uart_init(device_t *dev) {
    uint64_t base = dev->mmio_base;

    // Set baud rate, 8N1
    volatile uint8_t *regs = (uint8_t *)base;
    // ... 16550 initialisation sequence

    log_printf("UART at 0x%llx, IRQ %d\n", base, dev->irq);

    // Enable RX interrupt
    hpic_enable_irq(dev->irq);
    return 0;
}

void uart_putchar(char c) {
    volatile uint8_t *regs = (uint8_t *)UART_BASE;
    while (!(regs[UART_LSR] & UART_LSR_THRE)) {}
    regs[UART_THR] = (uint8_t)c;
    if (c == '\n') uart_putchar('\r');
}

int uart_getchar(void) {
    volatile uint8_t *regs = (uint8_t *)UART_BASE;
    if (!(regs[UART_LSR] & UART_LSR_DR)) return -1;
    return regs[UART_RBR];
}
```

### 10.4 Block Device Framework

```c
// block.h — Block device interface

#define BLOCK_SIZE   512

typedef struct block_device {
    char    name[32];
    uint64_t sector_count;
    uint32_t sector_size;
    spinlock_t lock;
    void    *priv;

    int  (*read)(struct block_device *dev, uint64_t sector,
                 void *buffer, uint32_t count);
    int  (*write)(struct block_device *dev, uint64_t sector,
                  const void *buffer, uint32_t count);
    int  (*flush)(struct block_device *dev);
    int  (*discard)(struct block_device *dev, uint64_t sector,
                    uint32_t count);
} block_device_t;

// I/O request queue
typedef struct bio_request {
    uint64_t           sector;
    void              *buffer;
    uint32_t           count;
    int                write;    // 0 = read, 1 = write
    int                status;
    struct task       *completion_task;
    struct bio_request *next;
} bio_request_t;

void block_submit_request(block_device_t *dev, bio_request_t *req);
int  block_read(block_device_t *dev, uint64_t sector,
                void *buffer, uint32_t count);
int  block_write(block_device_t *dev, uint64_t sector,
                 const void *buffer, uint32_t count);
```

---

## 11. SMP Boot and Synchronisation

### 11.1 Secondary Core Wakeup

The boot hart brings up secondary cores via HVM-SFI IPI or platform-specific wake sequence:

```c
// smp.c — SMP bring-up

typedef struct {
    uint64_t    entry_point;    // Where the secondary core should jump
    uint64_t    stack_top;      // Its kernel stack top
    spinlock_t  lock;           // Core waits here until released
    uint64_t    cpu_id;
    uint64_t    started;        // 1 when core is running
} core_wakeup_t;

static core_wakeup_t wakeup_data[MAX_CPUS];

void smp_boot(void) {
    uint64_t boot_hart = csr_read(CSR_STIME);  // Read CPU ID through HW

    // Prepare wakeup data for each secondary core
    for (int i = 1; i < cpu_count; i++) {
        wakeup_data[i].entry_point = (uint64_t)secondary_start;
        wakeup_data[i].stack_top = (uint64_t)alloc_kernel_stack() + 16384;
        wakeup_data[i].lock = 1;
        wakeup_data[i].cpu_id = i;
        wakeup_data[i].started = 0;

        // Send IPI to wake core i
        hlic_send_ipi(i);
    }

    // Wait for all cores to start
    for (int i = 1; i < cpu_count; i++) {
        while (!wakeup_data[i].started) {}
    }

    log_printf("SMP: %d cores online\n", cpu_count);
}

// Secondary core entry point (jumped to from wakeup vector)
void secondary_start(void) {
    uint64_t cpu_id = this_cpu_id();

    // Set up kernel stack
    lda sp, r0, wakeup_data[cpu_id].stack_top;

    // Set trap vector
    stvec_set(trap_vector_table);

    // Enable MMU (share kernel page table)
    csr_write(CSR_SATP, satp_make(SATP_MODE_HVM39, 0,
                                   pa_to_ppn(kernel_page_table)));
    tlb_flush_all();

    // Mark started
    wakeup_data[cpu_id].started = 1;

    // Enable interrupts
    uint64_t sstatus = csr_read(CSR_SSTATUS);
    csr_write(CSR_SSTATUS, sstatus | SSTATUS_SIE);

    // Start idle task
    scheduler_enqueue(idle_task);
    sched_start();
}
```

### 11.2 Per-CPU Data

```c
// percpu.h — Per-CPU data structures

#define PER_CPU_OFFSET 0xFFFF800000000000ULL  // Per-CPU data region

typedef struct per_cpu {
    task_t    *current_task;
    task_t    *idle_task;
    uint64_t   cpu_id;
    uint64_t   irq_nesting;
    uint64_t   sched_ticks;
    spinlock_t sched_lock;

    // Statistics
    uint64_t   irq_count;
    uint64_t   context_switches;
    uint64_t   page_faults;
} per_cpu_t;

static inline per_cpu_t *this_cpu(void) {
    // Read tp register (r4) which points to per-CPU data
    uint64_t tp;
    asm volatile("mov %0, r4" : "=r"(tp));
    return (per_cpu_t *)tp;
}

void percpu_init(void) {
    for (int i = 0; i < cpu_count; i++) {
        per_cpu_t *data = (per_cpu_t *)(PER_CPU_OFFSET + i * 4096);
        memset(data, 0, sizeof(per_cpu_t));
        data->cpu_id = i;
        data->current_task = NULL;
        data->idle_task = NULL;
    }

    // Set tp to this core's per-CPU data
    uint64_t cpu_id = get_boot_hart_id();
    asm volatile("mov r4, %0" : : "r"(PER_CPU_OFFSET + cpu_id * 4096));
}
```

---

## 12. Block I/O and Storage

### 12.1 Block Device Stack

```
User process
    |  read(fd, buf, count)
    v
VFS layer (sys_read -> file_read)
    |  inode_ops->read(inode, buf, offset)
    v
Page cache (cached: memcpy; uncached: bio)
    |  block_device->read(sector, buffer, count)
    v
Block device driver
    |  MMIO commands to NVMe / UFS / virtio
    v
Physical storage (simulated or real)
```

### 12.2 NVMe Driver Example

```c
// nvme.c — NVMe block device driver (simplified)

#define NVME_MMIO_BASE   0x40000000
#define NVME_CAP         0x00
#define NVME_VS          0x08
#define NVME_INTMS       0x0C
#define NVME_INTMC       0x10
#define NVME_CC          0x14
#define NVME_CSTS        0x1C
#define NVME_AQA         0x24
#define NVME_ASQ         0x28
#define NVME_ACQ         0x30

int nvme_init(device_t *dev) {
    volatile uint64_t *nvme = (uint64_t *)dev->mmio_base;

    // Reset controller
    nvme[NVME_CC / 8] = 0;
    while (nvme[NVME_CSTS / 8] & (1 << 0)) {}  // Wait for ready

    // Enable controller
    nvme[NVME_CC / 8] = (1 << 0);  // Enable
    // ... full initialisation: admin queue, I/O queue, identify

    // Register as block device
    block_device_register(&nvme_block_device);
    return 0;
}
```

---

## 13. File System Layer

### 13.1 Virtual File System (VFS)

```c
// vfs.h — Virtual file system layer

typedef struct inode {
    uint64_t       ino;
    uint64_t       size;
    uint64_t       blocks;
    uint32_t       uid;
    uint32_t       gid;
    uint32_t       mode;
    uint64_t       atime;
    uint64_t       mtime;
    uint64_t       ctime;
    struct fs_ops *fs;
    void          *private_data;   // Filesystem-specific
    spinlock_t     lock;
    uint32_t       refcount;
} inode_t;

typedef struct file_desc {
    uint64_t       offset;
    inode_t       *inode;
    uint32_t       flags;        // O_RDONLY, O_WRONLY, O_RDWR
    uint32_t       refcount;
    struct file_ops *ops;
} file_desc_t;

typedef struct file_ops {
    int     (*open)(inode_t *inode, file_desc_t *file);
    int     (*close)(file_desc_t *file);
    int64_t (*read)(file_desc_t *file, void *buf, uint64_t count);
    int64_t (*write)(file_desc_t *file, const void *buf, uint64_t count);
    uint64_t (*lseek)(file_desc_t *file, int64_t offset, int whence);
    int     (*mmap)(file_desc_t *file, task_t *task, uint64_t addr,
                    uint64_t length, uint64_t prot, uint64_t offset);
} file_ops_t;

typedef struct fs_ops {
    const char *name;
    int (*init)(void);
    int (*mount)(const char *source, const char *target,
                 const char *fstype, uint64_t flags);
    inode_t *(*alloc_inode)(void);
    int (*read_inode)(inode_t *inode);
    int (*write_inode)(inode_t *inode);
    int (*lookup)(inode_t *dir, const char *name, inode_t **result);
    int (*readdir)(inode_t *dir, uint64_t offset,
                   struct dirent *entry);
    int (*create)(inode_t *dir, const char *name, uint32_t mode);
    int (*unlink)(inode_t *dir, const char *name);
    int (*mkdir)(inode_t *dir, const char *name, uint32_t mode);
    int (*rmdir)(inode_t *dir, const char *name);
} fs_ops_t;
```

### 13.2 Initrd Filesystem

An initial ramdisk is essential for bootstrapping:

```c
// initrd.c — Minimal initrd filesystem (cpio-based)

int initrd_mount(uint64_t base, uint64_t size) {
    // Parse cpio archive in memory
    struct cpio_header *hdr = (struct cpio_header *)base;
    // ... walk cpio entries, create inodes
    // Register root inode in VFS
    vfs_set_root(root_inode);
    log_printf("Initrd mounted: %llu bytes\n", size);
    return 0;
}
```

---

## 14. HVM-SFI Client

The kernel communicates with firmware via HVM-SFI for services that cannot be performed in S-mode:

```c
// sfi.c — HVM Supervisor Firmware Interface client

#define SFI_CALL(func_id, a0, a1, a2, a3, a4, a5)  \
    ({ \
        uint64_t _ret; \
        asm volatile( \
            "mov r8, %1\n"   /* function ID in a6 */ \
            "mov r1, %2\n"   /* a0 */ \
            "mov r2, %3\n"   /* a1 */ \
            "mov r3, %4\n"   /* a2 */ \
            "mov r5, %5\n"   /* a3 */ \
            "mov r6, %6\n"   /* a4 */ \
            "mov r7, %7\n"   /* a5 */ \
            "ecall\n"        /* Trap to firmware */ \
            "mov %0, r1\n"   /* return value */ \
            : "=r"(_ret) \
            : "r"(func_id), "r"(a0), "r"(a1), "r"(a2), \
              "r"(a3), "r"(a4), "r"(a5) \
            : "r1", "r2", "r3", "r5", "r6", "r7", "r8", "memory"); \
        _ret; \
    })

// Required HVM-SFI calls
int64_t sfi_get_spec_version(void) {
    return SFI_CALL(0x0000, 0, 0, 0, 0, 0, 0);
}

void sfi_console_putchar(char c) {
    SFI_CALL(0x0002, c, 0, 0, 0, 0, 0);
}

int sfi_console_getchar(void) {
    return (int)SFI_CALL(0x0003, 0, 0, 0, 0, 0, 0);
}

void sfi_timer_set(uint64_t deadline) {
    SFI_CALL(0x0010, deadline, 0, 0, 0, 0, 0);
}

void sfi_ipi_send(uint64_t hart_mask) {
    SFI_CALL(0x0020, hart_mask, 0, 0, 0, 0, 0);
}

__attribute__((noreturn))
void sfi_system_shutdown(void) {
    SFI_CALL(0x0031, 0, 0, 0, 0, 0, 0);
    __builtin_unreachable();
}
```

---

## 15. Process ELF Loading

### 15.1 ELF64 Loader

```c
// elf.c — ELF64 binary loader

#include <kernel/elf.h>

#define ELF_MAGIC      0x464C457F  // "\x7fELF"
#define EM_HVM         0x9999      // Assigned machine ID for HVM (example)

int elf_load(task_t *task, const void *binary, size_t size) {
    elf64_hdr_t *hdr = (elf64_hdr_t *)binary;

    // Validate ELF header
    if (*(uint32_t *)binary != ELF_MAGIC) return -ENOEXEC;
    if (hdr->e_machine != EM_HVM) return -ENOEXEC;
    if (hdr->e_type != ET_EXEC) return -ENOEXEC;

    // Load each program segment
    elf64_phdr_t *phdr = (elf64_phdr_t *)((uint8_t *)binary + hdr->e_phoff);
    for (int i = 0; i < hdr->e_phnum; i++) {
        if (phdr[i].p_type != PT_LOAD) continue;

        uint64_t vaddr = phdr[i].p_vaddr;
        uint64_t memsz = phdr[i].p_memsz;
        uint64_t filesz = phdr[i].p_filesz;
        uint64_t offset = phdr[i].p_offset;

        // Allocate and map pages
        uint64_t pages = PAGE_ALIGN(memsz) / PAGE_SIZE;
        for (uint64_t j = 0; j < pages; j++) {
            uint64_t phys = (uint64_t)pmm_alloc_zeroed_page();
            uint64_t page_vaddr = vaddr + j * PAGE_SIZE;
            uint64_t flags = PTE_USER(PTE_RW);
            if (phdr[i].p_flags & PF_X) flags |= PTE_X;
            as_map(task->addr_space, page_vaddr, phys, PAGE_SIZE, flags);
        }

        // Copy file data
        memcpy((void *)vaddr, (uint8_t *)binary + offset, filesz);
    }

    // Set entry point
    task->entry_point = hdr->e_entry;
    task->pc = hdr->e_entry;

    // Set up user stack
    task->stack_top = USER_STACK_TOP;
    // Map a stack area (e.g., 8 pages)
    for (uint64_t i = 0; i < 8; i++) {
        uint64_t phys = (uint64_t)pmm_alloc_zeroed_page();
        as_map(task->addr_space, USER_STACK_TOP - PAGE_SIZE * (i + 1),
               phys, PAGE_SIZE, PTE_USER(PTE_RW));
    }

    // Set up initial registers
    task->regs[31] = USER_STACK_TOP;  // sp

    return 0;
}
```

### 15.2 execve System Call

```c
int64_t sys_execve(task_t *task, uint64_t path_ptr, uint64_t argv_ptr,
                    uint64_t envp_ptr, ...) {
    char path[256];
    if (copy_from_user(path, path_ptr, 256)) return -EFAULT;

    // Read the binary from the file system
    file_desc_t *file = file_open(path, O_RDONLY);
    if (!file) return -ENOENT;

    // Allocate new address space
    address_space_t *new_as = as_create();

    // Read ELF into memory
    uint8_t *buffer = vmalloc(file->inode->size);
    file_read(file, buffer, file->inode->size);
    file_close(file);

    // Load ELF into new address space
    task_t temp = { .addr_space = new_as };
    int ret = elf_load(&temp, buffer, file->inode->size);
    vfree(buffer);
    if (ret < 0) {
        as_destroy(new_as);
        return ret;
    }

    // Free old address space
    as_destroy(task->addr_space);
    task->addr_space = new_as;

    // Set up user arguments, environment, and auxiliary vector
    setup_user_stack(task, argv_ptr, envp_ptr);

    task->pc = temp.entry_point;
    task->regs[31] = USER_STACK_TOP;
    task->regs[29] = temp.entry_point;  // lr points to _start
    task->sstatus = SSTATUS_SIE;        // U-mode, interrupts on

    return 0;
}
```

---

## 16. Signal and Trap Delivery

### 16.1 Signal Framework

```c
// signal.h — Signal delivery

#define SIGSEGV    11
#define SIGILL     4
#define SIGFPE     8
#define SIGKILL    9
#define SIGTERM   15
#define SIGUSR1   10
#define SIGUSR2   12
#define SIGCHLD   17

typedef struct signal_action {
    uint64_t handler;     // 0 = SIG_DFL, 1 = SIG_IGN, else handler addr
    uint64_t flags;
    uint64_t mask;
} signal_action_t;

typedef struct signal_frame {
    uint64_t regs[32];
    uint64_t pc;
    uint64_t sstatus;
    uint64_t scause;
    uint64_t stval;
    uint64_t signal_number;
    uint64_t siginfo;
    // ... trampoline code for sigreturn
} signal_frame_t;

int  signal_deliver(task_t *task, int signum, uint64_t code);
void signal_setup_frame(task_t *task, int signum);
void sigreturn_handler(uint64_t *frame);
```

### 16.2 Signal Delivery

When a signal is delivered to a user process:

1. Push a `signal_frame_t` onto the user stack.
2. Modify the trap frame so that returning from the trap runs the signal handler.
3. The handler executes; on completion it calls `sigreturn`.

```c
int signal_deliver(task_t *task, int signum, uint64_t code) {
    signal_action_t *action = &task->signal_actions[signum];
    if (action->handler == 1) return 0;  // SIG_IGN
    if (action->handler == 0) {
        // SIG_DFL — terminate or ignore
        switch (signum) {
        case SIGSEGV: case SIGILL: case SIGFPE:
        case SIGKILL: case SIGTERM:
            task_exit(task, signum);
            return 0;
        default:
            return 0;  // Ignore
        }
    }

    // Set up signal frame on user stack
    uint64_t sp = task->regs[31];
    sp -= sizeof(signal_frame_t);
    signal_frame_t *frame = (signal_frame_t *)sp;

    // Save current context
    memcpy(frame->regs, task->regs, 32 * sizeof(uint64_t));
    frame->pc = task->pc;
    frame->sstatus = task->sstatus;
    frame->scause = code;
    frame->stval = 0;
    frame->signal_number = signum;

    // Modify return context to call signal handler
    task->regs[31] = sp;                            // New sp
    task->regs[1] = signum;                          // a0 = signal number
    task->pc = action->handler;                      // Handler address
    task->regs[29] = signal_trampoline;              // Return from handler

    return 0;
}
```

---

## 17. Performance and Production Considerations

### 17.1 Production Checklist

| Area | Requirement |
| :--- | :--- |
| **Locking** | Every shared data structure is protected by a spinlock or mutex. No deadlocks in interrupt context. |
| **Preemption** | The kernel is preemptible where practical. Spinlocks disable preemption on the local core. |
| **Watchdog** | A timer-based watchdog detects stuck interrupts or scheduler stalls. |
| **OOM handling** | When memory runs out, the kernel reclaims clean pages, kills the worst offender, or panics with diagnostics. |
| **Stack guards** | Kernel stacks have guard pages. Overflow triggers a page fault, not silent corruption. |
| **Interrupt nesting** | HPIC supports interrupt priorities. The kernel can handle higher-priority interrupts while servicing lower ones. |
| **RCU** | Read-copy-update for read-mostly data structures (routing tables, mount namespace). |
| **Perf counters** | `RDPROF` or software counters for IRQ rates, context switches, page faults, scheduler latency. |
| **Tracing** | Trace points for context switches, IRQ entry/exit, page faults, syscall entry/exit. |
| **Kernel ASLR** | Randomise the kernel virtual address at boot if the bootloader supports it. |
| **Stack canaries** | Function prologues check stack canaries against corruption. |

### 17.2 Optimisation Techniques

```text
1. Lazy TLB invalidation: Use ASIDs to avoid flushing on context switch.
2. Huge pages: Map kernel direct map with 2 MB or 1 GB pages.
3. Copy-on-write: Fork uses COW; physical pages are shared until written.
4. Per-CPU page colouring: Allocate pages from the local NUMA node first.
5. Batch TLB shootdown: Collect pending invalidations; flush once.
6. Interrupt coalescing: HPIC can batch multiple IRQs into a single dispatch.
7. Zero-copy I/O: User buffers are mapped directly for DMA (with ownership protocol).
8. Fast syscall path: Use the ECALL path; minimise register save/restore.
```

### 17.3 NUMA Considerations

For HVM-S1 server profiles:

```c
// numa.h — NUMA topology

typedef struct numa_node {
    uint64_t    node_id;
    uint64_t    mem_start;
    uint64_t    mem_end;
    pmm_t       pmm;              // Per-node physical allocator
    struct task *idle_task;
    struct task *current_task;
} numa_node_t;

numa_node_t *numa_node_of_phys(uint64_t phys_addr);
numa_node_t *numa_node_of_cpu(uint64_t cpu_id);
```

---

## 18. Testing and Validation

### 18.1 Testing Strategy

| Test Level | Scope | Tool / Method |
| :--- | :--- | :--- |
| **Unit tests** | Individual functions (page table walk, slab alloc, ELF parse) | GoogleTest / custom test harness on host + HVM simulator |
| **Kernel smoke tests** | Boot, trap, syscall, context switch | HVM simulator with test kernel |
| **Stress tests** | SMP, memory pressure, IRQ storms | `stress --cpu 64 --vm 4 --io 16` |
| **Fault injection** | Page alloc failure, disk read error, bad syscall args | Simulator fault injection |
| **Boot tests** | Each platform profile boots to `init` | `hvm-sim run --machine hvm-*` |
| **POSIX compliance** | LTP subset for HVM syscalls | Linux Test Project (ported) |
| **Stability** | 24+ hour runs with mixed workload | HVM simulator in CI |

### 18.2 Simulator Integration

```bash
# Boot with trace enabled
hvm-sim run --machine hvm-mobile \
    --kernel build/kernel.elf \
    --initrd build/initrd.cpio \
    --trace cpu.exec,cpu.exception,mmio.*,irq.* \
    --trace-output kernel_trace.json

# Debug with GDB stub
hvm-sim run --machine hvm-desktop \
    --kernel build/kernel.elf \
    --gdb tcp:1234

# Dump ABI for validation
hvm-sim run --machine hvm-server \
    --kernel build/kernel.elf \
    --abi-dump server_abi.json

# Fault injection testing
hvm-sim run --machine hvm-robot \
    --kernel build/kernel.elf \
    --inject-fault pmm.fail_rate=0.001
```

### 18.3 Kernel Self-Tests

Include compile-time and boot-time self-tests:

```c
// selftest.c — Kernel self-tests

static void selftest_page_table_walk(void) {
    // Create a test page table
    pte_t pgd[512] __attribute__((aligned(PAGE_SIZE)));
    // ... map known entries
    uint64_t result = mmu_walk(pgd, 0x80001000);
    ASSERT(result == 0x80001000);  // Identity-mapped
    log_puts("  [PASS] page_table_walk\n");
}

static void selftest_spinlock(void) {
    spinlock_t lock = 0;
    spin_lock(&lock);
    ASSERT(lock == 1);
    spin_unlock(&lock);
    ASSERT(lock == 0);
    log_puts("  [PASS] spinlock\n");
}

void selftests_run(void) {
    log_puts("Running kernel self-tests...\n");
    selftest_page_table_walk();
    selftest_spinlock();
    selftest_slab_alloc();
    selftest_context_switch();
    selftest_syscall();
    selftest_elf_loader();
    log_puts("All self-tests passed.\n");
}
```

---

## 19. Platform Profiles

### 19.1 HVM-M1 Mobile

| Parameter | Value |
| :--- | :--- |
| CPU count | 6 (2 big + 4 little) |
| DRAM | LPDDR5X, 8–16 GB |
| Storage | UFS |
| Display | Framebuffer + MIPI-DSI |
| Input | Touchscreen |
| Firmware | U-Boot SPL → U-Boot → kernel |
| Power states | Active, idle, suspend-to-RAM |

### 19.2 HVM-D1 Desktop

| Parameter | Value |
| :--- | :--- |
| CPU count | 8 big cores |
| DRAM | DDR5 UDIMM, 16–64 GB |
| Storage | NVMe |
| PCIe | Root complex with ECAM, MSI interrupts |
| Display | Framebuffer or dedicated GPU |
| Input | USB keyboard/mouse |
| Firmware | coreboot → EDK II → kernel |

### 19.3 HVM-S1 Server

| Parameter | Value |
| :--- | :--- |
| CPU count | 16–128 cores per socket |
| DRAM | DDR5 ECC RDIMM, 256 GB–2 TB, NUMA |
| Storage | Multiple NVMe/U.2 |
| PCIe | Multi-root, SR-IOV, ATS |
| BMC | OpenBMC with Redfish/IPMI |
| RAS | ECC, machine check, thermal telemetry |
| Firmware | coreboot/LinuxBoot → EDK II → kernel/hypervisor |

### 19.4 HVM-R1 Robotics

| Parameter | Value |
| :--- | :--- |
| CPU count | 2 RT cores + 2 app cores |
| RT SRAM | ECC-protected, deterministic |
| App DRAM | LPDDR5X, 4–8 GB |
| Fieldbus | CAN-FD, PWM, ADC, QEI |
| Safety | Watchdog, lockstep, safe-state latch |
| Firmware | Safety FSBL → U-Boot → RTOS + Linux |

### 19.5 Platform Feature Detection

```c
// features.c — Runtime platform feature detection

typedef struct hvm_features {
    bool hvm_c;        // Compressed instructions
    bool hvm_arc;      // Retain/release atomics
    bool hvm_l;        // Hardware loops
    bool hvm_mem;      // Pair load/store, prefetch
    bool hvm_v;        // Vector extension
    bool hvm_a;        // Accelerator doorbell
    bool hvm_alloc;    // ALLOC.BUMP
    bool hvm_prof;     // RDPROF performance counters
    bool hvm_nz;       // Null-checking loads
    bool hvm_cap;      // Capability/bounds checking
    uint64_t vlen;     // VLEN in bits (0 if no vector)
    uint64_t cpu_count;
    uint64_t cpu_freq; // Hz
} hvm_features_t;

hvm_features_t features;

void feature_detect(void) {
    // Read feature0 CSR for base and green-compute extensions
    uint64_t feat = csr_read(CSR_FEATURE0);
    features.hvm_arc  = !!(feat & (1ULL << 0));
    features.hvm_l    = !!(feat & (1ULL << 1));
    features.hvm_v    = !!(feat & (1ULL << 2));
    features.hvm_alloc = !!(feat & (1ULL << 3));
    features.hvm_c    = !!(feat & (1ULL << 4));

    // Probe VLEN
    if (features.hvm_v) {
        // VSETVL with maximum AVL, then read vl
        asm volatile("vsetvl r0, %0, r0" : : "r"(-1ULL));
        uint64_t vl;
        asm volatile("mov %0, vl" : "=r"(vl));
        // VLEN can be derived from the number of elements times element width
        features.vlen = vl * 64;  // Assuming vtype = 64-bit elements
    }

    // Read CPU topology
    features.cpu_count = sfi_get_cpu_count();
    log_printf("HVM features: %s%s%s%s%s, VLEN=%llu, CPUs=%llu\n",
               features.hvm_c ? "C " : "",
               features.hvm_arc ? "ARC " : "",
               features.hvm_l ? "L " : "",
               features.hvm_v ? "V " : "",
               features.hvm_alloc ? "ALLOC " : "",
               features.vlen, features.cpu_count);
}
```

---

## 20. References

- [HVM Specification](hvm-spec.md) — ISA reference, CSRs, privilege model, MMU
- [HVM System Memory Map and ABI](system/12-hvm-memory-map-and-abi.md) — Boot ABI, calling convention, physical map, device discovery
- [HVM Freestanding Library Guide](hvm-freestanding-lib.md) — Foundation library for kernel development
- [HVM Instruction Set CSV](hvm_instruction_set.csv) — All instruction encodings
- [HVM Register Set CSV](hvm_register_set.csv) — Register definitions
- [HVM Object File Format](ho-file-format.md) — ELF variant for HVM binaries
- [HVM Simulator Design](system/10-hvm-lightweight-system-simulator-design.md) — Simulator architecture
- [HVM Firmware and BIOS Options](system/09-hvm-open-source-firmware-bios-options.md) — Boot firmware ecosystem
- [HVM Platform Specifications](system/04-platform-specifications-readme.md) — Hardware profiles overview
- [HVM Memory Map and Platform ABI](system/12-hvm-memory-map-and-abi.md) — Detailed ABI and memory layout
