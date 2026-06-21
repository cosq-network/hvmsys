# HVM Object File Format (HO)

Version: `1.5`
Extension: `.ho`  
Endianness: little-endian only

This document is the normative binary format for `src/hvm/HOModule.h` and `src/hvm/HOModule.cpp`.
It is aligned with the current hardware-ready core ISA profile in `docs/hvm/hvm-spec.md`.

## 1. File Kinds

- `0x01` Executable
- `0x02` Shared Object
- `0x03` Object File

## 2. Top-Level Layout

1. Fixed header (`64` bytes)
2. Section table (`40 * section_count` bytes)
3. Section payloads (aligned per section entry)

## 3. Header (64 bytes)

All fields are little-endian.

| Offset | Size | Field | Description |
|---|---:|---|---|
| `0x00` | 4 | `magic` | `0x484F4F43` (`"HOO"`) |
| `0x04` | 2 | `version_major` | format major (`1`) |
| `0x06` | 2 | `version_minor` | format minor (`5`) |
| `0x08` | 1 | `file_type` | executable/shared/object |
| `0x09` | 1 | `target_arch` | `0x00` x86_64, `0x01` arm64, `0xFF` any |
| `0x0A` | 1 | `endianness` | must be `0x01` |
| `0x0B` | 1 | `pointer_size` | currently `8` |
| `0x0C` | 4 | `flags` | module feature flags |
| `0x10` | 8 | `entry_point` | RVA for executables |
| `0x18` | 8 | `base_address` | preferred base |
| `0x20` | 8 | `section_count` | number of section table entries |
| `0x28` | 8 | `symtab_offset` | reserved/compat metadata offset |
| `0x30` | 4 | `symtab_entry_count` | symbol count |
| `0x34` | 4 | `reloc_count` | relocation count |
| `0x38` | 4 | `export_count` | export count |
| `0x3C` | 4 | `import_count` | import count |

Notes:
- Parser validates magic, header size, section table bounds, and little-endian mode.
- `symtab_offset` is written by serializer and kept for compatibility, but parser discovers metadata via section table.
- HVM 1.5 modules remain 64-bit. `pointer_size` must be `8` for native HVM64 code.
- HVM 1.4 readers must reject 1.5 modules unless they explicitly opt into forward-compatible parsing of unknown feature flags.

### 3.1 Module Feature Flags

The 32-bit `flags` field advertises required instruction/profile features used by `.text`. A loader must reject a module if any required bit is set and the target CPU/simulator does not expose the corresponding HVM feature.

| Bit | Name | Meaning |
|---:|---|---|
| 0 | `HVM_FLAG_HVM_C` | Module may contain HVM-C compressed encodings |
| 1 | `HVM_FLAG_HVM_ARC` | Module may contain `RETAIN` or `RELEASE` |
| 2 | `HVM_FLAG_ICACHE_RNG` | Module may contain `ICACHE.RNG` |
| 3 | `HVM_FLAG_HVM_L` | Module may contain HVM-L hardware-loop instructions |
| 4 | `HVM_FLAG_HVM_MEM` | Module may contain pair memory operations or memory hints |
| 5 | `HVM_FLAG_HVM_V` | Module may contain HVM-V vector instructions |
| 6 | `HVM_FLAG_HVM_A` | Module may contain HVM-A accelerator doorbell instructions |
| 7 | `HVM_FLAG_HVM_ALLOC` | Module may contain `ALLOC.BUMP` |
| 8 | `HVM_FLAG_HVM_OBJREF` | Module expects compact object-reference runtime support |
| 9 | `HVM_FLAG_HVM_CAP` | Module may contain capability/bounds-check instructions |
| 10 | `HVM_FLAG_HVM_PROF` | Module may contain `RDPROF` |
| 11 | `HVM_FLAG_HVM_NZ` | Module may contain null-checking load instructions |
| 12 | `HVM_FLAG_HVM_RT` | Module is built for the deterministic RT subset |

Bits 13-31 are reserved and must be zero until assigned.

## 4. Section Entry (40 bytes)

Each section table entry is exactly `40` bytes.

| Offset | Size | Field |
|---|---:|---|
| `0x00` | 8 | `name_offset` (offset into `.strtab`) |
| `0x08` | 4 | `section_type` |
| `0x0C` | 4 | `flags` |
| `0x10` | 8 | `virtual_size` |
| `0x18` | 8 | `file_offset` (`0` for BSS/no payload) |
| `0x20` | 8 | `alignment` |

## 5. Section Types

These IDs match `enum class SectionType` in `src/hvm/HOModule.h`.

| ID | Name |
|---:|---|
| `0x01` | `SHT_NULL` |
| `0x02` | `SHT_TEXT` |
| `0x03` | `SHT_RODATA` |
| `0x04` | `SHT_DATA` |
| `0x05` | `SHT_BSS` |
| `0x06` | `SHT_SYMTAB` |
| `0x07` | `SHT_STRTAB` |
| `0x08` | `SHT_RELOC` |
| `0x09` | `SHT_EXPORT` |
| `0x0A` | `SHT_IMPORT` |
| `0x0B` | `SHT_FUNCMETA` |
| `0x0C` | `SHT_TYPES` |
| `0x0D` | `SHT_NOTE` |
| `0x0E` | `SHT_TLS` |
| `0x0F` | `SHT_DEBUG_LINE` |
| `0x10` | `SHT_DEBUG_INFO` |
| `0x11` | `SHT_DEBUG_ABBREV` |
| `0x12` | `SHT_DEBUG_STR` |
| `0x13` | `SHT_DEBUG_FRAME` |
| `0x14` | `SHT_DEBUG_LOC` |
| `0x15` | `SHT_DEBUG_RANGES` |
| `0x16` | `SHT_DEBUG_MACINFO` |
| `0x17` | `SHT_GROUP` |

## 6. Section Flags

Bit masks from `SectionFlags`:

- `0x8000` `TLS`
- `0x4000` `ALLOC`
- `0x2000` `WRITE`
- `0x1000` `EXECUTE`
- `0x0800` `MERGE`
- `0x0400` `STRINGS`
- `0x0200` `EXCLUDE`
- `0x0100` `COMPRESSED`

## 7. Metadata Table Entry Layouts

### 7.1 Symbol (`32` bytes)

| Offset | Size | Field |
|---|---:|---|
| `0x00` | 4 | name offset (`.strtab`) |
| `0x04` | 1 | binding |
| `0x05` | 1 | type |
| `0x06` | 1 | visibility |
| `0x07` | 1 | reserved |
| `0x08` | 8 | value |
| `0x10` | 8 | size |
| `0x18` | 4 | section index (signed) |
| `0x1C` | 4 | symbol index |

### 7.2 Relocation (`16` bytes)

| Offset | Size | Field |
|---|---:|---|
| `0x00` | 8 | offset |
| `0x08` | 4 | symbol index |
| `0x0C` | 2 | relocation type |
| `0x0E` | 2 | addend (signed) |

### 7.3 Export (`24` bytes)

| Offset | Size | Field |
|---|---:|---|
| `0x00` | 4 | name offset |
| `0x04` | 4 | symbol index |
| `0x08` | 8 | address |
| `0x10` | 8 | size |

### 7.4 Import (`32` bytes)

| Offset | Size | Field |
|---|---:|---|
| `0x00` | 4 | name offset |
| `0x04` | 4 | library name offset |
| `0x08` | 4 | import type |
| `0x0C` | 4 | version |
| `0x10` | 8 | flags |
| `0x18` | 8 | resolved address |

### 7.5 Function Metadata (`48` bytes)

| Offset | Size | Field |
|---|---:|---|
| `0x00` | 4 | name offset |
| `0x04` | 4 | symbol index |
| `0x08` | 8 | entry RVA |
| `0x10` | 4 | code size |
| `0x14` | 4 | local size |
| `0x18` | 4 | param count |
| `0x1C` | 4 | param types offset |
| `0x20` | 4 | return type offset |
| `0x24` | 4 | flags |
| `0x28` | 4 | source line |
| `0x2C` | 4 | debug offset |

## 8. String Table Rules (`.strtab`)

- First byte is `NUL` (`'\0'`) so offset `0` is valid empty string.
- Names in section table and metadata entries are offsets into `.strtab`.
- If a section name lookup fails, parser may fall back to built-in defaults for known section types.

## 9. Serialization Behavior (Current Implementation)

- Serializer auto-generates metadata sections from in-memory vectors:
  - `.symtab`, `.reloc`, `.export`, `.import`, `.funcmeta`, `.strtab`
- User-supplied payload bytes for those metadata sections are rejected.
- Non-BSS sections with payload are laid out at aligned offsets and copied into file.

## 10. Parsing and Validation Guarantees

`HOModule` parser rejects:

- invalid magic
- non-little-endian files
- truncated header/section table
- overflowed table sizes
- out-of-range section payload spans
- malformed metadata section sizes (not divisible by entry size)

## 11. Relationship to HVM ISA

- `.text` carries encoded HVM instructions.
- HVM 1.5 `.text` may contain base32 and escape32 instructions from `docs/hvm/hvm_instruction_set.csv`.
- Optional v1.5 extensions must be reflected in the header `flags` field.
- Supported language/runtime surface is defined by:
  - `docs/hvm/hvm-spec.md`
  - `docs/hvm/hvm_instruction_set.csv`
  - `docs/hvm/hvm_register_set.csv`
- This file-format spec intentionally does not re-list opcode families to avoid drift.
