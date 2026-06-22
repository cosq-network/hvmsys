#include "hvmc/encoder.h"

namespace hvmc {

u32 encode_r32(u32 opcode, u8 rd, u8 rs1, u8 rs2, u16 func) {
    return (opcode << 25) | (rd << 20) | (rs1 << 15) | (rs2 << 10) | (func & 0x3FF);
}

u32 encode_i32(u32 opcode, u8 rd, u8 rs1, i16 imm) {
    return (opcode << 25) | (rd << 20) | (rs1 << 15) | (static_cast<u16>(imm) & 0x7FFF);
}

u32 encode_b32(u32 opcode, u8 rs1, u8 rs2, i16 imm) {
    return (opcode << 25) | (rs1 << 20) | (rs2 << 15) | (static_cast<u16>(imm) & 0x7FFF);
}

u32 encode_j32(u32 opcode, u8 rd, i32 imm) {
    return (opcode << 25) | (rd << 20) | (static_cast<u32>(imm) & 0xFFFFF);
}

void encode_uleb128(std::vector<u8>& buf, u32 val) {
    while (val >= 0x80) {
        buf.push_back(static_cast<u8>((val & 0x7F) | 0x80));
        val >>= 7;
    }
    buf.push_back(static_cast<u8>(val));
}

void emit_escape32(std::vector<u8>& buf, u32 opcode, u32 payload) {
    buf.push_back(0xFE);
    encode_uleb128(buf, opcode);
    while (buf.size() < 4) buf.push_back(0x00);
    buf.push_back(static_cast<u8>(payload));
    buf.push_back(static_cast<u8>(payload >> 8));
    buf.push_back(static_cast<u8>(payload >> 16));
    buf.push_back(static_cast<u8>(payload >> 24));
}

void emit_escape_R(std::vector<u8>& buf, u32 opcode, u8 rd, u8 rs1, u8 rs2, u16 func) {
    emit_escape32(buf, opcode, encode_r32(0, rd, rs1, rs2, func));
}

void emit_escape_I(std::vector<u8>& buf, u32 opcode, u8 rd, u8 rs1, i16 imm) {
    emit_escape32(buf, opcode, encode_i32(0, rd, rs1, imm));
}

void emit_escape_B(std::vector<u8>& buf, u32 opcode, u8 rs1, u8 rs2, i16 imm) {
    emit_escape32(buf, opcode, encode_b32(0, rs1, rs2, imm));
}

void emit_escape_J(std::vector<u8>& buf, u32 opcode, u8 rd, i32 imm) {
    emit_escape32(buf, opcode, encode_j32(0, rd, imm));
}

EncodedInst encode_instruction(const Instruction& inst) {
    EncodedInst result;
    u32 payload = 0;

    switch (inst.format) {
    case InstFormat::R:
        payload = encode_r32(inst.opcode, inst.operands.rd, inst.operands.rs1,
                             inst.operands.rs2, inst.operands.func);
        break;
    case InstFormat::I:
        payload = encode_i32(inst.opcode, inst.operands.rd, inst.operands.rs1,
                             inst.operands.imm);
        break;
    case InstFormat::RI:
        payload = encode_r32(inst.opcode, inst.operands.rd, inst.operands.rs1,
                             inst.operands.rs2, inst.operands.imm);
        break;
    case InstFormat::B:
        payload = encode_b32(inst.opcode, inst.operands.rs1, inst.operands.rs2,
                             inst.operands.imm);
        break;
    case InstFormat::J:
        payload = encode_j32(inst.opcode, inst.operands.rd, inst.operands.imm);
        break;
    }

    if (inst.encoding == EncodingType::escape32) {
        emit_escape32(result.bytes, inst.opcode, payload);
        result.is_escape32 = true;
    } else {
        result.bytes.push_back(static_cast<u8>(payload));
        result.bytes.push_back(static_cast<u8>(payload >> 8));
        result.bytes.push_back(static_cast<u8>(payload >> 16));
        result.bytes.push_back(static_cast<u8>(payload >> 24));
        result.is_escape32 = false;
    }
    return result;
}

EncodedInst encode(const std::string& mnemonic, const Operands& ops) {
    for (size_t i = 0; i < opcode_count; i++) {
        if (mnemonic == opcode_table[i].mnemonic) {
            Instruction inst;
            inst.mnemonic = opcode_table[i].mnemonic;
            inst.opcode = opcode_table[i].opcode;
            inst.encoding = opcode_table[i].encoding;
            inst.format = opcode_table[i].format;
            inst.operands = ops;
            if (opcode_table[i].func >= 0) {
                inst.operands.func = opcode_table[i].func;
            }
            return encode_instruction(inst);
        }
    }
    return EncodedInst{};
}

} // namespace hvmc
