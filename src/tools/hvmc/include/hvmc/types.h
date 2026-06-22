#pragma once
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
#include <variant>

namespace hvmc {

using u8  = uint8_t;
using u16 = uint16_t;
using u32 = uint32_t;
using u64 = uint64_t;
using i16 = int16_t;
using i32 = int32_t;
using i64 = int64_t;

// HVM instruction formats
enum class InstFormat : u8 {
    R,  // opcode[7] rd[5] rs1[5] rs2[5] func[10]
    I,  // opcode[7] rd[5] rs1[5] imm[15]
    RI, // opcode[7] rd[5] rs1[5] rs2[5] imm[10]
    B,  // opcode[7] rs1[5] rs2[5] imm[15]
    J,  // opcode[7] rd[5] imm[20]
};

enum class EncodingType : u8 {
    base32,   // 4-byte instruction
    escape32, // 8-byte instruction (0xFE prefix + ULEB128 opcode + payload)
};

// Structured operand for encoding
struct Operands {
    // For R: rd, rs1, rs2, func
    // For I: rd, rs1, imm
    // For RI: rd, rs1, rs2, imm
    // For B: rs1, rs2, imm
    // For J: rd, imm
    int rd  = 0;
    int rs1 = 0;
    int rs2 = 0;
    int imm = 0;
    int func = 0;
};

// A single instruction
struct Instruction {
    std::string mnemonic;
    u32 opcode;
    EncodingType encoding;
    InstFormat format;
    Operands operands;
};

// Encoded instruction bytes
struct EncodedInst {
    std::vector<u8> bytes;
    bool is_escape32;
};

// Relocation types
enum class RelocType : u32 {
    R_HVM_NONE      = 0,
    R_HVM_64        = 1,
    R_HVM_PC32      = 2,
    R_HVM_PC64      = 3,
    R_HVM_BRANCH    = 4,
    R_HVM_JAL       = 5,
    R_HVM_CALL      = 6,
    R_HVM_ABS32     = 7,
    R_HVM_ABS64     = 8,
};

struct Relocation {
    u64 offset;
    RelocType type;
    int symbol;
    int64_t addend;
};

struct Symbol {
    std::string name;
    u64 value;
    u64 size;
    bool is_global;
    bool is_function;
    int section_idx;
};

struct Section {
    std::string name;
    std::vector<u8> data;
    u64 addr;
    u64 flags;
    int idx;
    std::vector<Relocation> relocs;
    std::vector<Symbol> symbols;
};

struct ObjectFile {
    std::vector<Section> sections;
    std::vector<Symbol> global_symbols;
    std::string entry_point;
    bool is_executable = false;
};

// Register enum for codegen
enum class Reg : int {
    R0 = 0, R1 = 1, R2 = 2, R3 = 3, R4 = 4, R5 = 5, R6 = 6, R7 = 7, R8 = 8,
    R9 = 9, R10 = 10, R11 = 11, R12 = 12, R13 = 13, R14 = 14, R15 = 15,
    R16 = 16, R17 = 17, R18 = 18, R19 = 19, R20 = 20, R21 = 21, R22 = 22,
    R23 = 23, R24 = 24, R25 = 25, R26 = 26, R27 = 27, R28 = 28, R29 = 29,
    R30 = 30, R31 = 31,
};

// Register ABI names
inline const char* reg_name(int idx) {
    static const char* names[32] = {
        "zero", "a0", "a1", "a2", "tp",  "a3", "a4", "a5",
        "a6",   "t0", "t1", "t2", "t3",  "t4", "t5", "t6",
        "s0",   "s1", "s2", "s3", "s4",  "s5", "s6", "s7",
        "s8",   "s9", "s10","s11","s12", "lr",  "fp", "sp"
    };
    return (idx >= 0 && idx < 32) ? names[idx] : "???";
}

// Opcode table entry
struct OpcodeEntry {
    const char* mnemonic;
    u32 opcode;
    EncodingType encoding;
    InstFormat format;
    int func;
};

// Canonical opcode table (from hvm_instruction_set.csv)
inline const OpcodeEntry opcode_table[] = {
    {"NOP",       0x00, EncodingType::base32,  InstFormat::R,  -1},
    {"MOV",       0x01, EncodingType::base32,  InstFormat::R,  -1},
    {"MOVZ",      0x03, EncodingType::base32,  InstFormat::I,  -1},
    {"LUI",       0x04, EncodingType::base32,  InstFormat::I,  -1},
    {"ADDI",      0x05, EncodingType::base32,  InstFormat::I,  -1},
    {"RETAIN",    0x06, EncodingType::base32,  InstFormat::R,  -1},
    {"RELEASE",   0x07, EncodingType::base32,  InstFormat::R,  -1},
    {"ICACHE.RNG",0x0B, EncodingType::base32,  InstFormat::R,  -1},
    {"ADD",       0x10, EncodingType::base32,  InstFormat::R,  0},
    {"SUB",       0x10, EncodingType::base32,  InstFormat::R,  1},
    {"MUL",       0x10, EncodingType::base32,  InstFormat::R,  2},
    {"DIV",       0x10, EncodingType::base32,  InstFormat::R,  5},
    {"DIVU",      0x10, EncodingType::base32,  InstFormat::R,  6},
    {"REM",       0x10, EncodingType::base32,  InstFormat::R,  7},
    {"SHL",       0x13, EncodingType::base32,  InstFormat::R,  0},
    {"SHR",       0x13, EncodingType::base32,  InstFormat::R,  1},
    {"SAR",       0x13, EncodingType::base32,  InstFormat::R,  2},
    {"AND",       0x20, EncodingType::base32,  InstFormat::R,  0},
    {"OR",        0x20, EncodingType::base32,  InstFormat::R,  1},
    {"XOR",       0x20, EncodingType::base32,  InstFormat::R,  2},
    {"NOT",       0x21, EncodingType::base32,  InstFormat::R,  -1},
    {"CMPEQ",     0x40, EncodingType::base32,  InstFormat::R,  0},
    {"CMPNE",     0x40, EncodingType::base32,  InstFormat::R,  1},
    {"CMPLT",     0x40, EncodingType::base32,  InstFormat::R,  2},
    {"CMPLE",     0x40, EncodingType::base32,  InstFormat::R,  3},
    {"FCMPEQ",    0x41, EncodingType::base32,  InstFormat::R,  0},
    {"FCMPLT",    0x41, EncodingType::base32,  InstFormat::R,  1},
    {"FCMPLE",    0x41, EncodingType::base32,  InstFormat::R,  2},
    {"FADD",      0x30, EncodingType::base32,  InstFormat::R,  0},
    {"FSUB",      0x30, EncodingType::base32,  InstFormat::R,  1},
    {"FMUL",      0x30, EncodingType::base32,  InstFormat::R,  2},
    {"FDIV",      0x30, EncodingType::base32,  InstFormat::R,  3},
    {"BEQ",       0x50, EncodingType::base32,  InstFormat::B,  -1},
    {"BNE",       0x51, EncodingType::base32,  InstFormat::B,  -1},
    {"BLT",       0x52, EncodingType::base32,  InstFormat::B,  -1},
    {"BLE",       0x53, EncodingType::base32,  InstFormat::B,  -1},
    {"JMP",       0x60, EncodingType::base32,  InstFormat::J,  -1},
    {"JAL",       0x61, EncodingType::base32,  InstFormat::J,  -1},
    {"JALR",      0x62, EncodingType::base32,  InstFormat::I,  -1},
    {"RET",       0x63, EncodingType::base32,  InstFormat::R,  -1},
    {"LD.B",      0x70, EncodingType::base32,  InstFormat::I,  -1},
    {"LD.BU",     0x71, EncodingType::base32,  InstFormat::I,  -1},
    {"LD.H",      0x72, EncodingType::base32,  InstFormat::I,  -1},
    {"LD.HU",     0x73, EncodingType::base32,  InstFormat::I,  -1},
    {"LD.W",      0x74, EncodingType::base32,  InstFormat::I,  -1},
    {"LD.WU",     0x75, EncodingType::base32,  InstFormat::I,  -1},
    {"LD.D",      0x76, EncodingType::base32,  InstFormat::I,  -1},
    {"LD.P",      0x77, EncodingType::base32,  InstFormat::R,  -1},
    {"ST.B",      0x78, EncodingType::base32,  InstFormat::I,  -1},
    {"ST.H",      0x79, EncodingType::base32,  InstFormat::I,  -1},
    {"ST.W",      0x7A, EncodingType::base32,  InstFormat::I,  -1},
    {"ST.D",      0x7B, EncodingType::base32,  InstFormat::I,  -1},
    {"ST.P",      0x7C, EncodingType::base32,  InstFormat::R,  -1},
    {"LDA",       0x7D, EncodingType::base32,  InstFormat::I,  -1},
    {"PUSH",      0x7E, EncodingType::base32,  InstFormat::R,  -1},
    {"POP",       0x7F, EncodingType::base32,  InstFormat::R,  -1},
    {"ENTER",     0x80, EncodingType::escape32,InstFormat::I,  -1},
    {"LEAVE",     0x81, EncodingType::escape32,InstFormat::R,  -1},
    {"ADJSP",     0x82, EncodingType::escape32,InstFormat::I,  -1},
    {"FRAME",     0x83, EncodingType::escape32,InstFormat::I,  -1},
    {"CALL",      0xB4, EncodingType::escape32,InstFormat::J,  -1},
    {"TAILCALL",  0xB6, EncodingType::escape32,InstFormat::J,  -1},
    {"SYSCALL",   0xC0, EncodingType::escape32,InstFormat::I,  -1},
    {"BREAK",     0xC1, EncodingType::escape32,InstFormat::R,  -1},
    {"ECALL",     0xC2, EncodingType::escape32,InstFormat::R,  -1},
    {"TRAPRET",   0xC3, EncodingType::escape32,InstFormat::R,  -1},
    {"LR.D",      0xC4, EncodingType::escape32,InstFormat::R,  -1},
    {"SC.D",      0xC5, EncodingType::escape32,InstFormat::R,  -1},
    {"CSRRW",     0xC6, EncodingType::escape32,InstFormat::I,  -1},
    {"SFENCE.VMA",0xC8, EncodingType::escape32,InstFormat::R,  -1},
    {"LOOP.SET",  0xD0, EncodingType::escape32,InstFormat::I,  -1},
    {"LOOP.DECBR",0xD1, EncodingType::escape32,InstFormat::B,  -1},
};

inline constexpr size_t opcode_count = sizeof(opcode_table) / sizeof(opcode_table[0]);

enum class DataType {
    NONE,
    VOID,
    CHAR, UCHAR,
    SHORT, USHORT,
    INT, UINT,
    LONG, ULONG,
    POINTER,
    ARRAY,
    STRUCT,
};

struct CType {
    DataType base;
    bool is_const = false;
    bool is_volatile = false;
    int pointer_depth = 0;
    int array_size = 0;
};

// Tokens for C lexer
enum class TokenKind {
    END, IDENT, NUMBER, STRING, CHAR_LIT,
    LPAREN, RPAREN, LBRACE, RBRACE, LBRACKET, RBRACKET,
    SEMICOLON, COMMA, DOT, ARROW,
    PLUS, MINUS, STAR, SLASH, PERCENT,
    AMPER, PIPE, CARET, TILDE, EXCLAM,
    EQ, EQEQ, NEQ, LT, LE, GT, GE,
    ANDAND, OROR,
    PLUSPLUS, MINUSMINUS,
    PLUS_EQ, MINUS_EQ, STAR_EQ, SLASH_EQ, PERCENT_EQ,
    AMPER_EQ, PIPE_EQ, CARET_EQ,
    LSHIFT, RSHIFT,
    LSHIFT_EQ, RSHIFT_EQ,
    COLON, QUESTION,
    IF, ELSE, WHILE, FOR, DO, SWITCH, CASE, DEFAULT,
    BREAK, CONTINUE, RETURN, GOTO,
    INT_KW, CHAR_KW, SHORT_KW, LONG_KW, UNSIGNED, SIGNED,
    VOID_KW, STATIC, EXTERN, CONST, VOLATILE, TYPEDEF, STRUCT_KW,
    SIZEOF,
    INLINE_ASM,
};

struct Token {
    TokenKind kind;
    std::string text;
    u64 intval;
    int line;
    int col;
};

// AST node types
enum class ASTKind {
    PROGRAM,
    FUNCTION_DEF,
    FUNCTION_DECL,
    VARIABLE_DECL,
    BLOCK,
    IF_STMT,
    WHILE_STMT,
    FOR_STMT,
    DO_WHILE_STMT,
    RETURN_STMT,
    BREAK_STMT,
    CONTINUE_STMT,
    EXPR_STMT,
    EMPTY_STMT,
    BINARY_OP,
    UNARY_OP,
    ASSIGN,
    CALL,
    VARIABLE,
    CONSTANT,
    STRING_LIT,
    DEREF,
    ADDRESS_OF,
    MEMBER_ACCESS,
    ARRAY_INDEX,
    CAST,
    SIZEOF_EXPR,
    ASM_STMT,
    STRUCT_DECL,
};

struct ASTNode;
using ASTNodePtr = ASTNode*;

// Simple dynamic array (replacement for std::vector in the union)
template<typename T>
struct NodeArray {
    T* data = nullptr;
    size_t count = 0;
    size_t capacity = 0;

    void push(T item) {
        if (count >= capacity) {
            size_t new_cap = capacity ? capacity * 2 : 4;
            T* new_data = new T[new_cap];
            for (size_t i = 0; i < count; i++) new_data[i] = data[i];
            delete[] data;
            data = new_data;
            capacity = new_cap;
        }
        data[count++] = item;
    }
    void clear() { count = 0; }
    void free() { delete[] data; data = nullptr; count = capacity = 0; }
    T at(size_t i) const { return data[i]; }
    T& at(size_t i) { return data[i]; }
    T operator[](size_t i) const { return data[i]; }
    T& operator[](size_t i) { return data[i]; }
    size_t size() const { return count; }
};

struct ASTNode {
    ASTKind kind;
    CType ctype;

    struct {
        int func_name_alloced;
        int var_name_alloced;
        int member_name_alloced;
        int call_name_alloced;
    } flags;

    union {
        struct { const char* name; NodeArray<ASTNodePtr> params; ASTNodePtr body; CType return_type; } func;
        struct { const char* name; CType var_type; ASTNodePtr init; bool is_extern; bool is_static; } var_decl;
        struct { NodeArray<ASTNodePtr> stmts; } block;
        struct { ASTNodePtr cond; ASTNodePtr then_body; ASTNodePtr else_body; } if_stmt;
        struct { ASTNodePtr cond; ASTNodePtr body; } while_stmt;
        struct { ASTNodePtr init; ASTNodePtr cond; ASTNodePtr inc; ASTNodePtr body; } for_stmt;
        struct { ASTNodePtr cond; ASTNodePtr body; } do_while;
        struct { ASTNodePtr expr; } ret;
        struct { ASTNodePtr expr; } expr_stmt;
        struct { TokenKind op; ASTNodePtr left; ASTNodePtr right; } binop;
        struct { TokenKind op; ASTNodePtr operand; bool is_prefix; } unop;
        struct { ASTNodePtr target; ASTNodePtr value; } assign;
        struct { const char* name; NodeArray<ASTNodePtr> args; } call;
        struct { const char* name; } var;
        struct { u64 value; } constant;
        struct { const char* value; } string_lit;
        struct { ASTNodePtr operand; } deref;
        struct { ASTNodePtr operand; } addr_of;
        struct { ASTNodePtr obj; const char* member; } member;
        struct { ASTNodePtr arr; ASTNodePtr index; } arr_idx;
        struct { ASTNodePtr expr; CType target_type; } cast_expr;
        struct { ASTNodePtr expr; } size_of;
        struct { const char* code; } asm_stmt;
        struct { const char* name; NodeArray<ASTNodePtr> members; } struct_decl;
    } u;

    ASTNode() : kind(ASTKind::PROGRAM), ctype{}, flags{}, u{} { memset(&u, 0, sizeof(u)); }
    ~ASTNode() {
        // Free string allocations if we own them
        if (flags.func_name_alloced) delete[] u.func.name;
        if (flags.var_name_alloced) delete[] u.var_decl.name;
        if (flags.member_name_alloced) delete[] u.member.member;
        if (flags.call_name_alloced) delete[] u.call.name;
        u.func.params.free();
        u.block.stmts.free();
        u.call.args.free();
        u.struct_decl.members.free();
        if (u.string_lit.value) delete[] u.string_lit.value;
        if (u.asm_stmt.code) delete[] u.asm_stmt.code;
    }
};

inline ASTNodePtr alloc_node(ASTKind kind) {
    auto ptr = new ASTNode();
    ptr->kind = kind;
    return ptr;
}

inline void free_node(ASTNodePtr node) {
    delete node;
}

} // namespace hvmc
