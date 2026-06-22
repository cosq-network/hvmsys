#include "hvmc/elf.h"
#include <cstring>
#include <unordered_map>

namespace hvmc {

static void w8(std::vector<u8>& buf, u8 v) { buf.push_back(v); }
static void w16le(std::vector<u8>& buf, u16 v) { w8(buf, v & 0xFF); w8(buf, (v >> 8) & 0xFF); }
static void w32le(std::vector<u8>& buf, u32 v) { w16le(buf, v & 0xFFFF); w16le(buf, (v >> 16) & 0xFFFF); }
static void w64le(std::vector<u8>& buf, u64 v) { w32le(buf, v & 0xFFFFFFFF); w32le(buf, (v >> 32) & 0xFFFFFFFF); }

static u32 add_str(std::vector<u8>& tab, const std::string& s) {
    u32 off = static_cast<u32>(tab.size());
    tab.insert(tab.end(), s.begin(), s.end());
    tab.push_back(0);
    return off;
}

struct ElfSec {
    u32 name_off;
    u32 type;
    u64 flags;
    u64 addr;
    u64 offset;
    u64 size;
    u32 link;
    u32 info;
    u64 align;
    u64 entsize;
    std::vector<u8> data;
};

std::vector<u8> write_elf(const ObjectFile& obj) {
    std::vector<u8> shstrtab, strtab;
    add_str(shstrtab, "");

    std::vector<ElfSec> secs;
    std::unordered_map<std::string, int> sec_idx_map;

    // Section 0: NULL
    secs.push_back({0, SHT_NULL, 0, 0, 0, 0, 0, 0, 0, 0, {}});
    int idx = 1;

    // Collect section data
    std::vector<u8> text_data, data_data, rodata_data;
    std::vector<Relocation> text_relocs;

    for (const auto& s : obj.sections) {
        if (s.name == ".text") { text_data = s.data; text_relocs = s.relocs; }
        else if (s.name == ".data") data_data = s.data;
        else if (s.name == ".rodata") rodata_data = s.data;
    }

    // .text
    {
        u32 sn = add_str(shstrtab, ".text");
        secs.push_back({sn, SHT_PROGBITS, SHF_ALLOC | SHF_EXECINSTR, 0, 0, text_data.size(), 0, 0, 4, 0, text_data});
        sec_idx_map[".text"] = idx++;
    }

    // .rodata
    {
        u32 sn = add_str(shstrtab, ".rodata");
        secs.push_back({sn, SHT_PROGBITS, SHF_ALLOC, 0, 0, rodata_data.size(), 0, 0, 8, 0, rodata_data});
        sec_idx_map[".rodata"] = idx++;
    }

    // .data
    {
        u32 sn = add_str(shstrtab, ".data");
        secs.push_back({sn, SHT_PROGBITS, SHF_ALLOC | SHF_WRITE, 0, 0, data_data.size(), 0, 0, 8, 0, data_data});
        sec_idx_map[".data"] = idx++;
    }

    // .bss
    {
        u32 sn = add_str(shstrtab, ".bss");
        secs.push_back({sn, SHT_NOBITS, SHF_ALLOC | SHF_WRITE, 0, 0, 0, 0, 0, 8, 0, {}});
        sec_idx_map[".bss"] = idx++;
    }

    // Build symbol table
    std::vector<Elf64_Sym> syms;
    // Null symbol
    Elf64_Sym null_sym = {};
    syms.push_back(null_sym);

    // Section symbols
    for (int i = 1; i < static_cast<int>(secs.size()); i++) {
        Elf64_Sym ss = {};
        ss.st_info = (STB_LOCAL << 4) | STT_SECTION;
        ss.st_shndx = i;
        syms.push_back(ss);
    }

    // Global symbols
    for (const auto& gs : obj.global_symbols) {
        Elf64_Sym es = {};
        es.st_name = add_str(strtab, gs.name);
        es.st_info = ((gs.is_global ? STB_GLOBAL : STB_LOCAL) << 4) |
                     (gs.is_function ? STT_FUNC : STT_OBJECT);
        es.st_shndx = gs.section_idx > 0 ? gs.section_idx : 1;
        es.st_value = gs.value;
        es.st_size = gs.size;
        syms.push_back(es);
    }

    // .symtab
    std::vector<u8> symtab_data;
    for (const auto& s : syms) {
        w32le(symtab_data, s.st_name);
        w8(symtab_data, s.st_info);
        w8(symtab_data, s.st_other);
        w16le(symtab_data, s.st_shndx);
        w64le(symtab_data, s.st_value);
        w64le(symtab_data, s.st_size);
    }
    {
        u32 sn = add_str(shstrtab, ".symtab");
        secs.push_back({sn, SHT_SYMTAB, 0, 0, 0, symtab_data.size(), static_cast<u32>(idx), 1, 8, sizeof(Elf64_Sym), symtab_data});
        // link to strtab (will be at idx after we add it)
        secs.back().link = idx + 1; // will be fixed
        sec_idx_map[".symtab"] = idx++;
    }

    // .strtab
    {
        u32 sn = add_str(shstrtab, ".strtab");
        secs.push_back({sn, SHT_STRTAB, 0, 0, 0, strtab.size(), 0, 0, 1, 0, strtab});
        sec_idx_map[".strtab"] = idx++;
    }

    // .rela.text
    if (!text_relocs.empty()) {
        std::vector<u8> rela_data;
        for (const auto& r : text_relocs) {
            w64le(rela_data, r.offset);
            u32 sym_idx = static_cast<u32>(r.symbol);
            w64le(rela_data, elf_r_info(sym_idx, static_cast<u32>(r.type)));
            w64le(rela_data, static_cast<u64>(r.addend));
        }
        u32 sn = add_str(shstrtab, ".rela.text");
        secs.push_back({sn, SHT_RELA, 0, 0, 0, rela_data.size(), static_cast<u32>(sec_idx_map[".symtab"]), static_cast<u32>(sec_idx_map[".text"]), 8, sizeof(Elf64_Rela), rela_data});
        sec_idx_map[".rela.text"] = idx++;
    }

    // Fix symtab link to strtab
    for (auto& s : secs) {
        if (s.type == SHT_SYMTAB) {
            s.link = sec_idx_map[".strtab"];
        }
    }

    // .shstrtab
    {
        u32 sn = add_str(shstrtab, ".shstrtab");
        secs.push_back({sn, SHT_STRTAB, 0, 0, 0, shstrtab.size(), 0, 0, 1, 0, shstrtab});
        sec_idx_map[".shstrtab"] = idx++;
    }

    int shstrndx = sec_idx_map[".shstrtab"];

    // Compute file layout
    u64 offset = ELF_HEADER_SIZE;
    for (auto& sec : secs) {
        if (sec.type == SHT_NULL || sec.type == SHT_NOBITS) continue;
        if (sec.data.empty()) continue;
        u64 align = sec.align;
        if (align > 1) {
            u64 mis = offset % align;
            if (mis) offset += align - mis;
        }
        sec.offset = offset;
        offset += sec.data.size();
    }

    // Align section header table
    u64 shoff = offset;
    if (shoff & 7) shoff += 8 - (shoff & 7);

    // Write ELF header
    std::vector<u8> out;
    w32le(out, ELF_MAGIC);
    w8(out, ELFCLASS64);
    w8(out, ELFDATA2LSB);
    w8(out, EV_CURRENT);
    w8(out, 0);
    w8(out, 0);
    for (int i = 0; i < 7; i++) w8(out, 0);
    w16le(out, obj.is_executable ? ET_EXEC : ET_REL);
    w16le(out, EM_HVM);
    w32le(out, 1);

    // Entry point
    u64 entry = 0;
    for (const auto& s : syms) {
        if (s.st_info >> 4 == STB_GLOBAL) {
            // Check if it's _start
            continue;
        }
    }
    // Search global symbols from obj
    for (const auto& gs : obj.global_symbols) {
        if (gs.name == "_start" || gs.name == "main" || gs.name == "kernel_main") {
            entry = gs.value;
        }
    }
    w64le(out, entry);
    w64le(out, 0); // phoff
    w64le(out, shoff);
    w32le(out, 0); // flags
    w16le(out, ELF_HEADER_SIZE);
    w16le(out, 0); // phentsize
    w16le(out, 0); // phnum
    w16le(out, sizeof(Elf64_Shdr));
    w16le(out, static_cast<u16>(secs.size()));
    w16le(out, static_cast<u16>(shstrndx));

    // Write section data
    for (auto& sec : secs) {
        if (sec.data.empty()) continue;
        if (sec.type == SHT_NOBITS) continue;
        while (out.size() < sec.offset) out.push_back(0);
        out.insert(out.end(), sec.data.begin(), sec.data.end());
    }

    // Write section headers
    while (out.size() < shoff) out.push_back(0);
    for (const auto& sec : secs) {
        w32le(out, sec.name_off);
        w32le(out, sec.type);
        w64le(out, sec.flags);
        w64le(out, sec.addr);
        w64le(out, sec.offset);
        w64le(out, sec.size);
        w32le(out, sec.link);
        w32le(out, sec.info);
        w64le(out, sec.align);
        w64le(out, sec.entsize);
    }

    return out;
}

std::vector<u8> write_executable(const ObjectFile& obj) {
    ObjectFile exec = obj;
    exec.is_executable = true;
    return write_elf(exec);
}

} // namespace hvmc
