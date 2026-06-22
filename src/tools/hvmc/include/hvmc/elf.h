#pragma once
#include "types.h"

namespace hvmc {

// ELF64-HVM constants
constexpr u16 EM_HVM = 0x9999;
constexpr u32 ELF_MAGIC = 0x464C457F; // "\x7fELF"
constexpr u8 ELFCLASS64 = 2;
constexpr u8 ELFDATA2LSB = 1;
constexpr u8 EV_CURRENT = 1;
constexpr u16 ET_REL = 1;
constexpr u16 ET_EXEC = 2;
constexpr u64 ELF_HEADER_SIZE = 64;

// Section types
constexpr u32 SHT_NULL     = 0;
constexpr u32 SHT_PROGBITS = 1;
constexpr u32 SHT_SYMTAB   = 2;
constexpr u32 SHT_STRTAB   = 3;
constexpr u32 SHT_RELA     = 4;
constexpr u32 SHT_NOBITS   = 8;

// Section flags
constexpr u64 SHF_WRITE      = 0x1;
constexpr u64 SHF_ALLOC      = 0x2;
constexpr u64 SHF_EXECINSTR  = 0x4;
constexpr u64 SHF_STRINGS    = 0x20;

// Symbol bind
constexpr u8 STB_LOCAL  = 0;
constexpr u8 STB_GLOBAL = 1;

// Symbol type
constexpr u8 STT_NOTYPE  = 0;
constexpr u8 STT_OBJECT  = 1;
constexpr u8 STT_FUNC    = 2;
constexpr u8 STT_SECTION = 3;

// ELF64 header structure (packed)
struct Elf64_Ehdr {
    u32 e_magic;
    u8  e_class;
    u8  e_data;
    u8  e_curver;
    u8  e_osabi;
    u8  e_abiver;
    u8  e_pad[7];
    u16 e_type;
    u16 e_machine;
    u32 e_version;
    u64 e_entry;
    u64 e_phoff;
    u64 e_shoff;
    u32 e_flags;
    u16 e_ehsize;
    u16 e_phentsize;
    u16 e_phnum;
    u16 e_shentsize;
    u16 e_shnum;
    u16 e_shstrndx;
};

// ELF64 section header
struct Elf64_Shdr {
    u32 sh_name;
    u32 sh_type;
    u64 sh_flags;
    u64 sh_addr;
    u64 sh_offset;
    u64 sh_size;
    u32 sh_link;
    u32 sh_info;
    u64 sh_addralign;
    u64 sh_entsize;
};

// ELF64 symbol table entry
struct Elf64_Sym {
    u32 st_name;
    u8  st_info;
    u8  st_other;
    u16 st_shndx;
    u64 st_value;
    u64 st_size;
};

// ELF64 RELA relocation entry
struct Elf64_Rela {
    u64 r_offset;
    u64 r_info;
    i64 r_addend;
};

inline u64 elf_r_info(u32 sym, u32 type) {
    return (static_cast<u64>(sym) << 32) | type;
}

// Write an ELF64-HVM object file or executable
std::vector<u8> write_elf(const ObjectFile& obj);

// Helper: create a simple executable from sections
std::vector<u8> write_executable(const ObjectFile& obj);

} // namespace hvmc
