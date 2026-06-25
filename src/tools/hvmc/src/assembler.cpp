#include "hvmc/assembler.h"
#include "hvmc/encoder.h"
#include "hvmc/elf.h"
#include <sstream>
#include <iostream>
#include <cctype>
#include <unordered_map>
#include <cstdlib>
#include <algorithm>

namespace hvmc {

static std::string to_upper(const std::string& s) {
    std::string result = s;
    std::transform(result.begin(), result.end(), result.begin(), 
                   [](unsigned char c) { return std::toupper(c); });
    return result;
}

struct AsmToken {
    enum Type { IDENT, NUMBER, STRING, COLON, COMMA, LPAREN, RPAREN,
                PLUS, MINUS, STAR, SLASH, NEWLINE, END, DOT, HASH };
    Type type;
    std::string text;
    i64 intval;
    int line;
};

struct AsmLexer {
    std::string src;
    size_t pos = 0;
    int line = 1;

    char peek() {
        while (pos < src.size() && (src[pos] == ' ' || src[pos] == '\t')) pos++;
        return pos < src.size() ? src[pos] : '\0';
    }

    char get() {
        return pos < src.size() ? src[pos++] : '\0';
    }

    void skip_line() {
        while (pos < src.size() && src[pos] != '\n') pos++;
    }

    AsmToken next() {
        char c = peek();
        if (c == '\0') return {AsmToken::END, "", 0, line};
        if (c == '\n') { get(); line++; return {AsmToken::NEWLINE, "", 0, line}; }
        if (c == '#') { skip_line(); return {AsmToken::NEWLINE, "", 0, line}; }
        if (c == '/') {
            if (pos + 1 < src.size() && src[pos + 1] == '/') {
                skip_line();
                return {AsmToken::NEWLINE, "", 0, line};
            }
        }
        if (c == ',') { get(); return {AsmToken::COMMA, ",", 0, line}; }
        if (c == ':') { get(); return {AsmToken::COLON, ":", 0, line}; }
        if (c == '(') { get(); return {AsmToken::LPAREN, "(", 0, line}; }
        if (c == ')') { get(); return {AsmToken::RPAREN, ")", 0, line}; }
        if (c == '+') { get(); return {AsmToken::PLUS, "+", 0, line}; }
        if (c == '-') { get(); return {AsmToken::MINUS, "-", 0, line}; }
        if (c == '*') { get(); return {AsmToken::STAR, "*", 0, line}; }
        if (c == '/') { get(); return {AsmToken::SLASH, "/", 0, line}; }
        if (c == '.') { get(); return {AsmToken::DOT, ".", 0, line}; }

        if (c == '"') {
            get(); std::string s;
            while (pos < src.size() && src[pos] != '"') {
                if (src[pos] == '\\' && pos + 1 < src.size()) {
                    pos++;
                    switch (src[pos]) {
                        case 'n': s += '\n'; break;
                        case 'r': s += '\r'; break;
                        case 't': s += '\t'; break;
                        case '0': s += '\0'; break;
                        default: s += src[pos]; break;
                    }
                } else s += src[pos];
                pos++;
            }
            if (pos < src.size()) pos++;
            return {AsmToken::STRING, s, 0, line};
        }

        if (std::isdigit(c) || (c == '-' && pos + 1 < src.size() && std::isdigit(src[pos+1]))) {
            std::string num;
            if (c == '-') { num += '-'; get(); c = peek(); }
            while (pos < src.size() && (std::isxdigit(src[pos]) || src[pos] == 'x' || src[pos] == 'X')) {
                num += get();
            }
            i64 val;
            if (num.size() > 1 && (num[0] == '0' && (num[1] == 'x' || num[1] == 'X'))) {
                val = static_cast<i64>(std::strtoull(num.c_str(), nullptr, 16));
            } else {
                val = std::stoll(num);
            }
            return {AsmToken::NUMBER, num, val, line};
        }

        if (c == '$') {
            get();
            std::string id;
            while (pos < src.size() && (std::isalnum(src[pos]) || src[pos] == '_' || src[pos] == '.')) {
                id += get();
            }
            return {AsmToken::IDENT, id, 0, line};
        }

        if (std::isalpha(c) || c == '_' || c == '.') {
            std::string id;
            while (pos < src.size() && (std::isalnum(src[pos]) || src[pos] == '_' || src[pos] == '.')) {
                id += get();
            }
            return {AsmToken::IDENT, id, 0, line};
        }

        AsmToken t;
        t.type = AsmToken::END;
        t.line = line;
        return t;
    }
};

static int parse_reg(const std::string& s) {
    if (s == "zero" || s == "r0") return 0;
    for (int i = 0; i < 32; i++) {
        if (s == reg_name(i)) return i;
    }
    if (s.size() > 1 && s[0] == 'r') {
        int n = std::atoi(s.c_str() + 1);
        if (n >= 0 && n < 32) return n;
    }
    return -1;
}

ObjectFile assemble(const std::string& source, const std::string&) {
    ObjectFile obj;
    Section text_sec;
    text_sec.name = ".text";
    text_sec.flags = SHF_ALLOC | SHF_EXECINSTR;

    Section data_sec;
    data_sec.name = ".data";
    data_sec.flags = SHF_ALLOC | SHF_WRITE;

    Section rodata_sec;
    rodata_sec.name = ".rodata";
    rodata_sec.flags = SHF_ALLOC;

    Section* current_sec = &text_sec;
    std::unordered_map<std::string, u64> labels;
    std::vector<std::pair<Section*, u64>> label_locations;

    // Two-pass assembler
    for (int current_pass = 0; current_pass < 2; current_pass++) {
        AsmLexer lex;
        lex.src = source;
        lex.pos = 0;
        lex.line = 1;

        std::vector<u8> current_data;
        std::vector<Relocation> current_relocs;

        auto next_tok = [&]() { return lex.next(); };

        AsmToken tok = next_tok();
        while (tok.type != AsmToken::END) {
            if (tok.type == AsmToken::NEWLINE) { tok = next_tok(); continue; }

            // Directive
            if (tok.type == AsmToken::DOT) {
                AsmToken dir = next_tok();
                if (dir.type != AsmToken::IDENT) { tok = next_tok(); continue; }

                if (dir.text == "section" || dir.text == "text" ||
                    dir.text == "data" || dir.text == "rodata") {
                    // Commit current section data
                    if (current_pass == 1 && current_sec) {
                        current_sec->data = current_data;
                        current_sec->relocs = current_relocs;
                    }
                    if (dir.text == "section") {
                        AsmToken sn = next_tok();
                        // Handle both ".section text" and ".section .text" syntax
                        if (sn.type == AsmToken::DOT) {
                            sn = next_tok(); // Skip the dot, get the actual section name
                        }
                        if (sn.text == "text") { current_sec = &text_sec; current_data.clear(); current_relocs.clear(); }
                        else if (sn.text == "data") { current_sec = &data_sec; current_data.clear(); current_relocs.clear(); }
                        else if (sn.text == "rodata") { current_sec = &rodata_sec; current_data.clear(); current_relocs.clear(); }
                    } else if (dir.text == "text") { current_sec = &text_sec; current_data.clear(); current_relocs.clear(); }
                    else if (dir.text == "data") { current_sec = &data_sec; current_data.clear(); current_relocs.clear(); }
                    else if (dir.text == "rodata") { current_sec = &rodata_sec; current_data.clear(); current_relocs.clear(); }
                    while (tok.type != AsmToken::NEWLINE && tok.type != AsmToken::END) tok = next_tok();
                    tok = next_tok();
                    continue;
                }

                if (dir.text == "global" || dir.text == "globl") {
                    AsmToken name = next_tok();
                    if (current_pass == 1) {
                        Symbol sym;
                        sym.name = name.text;
                        sym.is_global = true;
                        sym.is_function = true;
                        auto it = labels.find(name.text);
                        if (it != labels.end()) sym.value = it->second;
                        sym.section_idx = 1;
                        obj.global_symbols.push_back(sym);
                    }
                    while (tok.type != AsmToken::NEWLINE && tok.type != AsmToken::END) tok = next_tok();
                    tok = next_tok();
                    continue;
                }

                if (dir.text == "byte") {
                    AsmToken val = next_tok();
                    if (val.type == AsmToken::NUMBER) {
                        if (current_pass == 1) {
                            u8 v = static_cast<u8>(val.intval & 0xFF);
                            current_data.push_back(v);
                        } else {
                            current_data.push_back(0);
                        }
                    }
                    while (tok.type != AsmToken::NEWLINE && tok.type != AsmToken::END) tok = next_tok();
                    tok = next_tok();
                    continue;
                }

                if (dir.text == "word" || dir.text == "short") {
                    AsmToken val = next_tok();
                    if (val.type == AsmToken::NUMBER) {
                        if (current_pass == 1) {
                            u16 v = static_cast<u16>(val.intval & 0xFFFF);
                            current_data.push_back(v & 0xFF);
                            current_data.push_back((v >> 8) & 0xFF);
                        } else {
                            current_data.push_back(0);
                            current_data.push_back(0);
                        }
                    }
                    while (tok.type != AsmToken::NEWLINE && tok.type != AsmToken::END) tok = next_tok();
                    tok = next_tok();
                    continue;
                }

                if (dir.text == "quad" || dir.text == "dword") {
                    AsmToken val = next_tok();
                    if (val.type == AsmToken::NUMBER) {
                        if (current_pass == 1) {
                            u64 v = static_cast<u64>(val.intval);
                            for (int i = 0; i < 8; i++)
                                current_data.push_back((v >> (i*8)) & 0xFF);
                        } else {
                            for (int i = 0; i < 8; i++)
                                current_data.push_back(0);
                        }
                    }
                    while (tok.type != AsmToken::NEWLINE && tok.type != AsmToken::END) tok = next_tok();
                    tok = next_tok();
                    continue;
                }

                if (dir.text == "ascii") {
                    AsmToken str = next_tok();
                    if (str.type == AsmToken::STRING) {
                        if (current_pass == 1) {
                            current_data.insert(current_data.end(), str.text.begin(), str.text.end());
                        } else {
                            current_data.insert(current_data.end(), str.text.size(), 0);
                        }
                    }
                    while (tok.type != AsmToken::NEWLINE && tok.type != AsmToken::END) tok = next_tok();
                    tok = next_tok();
                    continue;
                }

                if (dir.text == "asciz" || dir.text == "string") {
                    AsmToken str = next_tok();
                    if (str.type == AsmToken::STRING) {
                        if (current_pass == 1) {
                            current_data.insert(current_data.end(), str.text.begin(), str.text.end());
                            current_data.push_back(0);
                        } else {
                            current_data.insert(current_data.end(), str.text.size() + 1, 0);
                        }
                    }
                    while (tok.type != AsmToken::NEWLINE && tok.type != AsmToken::END) tok = next_tok();
                    tok = next_tok();
                    continue;
                }

                if (dir.text == "align") {
                    AsmToken val_t = next_tok();
                    if (val_t.type == AsmToken::NUMBER) {
                        u64 align = static_cast<u64>(val_t.intval);
                        u64 mis = current_data.size() % align;
                        if (mis) {
                            current_data.insert(current_data.end(), align - mis, 0);
                        }
                    }
                    while (tok.type != AsmToken::NEWLINE && tok.type != AsmToken::END) tok = next_tok();
                    tok = next_tok();
                    continue;
                }

                while (tok.type != AsmToken::NEWLINE && tok.type != AsmToken::END) tok = next_tok();
                tok = next_tok();
                continue;
            }

            // Label
            if (tok.type == AsmToken::IDENT) {
                std::string id = tok.text;
                AsmToken next_t = next_tok();
                if (next_t.type == AsmToken::COLON) {
                    if (current_pass == 0) {
                        labels[id] = current_data.size();
                    }
                    tok = next_tok();
                    continue;
                }

                // It's a mnemonic
                if (id == "//") {
                    while (tok.type != AsmToken::NEWLINE && tok.type != AsmToken::END) tok = next_tok();
                    tok = next_tok();
                    continue;
                }

                // Look up opcode (case-insensitive)
                const OpcodeEntry* entry = nullptr;
                std::string id_upper = to_upper(id);
                for (size_t i = 0; i < opcode_count; i++) {
                    if (id_upper == opcode_table[i].mnemonic) {
                        entry = &opcode_table[i];
                        break;
                    }
                }
                if (!entry) {
                    while (tok.type != AsmToken::NEWLINE && tok.type != AsmToken::END) tok = next_tok();
                    tok = next_tok();
                    continue;
                }

                // Parse operands
                Operands ops;
                int regs[3] = {-1, -1, -1};
                int op_idx = 0;
                i64 imm_val = 0;
                bool has_imm = false;

                bool done = false;
                while (!done) {
                    AsmToken ot = next_tok();
                    if (ot.type == AsmToken::NEWLINE || ot.type == AsmToken::END) { done = true; break; }
                    if (ot.type == AsmToken::COMMA) continue;

                    if (ot.type == AsmToken::MINUS) {
                        AsmToken nt = next_tok();
                        if (nt.type == AsmToken::NUMBER) {
                            imm_val = -nt.intval;
                            has_imm = true;
                        } else if (nt.type == AsmToken::IDENT) {
                            auto it = labels.find(nt.text);
                            if (it != labels.end()) {
                                imm_val = -static_cast<i64>(it->second);
                            }
                            has_imm = true;
                        }
                        continue;
                    }

                    if (ot.type == AsmToken::PLUS) {
                        AsmToken nt = next_tok();
                        if (nt.type == AsmToken::NUMBER) {
                            imm_val = nt.intval;
                            has_imm = true;
                        }
                        continue;
                    }

                    if (ot.type == AsmToken::NUMBER) {
                        imm_val = ot.intval;
                        has_imm = true;
                        continue;
                    }

                    if (ot.type == AsmToken::IDENT) {
                        int reg = parse_reg(ot.text);
                        if (reg >= 0 && op_idx < 3) {
                            regs[op_idx++] = reg;
                        } else {
                            auto it = labels.find(ot.text);
                            if (it != labels.end()) {
                                i64 label_off = static_cast<i64>(it->second);
                                bool is_branch = (entry->format == InstFormat::B || entry->format == InstFormat::J);
                                if (current_pass == 1 && is_branch) {
                                    imm_val = label_off - static_cast<i64>(current_data.size());
                                } else {
                                    imm_val = label_off;
                                }
                            }
                            has_imm = true;
                        }
                    }
                }

                ops.rd = regs[0] >= 0 ? regs[0] : 0;
                ops.rs1 = regs[1] >= 0 ? regs[1] : 0;
                ops.rs2 = regs[2] >= 0 ? regs[2] : 0;
                if (has_imm) ops.imm = static_cast<int>(imm_val);
                if (entry->func >= 0) ops.func = entry->func;

                Instruction inst;
                inst.mnemonic = entry->mnemonic;
                inst.opcode = entry->opcode;
                inst.encoding = entry->encoding;
                inst.format = entry->format;
                inst.operands = ops;

                EncodedInst res = encode_instruction(inst);
                current_data.insert(current_data.end(), res.bytes.begin(), res.bytes.end());
                tok = next_tok();
                continue;
            }

            tok = next_tok();
        }

        if (current_pass == 1 && current_sec) {
            current_sec->data = current_data;
            current_sec->relocs = current_relocs;
        }
    }

    obj.sections.push_back(text_sec);
    obj.sections.push_back(rodata_sec);
    obj.sections.push_back(data_sec);
    return obj;
}

std::vector<u8> assemble_text(const std::string& source) {
    ObjectFile obj = assemble(source);
    for (const auto& sec : obj.sections) {
        if (sec.name == ".text") return sec.data;
    }
    return {};
}

} // namespace hvmc
