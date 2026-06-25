# HVMC Compiler Roadmap

This document is the phased implementation roadmap for `hvmc` — the HVM C compiler and assembler toolchain. The goal is to evolve `hvmc` from its current prototype state into a **production-grade, self-hosting C compiler** capable of building a freestanding library and a full ELF-format kernel for HVM boards and CPUs.

## Target Output

The primary deliverable is an `hvmc` toolchain that can compile a C freestanding library (as described in `docs/hvm-freestanding-lib.md`) and link it into a bootable ELF64-HVM kernel image. The compiler must emit correct HVM instructions, follow the HVM ABI (calling convention, register usage, stack frame layout), and produce valid ELF executables with proper program headers.

---

## Phase 0: Completed — Compiler Scaffold

The current codebase provides a working compiler skeleton with the following implemented:

### Completed

| Area | Feature | Notes |
| :--- | :--- | :--- |
| **Build** | Static library target `hvmc_lib` + executable `hvmc` | CMake build, links `fmt` |
| **CLI** | Flag parser: `-o`, `-S`, `-c`, `-E`, `-I`, `-v`, `--help` | `main.cpp` |
| **Instruction encoder** | All HVM opcodes encoded (R, I, B, J, RI formats; base32 + escape32) | `encoder.cpp`, ~70 opcodes |
| **Assembler** | Two-pass assembler with labels, sections, `.byte`/`.word`/`.quad`/`.ascii`/`.asciz`/`.align`, `.global` | `assembler.cpp` |
| **ELF writer** | ELF64-HVM object files (`ET_REL`) with `.symtab`, `.strtab`, `.shstrtab`, `.rela.text` | `elf.cpp`, `EM_HVM = 0x9999` |
| **C lexer** | Full tokenizer: identifiers, numbers, strings, char literals, operators, keywords, `asm{}` blocks | `lexer.cpp` |
| **C parser** | Expression parsing (precedence climbing), statements (if/while/for/do-while/return/break/continue/asm), variable/function declarations | `parser.cpp` |
 | **C codegen** | AST-to-assembly generation for expressions, control flow, function calls (improved) | `codegen.cpp` |
| **Preprocessor** | `#define`, `#undef`, `#include`, `#if`/`#ifdef`/`#ifndef`/`#else`/`#elif`/`#endif`, `#error`, `#pragma` | `preprocessor.cpp` |
| **Unit tests** | Encoder tests (all formats), assembler tests (sections, labels, globals), ELF writer tests (header, magic, machine type) | `test_hvmc.cpp`, ~31 tests |

---

## Phase 1: C Frontend Fixes — 🔶 PARTIALLY COMPLETE

This phase fixes the critical bugs in the existing C frontend so that the compiler can actually parse and compile real C code.

### 1.1 Fix Function Definition Parsing — ✅ DONE

**Bug:** `parse_function_def` in `parser.cpp:130` expects `;` after the parameter list, treating `int main() { ... }` as a declaration. The `{` block becomes orphaned and the function body is never captured.

**Work:**
- [x] Detect `{` vs `;` after `)` to distinguish declarations from definitions
- [x] Parse the function body as a `BLOCK` and attach it to `FUNCTION_DEF`
- Structure declarations (`struct foo;`) should still be handled as forward declarations

### 1.2 Fix Codegen for Variables — Stack Frame Model

**Problem:** Codegen emits `mov r1, var_name` for variable references, which is not a valid HVM instruction. There is no stack frame awareness.

**Work:**
- Implement proper HVM stack frame management using `ENTER`/`LEAVE`
- Track local variable offsets from `fp` (`r30`)
- Emit correct `ld.d rd, rs1, imm` / `st.d rd, rs1, imm` for variable access
- Assign all locals at negative offsets from `fp`, arguments at positive offsets from `fp`
- Use `ADJSP` for dynamic stack adjustments

### 1.3 Fix Codegen for Assignment

**Bug:** `gen_expression` for `ASSIGN` emits `mov r1, r1` — both target and value resolve to `r1`.

**Work:**
- Generate correct store instruction: compute address of the LHS, compute RHS value into a register, then `st.d value, addr_base, offset`
- Handle `*ptr = expr` (assignment through pointer)

### 1.4 Fix Codegen for Pointer Dereference

**Bug:** Codegen emits `ld rN, [rN]` which is not valid HVM assembly syntax.

**Work:**
- Emit `ld.d rd, rs1, 0` for load from address in `rs1`
- Handle pointer arithmetic correctly (scaled by pointee size)

### 1.5 Fix Codegen for Address-Of

**Bug:** Codegen emits `mov rN, $name` — `$` prefix is not valid assembler syntax.

**Work:**
- Compute the actual stack or global address using `LDA` (load address)
- Emit `lda rd, zero, offset(fp)` for locals or `lda rd, zero, global_addr` for globals

### 1.6 Fix String Literal Codegen

**Bug:** String literals are emitted inline in `.text` section with a `.string` directive embedded mid-code, which produces invalid assembly.

**Work:**
- Collect string literals and emit them in `.rodata` section
- Generate proper labels and `LDA` to load their address

### 1.7 Fix Executable ELF Output — ✅ DONE

**Bug:** `write_executable` sets `ET_EXEC` but emits no program headers (`phoff = 0`, `phnum = 0`).

**Work:**
- [x] Generate proper `PT_LOAD` program headers mapping `.text`, `.rodata`, `.data`, `.bss`
- [x] Set correct `p_vaddr`, `p_paddr`, `p_filesz`, `p_memsz`, `p_flags`, `p_align`
- [x] Set entry point from `_start` symbol

### Acceptance Criteria

- [x] `hvmc -c file.c` produces a valid ELF64-HVM object file
- [x] `hvmc file.c -o file.elf` produces a valid ELF64-HVM executable with program headers (program headers done)
- [x] Function definitions with `{` bodies parse correctly
- [ ] Object file can be linked (external linker) or `hvmc` handles the link step
- [ ] Simple C functions compile and produce valid HVM assembly that assembles correctly

---

## Phase 2: ABI Compliance and Code Quality

This phase makes the compiler generate correct, ABI-compliant code suitable for building a freestanding library.

### 2.1 HVM ABI Calling Convention

- Implement the HVM calling convention per `docs/system/12-hvm-memory-map-and-abi.md`:
  - Arguments 1–6 in `r1`–`r3`, `r5`–`r8` (a0–a6)
  - 7th+ arguments on stack
  - Return value in `r1` (a0)
  - Callee-saved: `r16`–`r28` (s0–s12), `r30` (fp), `r31` (sp)
  - Caller-saved: `r1`–`r3`, `r5`–`r15` (a0–a6, t0–t6), `r29` (lr)
- Generate proper function prologue (`ENTER imm`) and epilogue (`LEAVE`; `RET`)
- Save/restore callee-saved registers used by the function

### 2.2 Register Allocation

- Implement a simple register allocator (linear scan or basic local allocator)
- Track live registers and spill to stack when necessary
- Use all 32 registers effectively
- Special handling for `r0` (zero register)

### 2.3 Data Type Support

- Implement correct type sizes per HVM LP64 data model:
  - `char` = 1 byte, `short` = 2, `int` = 4, `long` = 8, pointer = 8
- Sign-extension for `char`/`short` loads (`ld.b`/`ld.bu`/`ld.h`/`ld.hu`)
- Promote small integer types per C rules
- Implement `float`/`double` support using HVM floating-point instructions (`FADD`, `FSUB`, `FMUL`, `FDIV`, `FCMP*`)

### 2.4 Global and Static Variables

- Allocate `.data` section entries for initialized globals
- Allocate `.bss` entries for zero-initialized globals
- Emit correct `.global` / `.local` directives
- Generate correct absolute addressing for global references

### 2.5 Array Support

- Proper array indexing with scaled index (`SHL` by element size)
- Multi-dimensional array support
- Array decay to pointer in expressions

### Acceptance Criteria

- A freestanding C function using the HVM ABI can be compiled and linked
- `memset`, `memcpy`, `memmove` from `hvm-freestanding-lib.md` compile correctly
- Simple function calls with up to 6 arguments work
- Global variables in `.data`/`.bss` are correctly placed

---

## Phase 3: Missing C Language Features

This phase adds the remaining C language constructs needed for kernel code.

### 3.1 Struct and Union Support

- **Parser:** Parse struct/union body with member list
- **Codegen:** Compute struct size and member offsets (proper alignment)
- **Codegen:** Emit correct `ld.d`/`st.d` with computed offsets for `.` and `->` member access
- Handle nested structs, struct assignment, struct passing by value

### 3.2 Switch Statement

- **Parser:** Parse `switch`/`case`/`default` (tokens already defined but not handled)
- **Codegen:** Generate jump table or chained compare-and-branch
- Handle fall-through, `break`, `default`

### 3.3 Goto Statement

- **Parser:** Parse `goto label;` (token already defined)
- **Codegen:** Emit `jmp` with label resolution
- Handle labeled statements

### 3.4 Enum Support

- **Parser:** Parse `enum` type definitions
- **Codegen:** Treat enum constants as integer constants

### 3.5 Sizeof Operator

- **Parser:** `sizeof(type)` (currently only `sizeof(expr)`)
- Implement compile-time evaluation of `sizeof`

### 3.6 Comma Operator

- Evaluate left-to-right, result is the right operand

### 3.7 Conditional Expression (Ternary)

- `expr ? expr : expr` — generate branch sequence

### Acceptance Criteria

- struct types compile with correct field access
- `switch` statements generate correct branch logic
- `goto` works within function scope
- `sizeof` evaluates correctly for all types
- The entire freestanding library from `hvm-freestanding-lib.md` compiles without errors

---

## Phase 4: Linker and ELF Production

This phase adds linking support so `hvmc` can produce final executables without requiring an external linker.

### 4.1 Internal Linker

- Consolidate multiple object files into one executable
- Resolve symbol references across compilation units
- Handle `R_HVM_64`, `R_HVM_PC32`, `R_HVM_PC64`, `R_HVM_BRANCH`, `R_HVM_JAL`, `R_HVM_CALL`, `R_HVM_ABS32`, `R_HVM_ABS64` relocations
- Report undefined symbols

### 4.2 Linker Script Support

- Accept `-T` linker script flag
- Parse minimal linker scripts (or provide a default/embedded script)
- Support `OUTPUT_ARCH(hvm)`, `OUTPUT_FORMAT(elf64-littlehvm)`, `ENTRY(_start)`
- Support `SECTIONS { .text : { *(.text*) } ... }` layout
- Support `ALIGN`, `AT`, `PROVIDE` directives

### 4.3 Default Linker Layout

Provide a built-in default linker layout matching `hvm-kernel-development.md`:
- `PHYS_BASE = 0x80000000`, `KERNEL_OFFSET = 0x100000`
- `.text`, `.rodata`, `.data`, `.bss` sections at 4K alignment
- Boot stack (16K) and kernel stack (64K) symbols
- `__text_start`, `__text_end`, `__bss_start`, `__bss_end`, `__end` symbols

### 4.4 Program Headers for Executables

- Generate `PT_LOAD` segments covering `.text`, `.rodata`, `.data`+`.bss`
- Generate `PT_GNU_STACK` (NX bit) segment
- Generate `PT_HVM_*` segments for platform-specific needs if defined

### Acceptance Criteria

- `hvmc -c a.c -o a.o && hvmc -c b.c -o b.o && hvmc a.o b.o -o kernel.elf` produces a linked executable
- Program headers are present and correct
- Symbols are resolved across translation units
- The linked kernel boots in the HVM simulator

---

## Phase 5: Optimization

This phase improves code quality for production use.

### 5.1 Peephole Optimizations

- Remove redundant `mov rX, rX` instructions
- Fold constant loads into immediate operands where possible
- Eliminate dead stores
- Optimize `addi rd, rs, 0` → `mov rd, rs`

### 5.2 Basic Block Optimizations

- Constant folding at compile time (e.g., `2 + 3` → `5`)
- Constant propagation across basic blocks
- Jump-to-next elimination (remove `jmp` to immediately following label)

### 5.3 Tail Call Optimization

- When a function ends with `return fn(args)`, emit `TAILCALL` instead of `CALL` + `RET`

### 5.4 Inline Assembly Alignment

- Ensure `asm {}` blocks compile correctly with proper register constraints
- Support extended asm syntax (input/output/clobber lists)

### Acceptance Criteria

- Generated code size is reduced by at least 20% vs. unoptimized
- Performance of compiled code is within 2× of hand-optimized assembly
- All freestanding library functions compile with `-O2`

---

## Phase 6: Hardware and ABI Support

This phase adds HVM-specific ISA features needed for kernel and systems programming.

### 6.1 CSR Instructions

- Support `CSRRW` instruction via `__builtin_csrrw()` or `asm` blocks
- Address key kernel CSRs: `SSTATUS`, `STVEC`, `SEPC`, `SCAUSE`, `STVAL`, `SATP`, `STIME`, `STIMECMP`

### 6.2 Atomic Instructions

- Support `LR.D` / `SC.D` via builtins or intrinsic functions
- Provide `__sync_*` builtins mapping to LR/SC sequences
- Memory fence support (`SFENCE.VMA`)

### 6.3 Interrupt and Trap Support

- Ensure `ECALL`, `BREAK`, `TRAPRET` instructions are usable
- Support interrupt attribute for functions (`__attribute__((interrupt))`)

### 6.4 Inline Assembly Enhancements

- Support extended GNU C inline asm with register constraints for all HVM registers
- Support `"r"`, `"i"`, `"m"` constraints mapped to HVM addressing modes

### 6.5 Section Attributes

- Support `__attribute__((section(".name")))` to place functions/variables in specific sections
- Support `__attribute__((aligned(N)))`, `__attribute__((packed))`

### Acceptance Criteria

- Kernel trap handler from `hvm-kernel-development.md` compiles correctly
- LR/SC spinlock primitives compile and assemble correctly
- CSR access via inline assembly works
- Sections are respected for `.text.start`, `.text.trap`, `.trap*` placement

---

## Phase 7: Toolchain Integration and Testing

This phase professionalizes the compiler for daily development use.

### 7.1 Debug Information

- Generate DWARF-5 debugging information in `.debug_*` sections
- Map source locations (file, line, column) through compilation
- Support `-g` flag

### 7.2 Error Messages and Diagnostics

- Improve error messages with source context (line display, caret pointing)
- Support `-Wall`, `-Wextra`, `-Werror` with categorized warnings
- Track source location through all phases (lexer → parser → AST → codegen)

### 7.3 Comprehensive Test Suite

- **Parser tests:** Parse all C constructs, error recovery
- **Codegen tests:** Verify generated assembly for each C construct
- **Integration tests:** Compile real C programs and verify output
- **ELF tests:** Verify generated object files and executables
- **Preprocessor tests:** Macro expansion, includes, conditionals
- **Linker tests:** Multi-object linking, symbol resolution, relocation
- **ABI tests:** Verify calling convention, stack layout, register saving

### 7.4 Optimization Validation

- Differential testing: compile with `-O0` vs `-O2` and verify same behavior
- Fuzz testing with random C programs

### 7.5 Build System Improvements

- Add install target for `hvmc` binary and headers
- Package for distribution
- CI integration for all presets

### Acceptance Criteria

- Test suite covers >80% of source code
- All freestanding library + kernel code compiles without warnings with `-Wall -Wextra -Werror`
- Generated ELF binaries boot correctly in `hvm-sim`

---

## Phase 8: Self-Hosting Path

The ultimate milestone: `hvmc` can compile itself.

### 8.1 Source Portability

- Ensure `hvmc` source code uses only the C subset that `hvmc` supports
- Remove dependency on `fmt` library (or provide an HVM-compiled version)
- Remove platform assumptions (endianness, file I/O patterns)

### 8.2 Bootstrapping

- Stage 1: Use host compiler to build `hvmc`
- Stage 2: Use built `hvmc` to rebuild itself
- Stage 3: Verify stage 2 binary produces identical output to stage 1

### 8.3 Self-Hosted Toolchain

- `hvmc` runs on HVM hardware (or simulator)
- Full compilation pipeline works without any host toolchain dependency

### Acceptance Criteria

- `hvmc` compiles itself into a byte-identical binary in two stages
- The self-hosted `hvmc` can compile the freestanding library and kernel

---

## Summary

| Phase | Focus | Est. Effort | Dependencies |
| :--- | :--- | :--- | :--- |
| **0** | Compiler scaffold (done) | — | — |
| **1** | C frontend fixes | 2–3 weeks | Phase 0 |
| **2** | ABI compliance & code quality | 2–3 weeks | Phase 1 |
| **3** | Missing C features | 2–4 weeks | Phase 2 |
| **4** | Linker & ELF production | 3–4 weeks | Phase 2 |
| **5** | Optimization | 2–3 weeks | Phase 3 |
| **6** | Hardware & ABI support | 2–3 weeks | Phase 4 |
| **7** | Toolchain integration & testing | 3–4 weeks | Phase 5, 6 |
| **8** | Self-hosting path | 4–6 weeks | Phase 7 |

The **minimum viable product** for kernel development is reached at the end of **Phase 3** — at that point `hvmc` can compile a C freestanding library into a valid, loadable ELF64-HVM executable. Phase 4 (linker) is required if an external linker is not available. Phases 5–8 are production-hardening steps.
