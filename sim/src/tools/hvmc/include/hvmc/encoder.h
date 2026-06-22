#pragma once
#include "types.h"

namespace hvmc {

// Encode a base32 R-format instruction: opcode[7] rd[5] rs1[5] rs2[5] func[10]
u32 encode_r32(u32 opcode, u8 rd, u8 rs1, u8 rs2, u16 func);

// Encode a base32 I-format instruction: opcode[7] rd[5] rs1[5] imm[15]
u32 encode_i32(u32 opcode, u8 rd, u8 rs1, i16 imm);

// Encode a base32 B-format instruction: opcode[7] rs1[5] rs2[5] imm[15]
u32 encode_b32(u32 opcode, u8 rs1, u8 rs2, i16 imm);

// Encode a base32 J-format instruction: opcode[7] rd[5] imm[20]
u32 encode_j32(u32 opcode, u8 rd, i32 imm);

// Encode a ULEB128 value
void encode_uleb128(std::vector<u8>& buf, u32 val);

// Encode an escape32 instruction (8 bytes): 0xFE + ULEB128 opcode + zero-pad + 32-bit payload
void emit_escape32(std::vector<u8>& buf, u32 opcode, u32 payload);

// Encode an escape32 instruction with the given format payload
void emit_escape_R(std::vector<u8>& buf, u32 opcode, u8 rd, u8 rs1, u8 rs2, u16 func);
void emit_escape_I(std::vector<u8>& buf, u32 opcode, u8 rd, u8 rs1, i16 imm);
void emit_escape_B(std::vector<u8>& buf, u32 opcode, u8 rs1, u8 rs2, i16 imm);
void emit_escape_J(std::vector<u8>& buf, u32 opcode, u8 rd, i32 imm);

// Encode an instruction from its structured form
EncodedInst encode_instruction(const Instruction& inst);

// Encode instruction from mnemonic + operands (looks up opcode table)
EncodedInst encode(const std::string& mnemonic, const Operands& ops);

} // namespace hvmc
