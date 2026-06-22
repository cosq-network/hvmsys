#include <gtest/gtest.h>
#include "hvmc/encoder.h"
#include "hvmc/elf.h"
#include "hvmc/assembler.h"

using namespace hvmc;

// ═════════════════════════════════════════════════════════════════════
//  ENCODER TESTS
// ═════════════════════════════════════════════════════════════════════

TEST(HvmcEncoder, NOP) {
    EncodedInst inst = encode("NOP", {});
    ASSERT_EQ(inst.bytes.size(), 4);
    EXPECT_FALSE(inst.is_escape32);
    // opcode 0x00, rest = 0
    EXPECT_EQ(inst.bytes[0], 0);
    EXPECT_EQ(inst.bytes[1], 0);
    EXPECT_EQ(inst.bytes[2], 0);
    EXPECT_EQ(inst.bytes[3], 0);
}

TEST(HvmcEncoder, MOV) {
    EncodedInst inst = encode("MOV", {.rd = 10, .rs1 = 5});
    ASSERT_EQ(inst.bytes.size(), 4);
    // opcode=0x01, rd=10, rs1=5
    u32 expected = encode_r32(0x01, 10, 5, 0, 0);
    EXPECT_EQ(inst.bytes[0], expected & 0xFF);
    EXPECT_EQ(inst.bytes[1], (expected >> 8) & 0xFF);
    EXPECT_EQ(inst.bytes[2], (expected >> 16) & 0xFF);
    EXPECT_EQ(inst.bytes[3], (expected >> 24) & 0xFF);
}

TEST(HvmcEncoder, ADDI) {
    EncodedInst inst = encode("ADDI", {.rd = 10, .rs1 = 5, .imm = 42});
    ASSERT_EQ(inst.bytes.size(), 4);
    u32 expected = encode_i32(0x05, 10, 5, 42);
    EXPECT_EQ(inst.bytes[0], expected & 0xFF);
    EXPECT_EQ(inst.bytes[1], (expected >> 8) & 0xFF);
    EXPECT_EQ(inst.bytes[2], (expected >> 16) & 0xFF);
    EXPECT_EQ(inst.bytes[3], (expected >> 24) & 0xFF);
}

TEST(HvmcEncoder, ADDI_negative_imm) {
    EncodedInst inst = encode("ADDI", {.rd = 10, .rs1 = 5, .imm = -50});
    ASSERT_EQ(inst.bytes.size(), 4);
    u32 expected = encode_i32(0x05, 10, 5, -50);
    EXPECT_EQ(inst.bytes[0], expected & 0xFF);
}

TEST(HvmcEncoder, ADD) {
    // Same opcode 0x10, func=0
    EncodedInst inst = encode("ADD", {.rd = 1, .rs1 = 2, .rs2 = 3});
    ASSERT_EQ(inst.bytes.size(), 4);
    u32 expected = encode_r32(0x10, 1, 2, 3, 0);
    EXPECT_EQ(inst.bytes[0], expected & 0xFF);
}

TEST(HvmcEncoder, SUB) {
    EncodedInst inst = encode("SUB", {.rd = 1, .rs1 = 2, .rs2 = 3});
    ASSERT_EQ(inst.bytes.size(), 4);
    u32 expected = encode_r32(0x10, 1, 2, 3, 1); // func=1 for SUB
    EXPECT_EQ(inst.bytes[0], expected & 0xFF);
}

TEST(HvmcEncoder, BEQ) {
    EncodedInst inst = encode("BEQ", {.rs1 = 1, .rs2 = 2, .imm = 8});
    ASSERT_EQ(inst.bytes.size(), 4);
    u32 expected = encode_b32(0x50, 1, 2, 8);
    EXPECT_EQ(inst.bytes[0], expected & 0xFF);
}

TEST(HvmcEncoder, JMP) {
    EncodedInst inst = encode("JMP", {.imm = 100});
    ASSERT_EQ(inst.bytes.size(), 4);
}

TEST(HvmcEncoder, JAL) {
    EncodedInst inst = encode("JAL", {.rd = 29, .imm = 50});
    ASSERT_EQ(inst.bytes.size(), 4);
}

TEST(HvmcEncoder, RET) {
    EncodedInst inst = encode("RET", {});
    ASSERT_EQ(inst.bytes.size(), 4);
    u32 expected = encode_r32(0x63, 0, 0, 0, 0);
    EXPECT_EQ(inst.bytes[0], expected & 0xFF);
}

TEST(HvmcEncoder, LD_D) {
    EncodedInst inst = encode("LD.D", {.rd = 10, .rs1 = 2, .imm = 8});
    ASSERT_EQ(inst.bytes.size(), 4);
}

TEST(HvmcEncoder, ST_D) {
    EncodedInst inst = encode("ST.D", {.rd = 1, .rs1 = 2, .imm = 4});
    ASSERT_EQ(inst.bytes.size(), 4);
}

TEST(HvmcEncoder, PUSH) {
    EncodedInst inst = encode("PUSH", {.rs1 = 5});
    ASSERT_EQ(inst.bytes.size(), 4);
}

TEST(HvmcEncoder, POP) {
    EncodedInst inst = encode("POP", {.rd = 10});
    ASSERT_EQ(inst.bytes.size(), 4);
}

TEST(HvmcEncoder, ENTER_escape32) {
    EncodedInst inst = encode("ENTER", {.imm = 32});
    ASSERT_EQ(inst.bytes.size(), 8);
    EXPECT_TRUE(inst.is_escape32);
    EXPECT_EQ(inst.bytes[0], 0xFE);
}

TEST(HvmcEncoder, LEAVE_escape32) {
    EncodedInst inst = encode("LEAVE", {});
    ASSERT_EQ(inst.bytes.size(), 8);
    EXPECT_TRUE(inst.is_escape32);
    EXPECT_EQ(inst.bytes[0], 0xFE);
}

TEST(HvmcEncoder, CALL_escape32) {
    EncodedInst inst = encode("CALL", {.rd = 29, .imm = 100});
    ASSERT_EQ(inst.bytes.size(), 8);
    EXPECT_TRUE(inst.is_escape32);
    EXPECT_EQ(inst.bytes[0], 0xFE);
}

TEST(HvmcEncoder, SYSCALL) {
    EncodedInst inst = encode("SYSCALL", {.rd = 1, .imm = 5});
    ASSERT_EQ(inst.bytes.size(), 8);
    EXPECT_TRUE(inst.is_escape32);
}

TEST(HvmcEncoder, CSRRW) {
    EncodedInst inst = encode("CSRRW", {.rd = 1, .rs1 = 5, .imm = 0x005});
    ASSERT_EQ(inst.bytes.size(), 8);
    EXPECT_TRUE(inst.is_escape32);
}

TEST(HvmcEncoder, ECALL) {
    EncodedInst inst = encode("ECALL", {});
    ASSERT_EQ(inst.bytes.size(), 8);
}

TEST(HvmcEncoder, LR_D) {
    EncodedInst inst = encode("LR.D", {.rd = 1, .rs1 = 2});
    ASSERT_EQ(inst.bytes.size(), 8);
}

TEST(HvmcEncoder, SC_D) {
    EncodedInst inst = encode("SC.D", {.rd = 4, .rs1 = 2, .rs2 = 3});
    ASSERT_EQ(inst.bytes.size(), 8);
}

TEST(HvmcEncoder, LDA) {
    EncodedInst inst = encode("LDA", {.rd = 10, .rs1 = 1, .imm = 64});
    ASSERT_EQ(inst.bytes.size(), 4);
}

TEST(HvmcEncoder, ENTER_LEAVE_roundtrip) {
    // ENTER 32
    EncodedInst enter = encode("ENTER", {.imm = 32});
    ASSERT_EQ(enter.bytes.size(), 8);
    EXPECT_EQ(enter.bytes[0], 0xFE);

    // LEAVE
    EncodedInst leave = encode("LEAVE", {});
    ASSERT_EQ(leave.bytes.size(), 8);
    EXPECT_EQ(leave.bytes[0], 0xFE);
}

// ═════════════════════════════════════════════════════════════════════
//  ASSEMBLER TESTS
// ═════════════════════════════════════════════════════════════════════

TEST(HvmcAssembler, simple_mov) {
    std::string asm_src = R"(
        .section .text
        mov r1, r2
        ret
    )";
    ObjectFile obj = assemble(asm_src);
    ASSERT_GE(obj.sections.size(), 1);
    ASSERT_FALSE(obj.sections[0].data.empty());
    // First section should be .text
    EXPECT_EQ(obj.sections[0].name, ".text");
}

TEST(HvmcAssembler, labels_and_globals) {
    std::string asm_src = R"(
        .section .text
        .global _start
    _start:
        mov a0, zero
        ret
    )";
    ObjectFile obj = assemble(asm_src);
    bool found_start = false;
    for (const auto& s : obj.global_symbols) {
        if (s.name == "_start") found_start = true;
    }
    EXPECT_TRUE(found_start);
    EXPECT_FALSE(obj.sections[0].data.empty());
}

TEST(HvmcAssembler, data_section) {
    std::string asm_src = R"(
        .section .data
    my_data:
        .quad 0xDEADBEEF
        .word 0x1234
        .byte 0xAB
    )";
    ObjectFile obj = assemble(asm_src);
    bool found_data = false;
    for (const auto& sec : obj.sections) {
        if (sec.name == ".data") {
            found_data = true;
            EXPECT_GE(sec.data.size(), 8 + 2 + 1);
        }
    }
    EXPECT_TRUE(found_data);
}

TEST(HvmcAssembler, rodata_string) {
    std::string asm_src = R"(
        .section .rodata
    hello:
        .asciz "Hello, World!"
    )";
    ObjectFile obj = assemble(asm_src);
    bool found_rodata = false;
    for (const auto& sec : obj.sections) {
        if (sec.name == ".rodata") {
            found_rodata = true;
            EXPECT_GT(sec.data.size(), 0);
        }
    }
    EXPECT_TRUE(found_rodata);
}

// ═════════════════════════════════════════════════════════════════════
//  ELF WRITER TESTS
// ═════════════════════════════════════════════════════════════════════

TEST(HvmcElf, writes_valid_elf_header) {
    ObjectFile obj;
    Section text;
    text.name = ".text";
    text.flags = SHF_ALLOC | SHF_EXECINSTR;
    // One NOP instruction
    EncodedInst nop = encode("NOP", {});
    text.data = nop.bytes;
    obj.sections.push_back(text);

    Symbol sym;
    sym.name = "_start";
    sym.value = 0;
    sym.is_global = true;
    sym.is_function = true;
    sym.section_idx = 1;
    obj.global_symbols.push_back(sym);

    std::vector<u8> elf = write_elf(obj);
    ASSERT_GT(elf.size(), 64);

    // Check ELF magic
    EXPECT_EQ(elf[0], 0x7F);
    EXPECT_EQ(elf[1], 'E');
    EXPECT_EQ(elf[2], 'L');
    EXPECT_EQ(elf[3], 'F');

    // 64-bit, little-endian
    EXPECT_EQ(elf[4], 2); // ELFCLASS64
    EXPECT_EQ(elf[5], 1); // ELFDATA2LSB

    // Machine = EM_HVM
    EXPECT_EQ(elf[18], 0x99); // low byte of EM_HVM
    EXPECT_EQ(elf[19], 0x99); // high byte of EM_HVM

    // Object file (ET_REL)
    EXPECT_EQ(elf[16], 1);
}

TEST(HvmcElf, writes_executable) {
    ObjectFile obj;
    Section text;
    text.name = ".text";
    text.flags = SHF_ALLOC | SHF_EXECINSTR;
    EncodedInst inst = encode("NOP", {});
    text.data = inst.bytes;
    obj.sections.push_back(text);

    Symbol sym;
    sym.name = "_start";
    sym.value = 0;
    sym.is_global = true;
    sym.is_function = true;
    sym.section_idx = 1;
    obj.global_symbols.push_back(sym);

    std::vector<u8> elf = write_executable(obj);
    ASSERT_GT(elf.size(), 64);

    // Executable type
    EXPECT_EQ(elf[16], 2); // ET_EXEC
}

// ═════════════════════════════════════════════════════════════════════
//  INTEGRATION: Assemble → ELF round-trip
// ═════════════════════════════════════════════════════════════════════

TEST(HvmcIntegration, assemble_and_write_elf) {
    std::string asm_src = R"(
        .section .text
        .global _start
    _start:
        mov a0, zero
        addi a0, a0, 42
        ret
    )";
    ObjectFile obj = assemble(asm_src);
    std::vector<u8> elf = write_elf(obj);
    ASSERT_GT(elf.size(), 64);

    // Read back the header
    u32 magic = static_cast<u32>(elf[0]) | (static_cast<u32>(elf[1]) << 8) |
                (static_cast<u32>(elf[2]) << 16) | (static_cast<u32>(elf[3]) << 24);
    EXPECT_EQ(magic, 0x464C457F);
    EXPECT_EQ(elf[16], 1); // ET_REL
}
