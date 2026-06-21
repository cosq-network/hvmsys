#include <gtest/gtest.h>
#include <vector>
#include <cstring>
#include "hvm-sim/core/cpu_state.hpp"
#include "hvm-sim/core/decoder.hpp"
#include "hvm-sim/core/executor.hpp"

using namespace hvm;

// ── Flat memory implementation for tests ──────────────────────────────────

class FlatMemory : public MemoryAccess {
  std::vector<uint8_t> mem;
public:
  explicit FlatMemory(size_t size = 1024 * 64)
    : mem(size, 0) {}

  void store(size_t addr, const uint8_t* data, size_t len) {
    if (addr + len > mem.size()) mem.resize(addr + len + 4096, 0);
    std::memcpy(&mem[addr], data, len);
  }

  uint8_t  read_byte(uint64_t addr) override { return addr < mem.size() ? mem[static_cast<size_t>(addr)] : 0; }
  uint16_t read_half(uint64_t addr) override {
    if (addr + 1 >= mem.size()) return 0;
    return static_cast<uint16_t>(mem[static_cast<size_t>(addr)]) |
           (static_cast<uint16_t>(static_cast<uint16_t>(mem[static_cast<size_t>(addr + 1)]) << 8));
  }
  uint32_t read_word(uint64_t addr) override {
    if (addr + 3 >= mem.size()) return 0;
    return static_cast<uint32_t>(mem[static_cast<size_t>(addr)]) |
           (static_cast<uint32_t>(mem[static_cast<size_t>(addr + 1)]) << 8) |
           (static_cast<uint32_t>(mem[static_cast<size_t>(addr + 2)]) << 16) |
           (static_cast<uint32_t>(mem[static_cast<size_t>(addr + 3)]) << 24);
  }
  uint64_t read_dword(uint64_t addr) override {
    if (addr + 7 >= mem.size()) return 0;
    return static_cast<uint64_t>(mem[static_cast<size_t>(addr)]) |
           (static_cast<uint64_t>(mem[static_cast<size_t>(addr + 1)]) << 8) |
           (static_cast<uint64_t>(mem[static_cast<size_t>(addr + 2)]) << 16) |
           (static_cast<uint64_t>(mem[static_cast<size_t>(addr + 3)]) << 24) |
           (static_cast<uint64_t>(mem[static_cast<size_t>(addr + 4)]) << 32) |
           (static_cast<uint64_t>(mem[static_cast<size_t>(addr + 5)]) << 40) |
           (static_cast<uint64_t>(mem[static_cast<size_t>(addr + 6)]) << 48) |
           (static_cast<uint64_t>(mem[static_cast<size_t>(addr + 7)]) << 56);
  }
  void write_byte(uint64_t addr, uint8_t val) override {
    if (addr < mem.size()) mem[static_cast<size_t>(addr)] = val;
  }
  void write_half(uint64_t addr, uint16_t val) override {
    if (addr + 1 >= mem.size()) return;
    mem[static_cast<size_t>(addr)]     = static_cast<uint8_t>(val);
    mem[static_cast<size_t>(addr + 1)] = static_cast<uint8_t>(val >> 8);
  }
  void write_word(uint64_t addr, uint32_t val) override {
    if (addr + 3 >= mem.size()) return;
    mem[static_cast<size_t>(addr)]     = static_cast<uint8_t>(val);
    mem[static_cast<size_t>(addr + 1)] = static_cast<uint8_t>(val >> 8);
    mem[static_cast<size_t>(addr + 2)] = static_cast<uint8_t>(val >> 16);
    mem[static_cast<size_t>(addr + 3)] = static_cast<uint8_t>(val >> 24);
  }
  void write_dword(uint64_t addr, uint64_t val) override {
    if (addr + 7 >= mem.size()) return;
    mem[static_cast<size_t>(addr)]     = static_cast<uint8_t>(val);
    mem[static_cast<size_t>(addr + 1)] = static_cast<uint8_t>(val >> 8);
    mem[static_cast<size_t>(addr + 2)] = static_cast<uint8_t>(val >> 16);
    mem[static_cast<size_t>(addr + 3)] = static_cast<uint8_t>(val >> 24);
    mem[static_cast<size_t>(addr + 4)] = static_cast<uint8_t>(val >> 32);
    mem[static_cast<size_t>(addr + 5)] = static_cast<uint8_t>(val >> 40);
    mem[static_cast<size_t>(addr + 6)] = static_cast<uint8_t>(val >> 48);
    mem[static_cast<size_t>(addr + 7)] = static_cast<uint8_t>(val >> 56);
  }
};

// ── Float <-> bits helpers (mirrors executor.cpp) ────────────────────────

static double bits_to_double(uint64_t v) {
  double d;
  std::memcpy(&d, &v, sizeof(d));
  return d;
}

static uint64_t double_to_bits(double d) {
  uint64_t v;
  std::memcpy(&v, &d, sizeof(v));
  return v;
}

// ── Instruction encoding helpers ─────────────────────────────────────────

static uint32_t encode_R(uint32_t op, uint8_t rd, uint8_t rs1, uint8_t rs2, uint16_t func) {
  return (static_cast<uint32_t>(op) << 25) |
         (static_cast<uint32_t>(rd) << 20) |
         (static_cast<uint32_t>(rs1) << 15) |
         (static_cast<uint32_t>(rs2) << 10) |
         func;
}

static uint32_t encode_I(uint32_t op, uint8_t rd, uint8_t rs1, int16_t imm) {
  return (static_cast<uint32_t>(op) << 25) |
         (static_cast<uint32_t>(rd) << 20) |
         (static_cast<uint32_t>(rs1) << 15) |
         (static_cast<uint16_t>(imm) & 0x7FFF);
}

static uint32_t encode_B(uint32_t op, uint8_t rs1, uint8_t rs2, int16_t imm) {
  return (static_cast<uint32_t>(op) << 25) |
         (static_cast<uint32_t>(rs1) << 20) |
         (static_cast<uint32_t>(rs2) << 15) |
         (static_cast<uint16_t>(imm) & 0x7FFF);
}

static uint32_t encode_J(uint32_t op, uint8_t rd, int32_t imm) {
  return (static_cast<uint32_t>(op) << 25) |
         (static_cast<uint32_t>(rd) << 20) |
         (static_cast<uint32_t>(imm) & 0xFFFFF);
}

static void encode_uleb128(std::vector<uint8_t>& buf, uint32_t val) {
  while (val >= 0x80) {
    buf.push_back(static_cast<uint8_t>((val & 0x7F) | 0x80));
    val >>= 7;
  }
  buf.push_back(static_cast<uint8_t>(val));
}

static void emit_escape32(std::vector<uint8_t>& buf, uint32_t opcode, uint32_t payload) {
  buf.push_back(0xFE);
  encode_uleb128(buf, opcode);
  while (buf.size() < 4) buf.push_back(0x00);
  buf.push_back(static_cast<uint8_t>(payload));
  buf.push_back(static_cast<uint8_t>(payload >> 8));
  buf.push_back(static_cast<uint8_t>(payload >> 16));
  buf.push_back(static_cast<uint8_t>(payload >> 24));
}

// Convenient wrappers for escape32 payload encoding
static void emit_escape_R(std::vector<uint8_t>& buf, uint32_t op, uint8_t rd, uint8_t rs1, uint8_t rs2, uint16_t func) {
  emit_escape32(buf, op, encode_R(0, rd, rs1, rs2, func));
}
static void emit_escape_I(std::vector<uint8_t>& buf, uint32_t op, uint8_t rd, uint8_t rs1, int16_t imm) {
  emit_escape32(buf, op, encode_I(0, rd, rs1, imm));
}
static void emit_escape_B(std::vector<uint8_t>& buf, uint32_t op, uint8_t rs1, uint8_t rs2, int16_t imm) {
  emit_escape32(buf, op, encode_B(0, rs1, rs2, imm));
}
static void emit_escape_J(std::vector<uint8_t>& buf, uint32_t op, uint8_t rd, int32_t imm) {
  emit_escape32(buf, op, encode_J(0, rd, imm));
}

// ── Test fixture ─────────────────────────────────────────────────────────

struct CpuTest : ::testing::Test {
  FlatMemory mem;
  HvmCpuState state;

  void SetUp() override {
    state.reset();
  }

  // Place 4-byte base32 instruction at PC and step
  void exec32(uint32_t instr) {
    mem.write_word(state.pc, instr);
    step(state, mem);
  }

  // Place 8-byte escape32 instruction at PC and step
  void execEsc(const std::vector<uint8_t>& bytes) {
    for (size_t i = 0; i < bytes.size(); i++)
      mem.write_byte(state.pc + i, bytes[i]);
    step(state, mem);
  }

  void set_reg(unsigned idx, uint64_t val) { state.write_reg(idx, val); }
  uint64_t get_reg(unsigned idx) { return state.read_reg(idx); }
};

// ══════════════════════════════════════════════════════════════════════════
//  DATA MOVEMENT
// ══════════════════════════════════════════════════════════════════════════

TEST_F(CpuTest, NOP) {
  state.pc = 0x1000;
  exec32(encode_R(0x00, 0, 0, 0, 0));
  EXPECT_EQ(state.pc, 0x1004);
}

TEST_F(CpuTest, MOV) {
  set_reg(5, 0xDEADBEEFCAFEBABEULL);
  state.pc = 0;
  exec32(encode_R(0x01, 10, 5, 0, 0)); // r10 = r5
  EXPECT_EQ(get_reg(10), 0xDEADBEEFCAFEBABEULL);
}

TEST_F(CpuTest, MOV_r0_writes_discarded) {
  set_reg(5, 42);
  state.pc = 0;
  exec32(encode_R(0x01, 0, 5, 0, 0)); // r0 = r5 (should discard)
  EXPECT_EQ(get_reg(0), 0);
}

TEST_F(CpuTest, MOVZ) {
  set_reg(5, 0xFFFFFFFFFFFF0000ULL);
  state.pc = 0;
  exec32(encode_I(0x03, 10, 5, 0x7FFF)); // OR with 0x7FFF
  EXPECT_EQ(get_reg(10), 0xFFFFFFFFFFFF7FFFULL);
}

TEST_F(CpuTest, LUI) {
  set_reg(5, 0);
  state.pc = 0;
  exec32(encode_I(0x04, 10, 5, 0x1234)); // OR with 0x1234 << 49
  EXPECT_EQ(get_reg(10), 0x1234ULL << 49);
}

TEST_F(CpuTest, ADDI) {
  set_reg(5, 100);
  state.pc = 0;
  exec32(encode_I(0x05, 10, 5, -50)); // r10 = r5 + (-50)
  EXPECT_EQ(get_reg(10), 50ULL);
}

TEST_F(CpuTest, ADDI_negative_imm) {
  set_reg(5, 10);
  state.pc = 0;
  exec32(encode_I(0x05, 10, 5, -20)); // r10 = r5 + (-20) => -10
  EXPECT_EQ(get_reg(10), static_cast<uint64_t>(-10));
}

// ══════════════════════════════════════════════════════════════════════════
//  INTEGER ARITHMETIC
// ══════════════════════════════════════════════════════════════════════════

TEST_F(CpuTest, ADD) {
  set_reg(2, 10); set_reg(3, 20);
  state.pc = 0;
  exec32(encode_R(0x10, 1, 2, 3, 0));
  EXPECT_EQ(get_reg(1), 30ULL);
}

TEST_F(CpuTest, SUB) {
  set_reg(2, 30); set_reg(3, 10);
  state.pc = 0;
  exec32(encode_R(0x10, 1, 2, 3, 1));
  EXPECT_EQ(get_reg(1), 20ULL);
}

TEST_F(CpuTest, MUL) {
  set_reg(2, 1000); set_reg(3, 2000);
  state.pc = 0;
  exec32(encode_R(0x10, 1, 2, 3, 2));
  EXPECT_EQ(get_reg(1), 2000000ULL);
}

TEST_F(CpuTest, DIV) {
  set_reg(2, 100); set_reg(3, 7);
  state.pc = 0;
  exec32(encode_R(0x10, 1, 2, 3, 5));
  EXPECT_EQ(get_reg(1), 14ULL); // 100/7 = 14
}

TEST_F(CpuTest, DIV_negative) {
  set_reg(2, static_cast<uint64_t>(-100)); set_reg(3, 7);
  state.pc = 0;
  exec32(encode_R(0x10, 1, 2, 3, 5));
  EXPECT_EQ(get_reg(1), static_cast<uint64_t>(-14));
}

TEST_F(CpuTest, DIV_by_zero) {
  set_reg(2, 100); set_reg(3, 0);
  state.pc = 0;
  exec32(encode_R(0x10, 1, 2, 3, 5));
  EXPECT_EQ(get_reg(1), static_cast<uint64_t>(-1));
}

TEST_F(CpuTest, DIVU) {
  set_reg(2, 100); set_reg(3, 7);
  state.pc = 0;
  exec32(encode_R(0x10, 1, 2, 3, 6));
  EXPECT_EQ(get_reg(1), 14ULL);
}

TEST_F(CpuTest, DIVU_by_zero) {
  set_reg(2, 100); set_reg(3, 0);
  state.pc = 0;
  exec32(encode_R(0x10, 1, 2, 3, 6));
  EXPECT_EQ(get_reg(1), static_cast<uint64_t>(-1));
}

TEST_F(CpuTest, REM) {
  set_reg(2, 100); set_reg(3, 7);
  state.pc = 0;
  exec32(encode_R(0x10, 1, 2, 3, 7));
  EXPECT_EQ(get_reg(1), 2ULL); // 100 % 7 = 2
}

TEST_F(CpuTest, REM_by_zero_returns_dividend) {
  set_reg(2, 42); set_reg(3, 0);
  state.pc = 0;
  exec32(encode_R(0x10, 1, 2, 3, 7));
  EXPECT_EQ(get_reg(1), 42ULL);
}

// ══════════════════════════════════════════════════════════════════════════
//  SHIFTS
// ══════════════════════════════════════════════════════════════════════════

TEST_F(CpuTest, SHL) {
  set_reg(2, 1); set_reg(3, 10);
  state.pc = 0;
  exec32(encode_R(0x13, 1, 2, 3, 0));
  EXPECT_EQ(get_reg(1), 1024ULL);
}

TEST_F(CpuTest, SHR) {
  set_reg(2, 0xFF00); set_reg(3, 8);
  state.pc = 0;
  exec32(encode_R(0x13, 1, 2, 3, 1));
  EXPECT_EQ(get_reg(1), 0xFFULL);
}

TEST_F(CpuTest, SAR) {
  set_reg(2, static_cast<uint64_t>(-256)); set_reg(3, 4);
  state.pc = 0;
  exec32(encode_R(0x13, 1, 2, 3, 2));
  EXPECT_EQ(get_reg(1), static_cast<uint64_t>(-16));
}

// ══════════════════════════════════════════════════════════════════════════
//  BITWISE
// ══════════════════════════════════════════════════════════════════════════

TEST_F(CpuTest, AND) {
  set_reg(2, 0xFF00FF00ULL); set_reg(3, 0x0F0F0F0FULL);
  state.pc = 0;
  exec32(encode_R(0x20, 1, 2, 3, 0));
  EXPECT_EQ(get_reg(1), 0x0F000F00ULL);
}

TEST_F(CpuTest, OR) {
  set_reg(2, 0xFF00ULL); set_reg(3, 0x00FFULL);
  state.pc = 0;
  exec32(encode_R(0x20, 1, 2, 3, 1));
  EXPECT_EQ(get_reg(1), 0xFFFFULL);
}

TEST_F(CpuTest, XOR) {
  set_reg(2, 0xFF00FF00ULL); set_reg(3, 0xFFFFFFFFULL);
  state.pc = 0;
  exec32(encode_R(0x20, 1, 2, 3, 2));
  EXPECT_EQ(get_reg(1), 0x00FF00FFULL);
}

TEST_F(CpuTest, NOT) {
  set_reg(2, 0xAAAAAAAAAAAAAAAAULL);
  state.pc = 0;
  exec32(encode_R(0x21, 1, 2, 0, 0));
  EXPECT_EQ(get_reg(1), 0x5555555555555555ULL);
}

// ══════════════════════════════════════════════════════════════════════════
//  FLOATING POINT
// ══════════════════════════════════════════════════════════════════════════

TEST_F(CpuTest, FADD) {
  double a = 1.5, b = 2.25;
  uint64_t ra, rb;
  std::memcpy(&ra, &a, 8); std::memcpy(&rb, &b, 8);
  set_reg(2, ra); set_reg(3, rb);
  state.pc = 0;
  exec32(encode_R(0x30, 1, 2, 3, 0));
  uint64_t res = get_reg(1);
  double result;
  std::memcpy(&result, &res, 8);
  EXPECT_DOUBLE_EQ(result, 3.75);
}

TEST_F(CpuTest, FSUB) {
  double a = 5.0, b = 2.0;
  uint64_t ra, rb;
  std::memcpy(&ra, &a, 8); std::memcpy(&rb, &b, 8);
  set_reg(2, ra); set_reg(3, rb);
  state.pc = 0;
  exec32(encode_R(0x30, 1, 2, 3, 1));
  uint64_t res = get_reg(1);
  double result;
  std::memcpy(&result, &res, 8);
  EXPECT_DOUBLE_EQ(result, 3.0);
}

TEST_F(CpuTest, FMUL) {
  double a = 3.0, b = 4.0;
  uint64_t ra, rb;
  std::memcpy(&ra, &a, 8); std::memcpy(&rb, &b, 8);
  set_reg(2, ra); set_reg(3, rb);
  state.pc = 0;
  exec32(encode_R(0x30, 1, 2, 3, 2));
  uint64_t res = get_reg(1);
  double result;
  std::memcpy(&result, &res, 8);
  EXPECT_DOUBLE_EQ(result, 12.0);
}

TEST_F(CpuTest, FDIV) {
  double a = 10.0, b = 4.0;
  uint64_t ra, rb;
  std::memcpy(&ra, &a, 8); std::memcpy(&rb, &b, 8);
  set_reg(2, ra); set_reg(3, rb);
  state.pc = 0;
  exec32(encode_R(0x30, 1, 2, 3, 3));
  uint64_t res = get_reg(1);
  double result;
  std::memcpy(&result, &res, 8);
  EXPECT_DOUBLE_EQ(result, 2.5);
}

// ══════════════════════════════════════════════════════════════════════════
//  COMPARISONS
// ══════════════════════════════════════════════════════════════════════════

TEST_F(CpuTest, CMPEQ_true) {
  set_reg(2, 42); set_reg(3, 42);
  state.pc = 0;
  exec32(encode_R(0x40, 1, 2, 3, 0));
  EXPECT_EQ(get_reg(1), 1ULL);
}

TEST_F(CpuTest, CMPEQ_false) {
  set_reg(2, 42); set_reg(3, 43);
  state.pc = 0;
  exec32(encode_R(0x40, 1, 2, 3, 0));
  EXPECT_EQ(get_reg(1), 0ULL);
}

TEST_F(CpuTest, CMPNE_true) {
  set_reg(2, 1); set_reg(3, 2);
  state.pc = 0;
  exec32(encode_R(0x40, 1, 2, 3, 1));
  EXPECT_EQ(get_reg(1), 1ULL);
}

TEST_F(CpuTest, CMPLT_signed) {
  set_reg(2, static_cast<uint64_t>(-5)); set_reg(3, 10);
  state.pc = 0;
  exec32(encode_R(0x40, 1, 2, 3, 2));
  EXPECT_EQ(get_reg(1), 1ULL);
}

TEST_F(CpuTest, CMPLE_signed) {
  set_reg(2, 5); set_reg(3, 5);
  state.pc = 0;
  exec32(encode_R(0x40, 1, 2, 3, 3));
  EXPECT_EQ(get_reg(1), 1ULL);
}

TEST_F(CpuTest, FCMPEQ) {
  double a = 3.14, b = 3.14;
  uint64_t ra, rb;
  std::memcpy(&ra, &a, 8); std::memcpy(&rb, &b, 8);
  set_reg(2, ra); set_reg(3, rb);
  state.pc = 0;
  exec32(encode_R(0x41, 1, 2, 3, 0));
  EXPECT_EQ(get_reg(1), 1ULL);
}

TEST_F(CpuTest, FCMPLT) {
  double a = 1.0, b = 2.0;
  uint64_t ra, rb;
  std::memcpy(&ra, &a, 8); std::memcpy(&rb, &b, 8);
  set_reg(2, ra); set_reg(3, rb);
  state.pc = 0;
  exec32(encode_R(0x41, 1, 2, 3, 1));
  EXPECT_EQ(get_reg(1), 1ULL);
}

TEST_F(CpuTest, FCMPLE) {
  double a = 2.0, b = 2.0;
  uint64_t ra, rb;
  std::memcpy(&ra, &a, 8); std::memcpy(&rb, &b, 8);
  set_reg(2, ra); set_reg(3, rb);
  state.pc = 0;
  exec32(encode_R(0x41, 1, 2, 3, 2));
  EXPECT_EQ(get_reg(1), 1ULL);
}

// ══════════════════════════════════════════════════════════════════════════
//  BRANCHES
// ══════════════════════════════════════════════════════════════════════════

TEST_F(CpuTest, BEQ_taken) {
  set_reg(1, 10); set_reg(2, 10);
  state.pc = 0x1000;
  exec32(encode_B(0x50, 1, 2, 8)); // if r1==r2, pc += 8*4 = 32
  EXPECT_EQ(state.pc, 0x1020ULL);
}

TEST_F(CpuTest, BEQ_not_taken) {
  set_reg(1, 10); set_reg(2, 20);
  state.pc = 0x1000;
  exec32(encode_B(0x50, 1, 2, 8));
  EXPECT_EQ(state.pc, 0x1004ULL);
}

TEST_F(CpuTest, BNE_taken) {
  set_reg(1, 10); set_reg(2, 20);
  state.pc = 0x1000;
  exec32(encode_B(0x51, 1, 2, -4));
  EXPECT_EQ(state.pc, 0x0FF0ULL); // 0x1000 + (-4)*4 = 0x1000 - 16 = 0xFF0
}

TEST_F(CpuTest, BLT_taken) {
  set_reg(1, static_cast<uint64_t>(-10)); set_reg(2, 0);
  state.pc = 0x1000;
  exec32(encode_B(0x52, 1, 2, 4));
  EXPECT_EQ(state.pc, 0x1010ULL);
}

TEST_F(CpuTest, BLE_taken) {
  set_reg(1, 10); set_reg(2, 10);
  state.pc = 0x1000;
  exec32(encode_B(0x53, 1, 2, 4));
  EXPECT_EQ(state.pc, 0x1010ULL);
}

TEST_F(CpuTest, BLE_not_taken) {
  set_reg(1, 11); set_reg(2, 10);
  state.pc = 0x1000;
  exec32(encode_B(0x53, 1, 2, 4));
  EXPECT_EQ(state.pc, 0x1004ULL);
}

// ══════════════════════════════════════════════════════════════════════════
//  JUMPS
// ══════════════════════════════════════════════════════════════════════════

TEST_F(CpuTest, JMP) {
  state.pc = 0x1000;
  exec32(encode_J(0x60, 0, 100)); // pc += 100*4 = 400
  EXPECT_EQ(state.pc, 0x1190ULL);
}

TEST_F(CpuTest, JMP_backward) {
  state.pc = 0x1000;
  exec32(encode_J(0x60, 0, -10));
  EXPECT_EQ(state.pc, 0x0FD8ULL); // 0x1000 - 40 = 0xFD8, aligned
}

TEST_F(CpuTest, JAL) {
  state.pc = 0x1000;
  exec32(encode_J(0x61, 29, 50)); // r29 = pc+4; pc += 50*4
  EXPECT_EQ(get_reg(29), 0x1004ULL);
  EXPECT_EQ(state.pc, 0x10C8ULL); // 0x1000 + 200 = 0x10C8
}

TEST_F(CpuTest, JALR) {
  set_reg(1, 0x2000);
  state.pc = 0x1000;
  exec32(encode_I(0x62, 29, 1, 8)); // r29 = pc+4; pc = r1 + 8
  EXPECT_EQ(get_reg(29), 0x1004ULL);
  EXPECT_EQ(state.pc, 0x2008ULL);
}

TEST_F(CpuTest, RET) {
  set_reg(29, 0x3000);
  state.pc = 0x1000;
  exec32(encode_R(0x63, 0, 0, 0, 0));
  EXPECT_EQ(state.pc, 0x3000ULL);
}

// ══════════════════════════════════════════════════════════════════════════
//  MEMORY LOAD / STORE
// ══════════════════════════════════════════════════════════════════════════

TEST_F(CpuTest, LD_B) {
  mem.write_byte(0x1008, 0xFE);
  set_reg(1, 0x1000);
  state.pc = 0;
  exec32(encode_I(0x70, 10, 1, 8));
  EXPECT_EQ(get_reg(10), static_cast<uint64_t>(-2)); // sign-extended -2
}

TEST_F(CpuTest, LD_BU) {
  mem.write_byte(0x1008, 0xFE);
  set_reg(1, 0x1000);
  state.pc = 0;
  exec32(encode_I(0x71, 10, 1, 8));
  EXPECT_EQ(get_reg(10), 0xFEULL);
}

TEST_F(CpuTest, LD_H) {
  mem.write_half(0x1008, static_cast<uint16_t>(0x8001));
  set_reg(1, 0x1000);
  state.pc = 0;
  exec32(encode_I(0x72, 10, 1, 8));
  EXPECT_EQ(get_reg(10), static_cast<uint64_t>(-32767));
}

TEST_F(CpuTest, LD_HU) {
  mem.write_half(0x1008, 0xFEED);
  set_reg(1, 0x1000);
  state.pc = 0;
  exec32(encode_I(0x73, 10, 1, 8));
  EXPECT_EQ(get_reg(10), 0xFEEDULL);
}

TEST_F(CpuTest, LD_W) {
  mem.write_word(0x1008, 0x80000005);
  set_reg(1, 0x1000);
  state.pc = 0;
  exec32(encode_I(0x74, 10, 1, 8));
  EXPECT_EQ(get_reg(10), static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(0x80000005))));
}

TEST_F(CpuTest, LD_WU) {
  mem.write_word(0x1008, 0xDEADBEEF);
  set_reg(1, 0x1000);
  state.pc = 0;
  exec32(encode_I(0x75, 10, 1, 8));
  EXPECT_EQ(get_reg(10), 0xDEADBEEFULL);
}

TEST_F(CpuTest, LD_D) {
  mem.write_dword(0x1008, 0xBADF00DCAFEBABEULL);
  set_reg(1, 0x1000);
  state.pc = 0;
  exec32(encode_I(0x76, 10, 1, 8));
  EXPECT_EQ(get_reg(10), 0xBADF00DCAFEBABEULL);
}

TEST_F(CpuTest, LD_P) {
  mem.write_dword(0x1000, 0x1111111111111111ULL);
  mem.write_dword(0x1008, 0x2222222222222222ULL);
  set_reg(2, 0x1000);
  state.pc = 0;
  exec32(encode_R(0x77, 1, 5, 2, 0)); // r1=mem[0x1000], r5=mem[0x1008]
  EXPECT_EQ(get_reg(1), 0x1111111111111111ULL);
  EXPECT_EQ(get_reg(5), 0x2222222222222222ULL);
}

TEST_F(CpuTest, ST_B) {
  set_reg(1, 0xAB); set_reg(2, 0x2000);
  state.pc = 0;
  exec32(encode_I(0x78, 1, 2, 4)); // mem[0x2004] = 0xAB
  EXPECT_EQ(mem.read_byte(0x2004), 0xAB);
}

TEST_F(CpuTest, ST_H) {
  set_reg(1, 0xAABB); set_reg(2, 0x2000);
  state.pc = 0;
  exec32(encode_I(0x79, 1, 2, 4));
  EXPECT_EQ(mem.read_half(0x2004), 0xAABB);
}

TEST_F(CpuTest, ST_W) {
  set_reg(1, 0xDEADBEEF); set_reg(2, 0x2000);
  state.pc = 0;
  exec32(encode_I(0x7A, 1, 2, 4));
  EXPECT_EQ(mem.read_word(0x2004), 0xDEADBEEF);
}

TEST_F(CpuTest, ST_D) {
  set_reg(1, 0xBADF00DCAFEBABEULL); set_reg(2, 0x2000);
  state.pc = 0;
  exec32(encode_I(0x7B, 1, 2, 4));
  EXPECT_EQ(mem.read_dword(0x2004), 0xBADF00DCAFEBABEULL);
}

TEST_F(CpuTest, ST_P) {
  set_reg(1, 0xAAAAAAAAAAAAAAAAULL); set_reg(5, 0xBBBBBBBBBBBBBBBBULL);
  set_reg(2, 0x3000);
  state.pc = 0;
  exec32(encode_R(0x7C, 1, 5, 2, 0)); // mem[r2]=r1, mem[r2+8]=r5
  EXPECT_EQ(mem.read_dword(0x3000), 0xAAAAAAAAAAAAAAAAULL);
  EXPECT_EQ(mem.read_dword(0x3008), 0xBBBBBBBBBBBBBBBBULL);
}

TEST_F(CpuTest, LDA) {
  set_reg(1, 0x1000);
  state.pc = 0;
  exec32(encode_I(0x7D, 10, 1, 64));
  EXPECT_EQ(get_reg(10), 0x1040ULL);
}

// ══════════════════════════════════════════════════════════════════════════
//  STACK / FRAME
// ══════════════════════════════════════════════════════════════════════════

TEST_F(CpuTest, PUSH_POP) {
  set_reg(31, 0x8000);
  set_reg(5, 0xCAFEBABE);
  state.pc = 0;
  exec32(encode_R(0x7E, 5, 0, 0, 0)); // push r5: sp -= 8, [sp] = r5
  EXPECT_EQ(state.read_reg(31), 0x7FF8ULL);
  EXPECT_EQ(mem.read_dword(0x7FF8), 0xCAFEBABE);

  state.pc = state.pc; // PC now at 4
  exec32(encode_R(0x7F, 10, 0, 0, 0)); // pop r10: r10 = [sp], sp += 8
  EXPECT_EQ(get_reg(10), 0xCAFEBABE);
  EXPECT_EQ(state.read_reg(31), 0x8000ULL);
}

TEST_F(CpuTest, ENTER_LEAVE) {
  // ENTER imm15: sp -= 16; mem[sp] = r29; mem[sp+8] = r30; r30 = sp; sp -= imm15
  set_reg(29, 0x4000); // return address
  set_reg(30, 0x5000); // old frame pointer
  set_reg(31, 0x6000); // sp
  state.pc = 0;

  std::vector<uint8_t> enc;
  emit_escape_I(enc, 0x80, 0, 0, 32); // ENTER 32
  execEsc(enc);

  EXPECT_EQ(state.read_reg(31), 0x5FD0ULL); // 0x6000 - 16 - 32 = 0x5FD0
  EXPECT_EQ(state.read_reg(30), 0x5FF0ULL); // r30 = sp before alloc = 0x6000-16 = 0x5FF0
  EXPECT_EQ(mem.read_dword(0x5FF0), 0x4000); // saved r29
  EXPECT_EQ(mem.read_dword(0x5FF8), 0x5000); // saved r30

  // LEAVE: sp = r30; r29 = [sp]; r30 = [sp+8]; sp += 16
  state.pc = state.pc;
  std::vector<uint8_t> enc2;
  emit_escape_R(enc2, 0x81, 0, 0, 0, 0);
  execEsc(enc2);

  EXPECT_EQ(get_reg(29), 0x4000);
  EXPECT_EQ(get_reg(30), 0x5000);
  EXPECT_EQ(state.read_reg(31), 0x6000ULL); // 0x5FF0 + 16 = 0x6000
}

TEST_F(CpuTest, ADJSP) {
  set_reg(31, 0x8000);
  state.pc = 0;
  std::vector<uint8_t> enc;
  emit_escape_I(enc, 0x82, 0, 0, -64); // sp -= 64
  execEsc(enc);
  EXPECT_EQ(state.read_reg(31), 0x7FC0ULL);
}

TEST_F(CpuTest, FRAME) {
  set_reg(30, 0x5000);
  state.pc = 0;
  std::vector<uint8_t> enc;
  emit_escape_I(enc, 0x83, 10, 0, -16); // r10 = r30 + (-16)
  execEsc(enc);
  EXPECT_EQ(get_reg(10), 0x4FF0ULL);
}

// ══════════════════════════════════════════════════════════════════════════
//  CALLS (escape32)
// ══════════════════════════════════════════════════════════════════════════

TEST_F(CpuTest, CALL) {
  state.pc = 0x1000;
  std::vector<uint8_t> enc;
  emit_escape_J(enc, 0xB4, 29, 100); // r29 = pc+4; pc += 100*4
  execEsc(enc);
  EXPECT_EQ(get_reg(29), 0x1008ULL); // escape32 = 8 bytes
  EXPECT_EQ(state.pc, 0x1190ULL);
}

TEST_F(CpuTest, TAILCALL) {
  state.pc = 0x1000;
  std::vector<uint8_t> enc;
  emit_escape_J(enc, 0xB6, 0, -20); // pc += (-20)*4
  execEsc(enc);
  EXPECT_EQ(state.pc, 0x0FB0ULL);
}

// ══════════════════════════════════════════════════════════════════════════
//  SYSTEM / PRIVILEGED
// ══════════════════════════════════════════════════════════════════════════

TEST_F(CpuTest, SYSCALL) {
  set_reg(2, 42);
  state.pc = 0;
  std::vector<uint8_t> enc;
  emit_escape_I(enc, 0xC0, 1, 0, 5); // syscall 5 (kSysTypeId)
  execEsc(enc);
  EXPECT_EQ(state.pc, 8ULL); // escape32 advances by 8
}

TEST_F(CpuTest, BREAK_triggers_trap) {
  state.pc = 0;
  state.stvec = 0x1000;
  std::vector<uint8_t> enc;
  emit_escape_R(enc, 0xC1, 0, 0, 0, 0);
  execEsc(enc);
  EXPECT_TRUE(state.trap_pending);
  EXPECT_EQ(state.scause, 10ULL); // breakpoint
  EXPECT_EQ(state.sepc, 0ULL);
  EXPECT_EQ(state.pc, 0x1000ULL); // trap handler
}

TEST_F(CpuTest, ECALL_triggers_trap) {
  state.pc = 0x2000;
  state.stvec = 0x8000;
  std::vector<uint8_t> enc;
  emit_escape_R(enc, 0xC2, 0, 0, 0, 0);
  execEsc(enc);
  EXPECT_TRUE(state.trap_pending);
  EXPECT_EQ(state.scause, 8ULL); // ecall from u-mode
  EXPECT_EQ(state.sepc, 0x2000ULL);
  EXPECT_EQ(state.pc, 0x8000ULL);
}

TEST_F(CpuTest, TRAPRET) {
  state.sepc = 0x4000;
  state.sstatus = 0x100; // SPP=1 (supervisor), SIE=0, SPIE=1
  state.pc = 0x3000;
  state.privilege = PrivilegeLevel::kSupervisor;
  std::vector<uint8_t> enc;
  emit_escape_R(enc, 0xC3, 0, 0, 0, 0);
  execEsc(enc);
  EXPECT_EQ(state.pc, 0x4000ULL);
  EXPECT_EQ(state.privilege, PrivilegeLevel::kSupervisor);
  EXPECT_EQ(state.sstatus & 1, 0ULL); // SIE = old SPIE = 1... wait SPIE is bit 1
  // Actually: TRAPRET sets SIE = sstatus.SPIE. SPIE is bit 1.
  // sstatus was 0x100, so bit 1 = 0. SPP = 1. After trapret:
  // SIE = 0, privilege = Supervisor (SPP=1).
  EXPECT_EQ(state.privilege, PrivilegeLevel::kSupervisor);
}

// ══════════════════════════════════════════════════════════════════════════
//  CSR ACCESS
// ══════════════════════════════════════════════════════════════════════════

TEST_F(CpuTest, CSRRW_read_write) {
  state.satp = 0xDEAD;
  set_reg(5, 0xBEEF);
  state.pc = 0;
  std::vector<uint8_t> enc;
  emit_escape_I(enc, 0xC6, 1, 5, 0x005); // r1 = CSR[0x005]; CSR[0x005] = r5
  execEsc(enc);
  EXPECT_EQ(get_reg(1), 0xDEADULL);
  EXPECT_EQ(state.satp, 0xBEEFULL);
}

TEST_F(CpuTest, CSRRW_read_only_suppressed) {
  state.stime = 0x1234;
  set_reg(5, 0x5678);
  state.pc = 0;
  std::vector<uint8_t> enc;
  emit_escape_I(enc, 0xC6, 1, 5, 0x006); // r1 = stime; write suppressed because stime is read-only
  execEsc(enc);
  EXPECT_EQ(get_reg(1), 0x1234ULL);
  EXPECT_EQ(state.stime, 0x1235ULL); // tick_timer incremented after instruction
}

TEST_F(CpuTest, CSRRW_read_only_with_r0) {
  state.satp = 0xCAFE;
  state.pc = 0;
  std::vector<uint8_t> enc;
  emit_escape_I(enc, 0xC6, 1, 0, 0x005); // r1 = satp, write suppressed (rs=r0)
  execEsc(enc);
  EXPECT_EQ(get_reg(1), 0xCAFEULL);
  EXPECT_EQ(state.satp, 0xCAFEULL); // unchanged
}

// ══════════════════════════════════════════════════════════════════════════
//  ATOMICS
// ══════════════════════════════════════════════════════════════════════════

TEST_F(CpuTest, LR_D_SC_D_success) {
  mem.write_dword(0x1000, 0x42);
  set_reg(2, 0x1000);
  set_reg(3, 0xDEAD);
  state.pc = 0;

  // LR.D r1, (r2) — load-reserve
  std::vector<uint8_t> enc;
  emit_escape_R(enc, 0xC4, 1, 2, 0, 0);
  execEsc(enc);
  EXPECT_EQ(get_reg(1), 0x42ULL);
  EXPECT_TRUE(state.reservation_valid);
  EXPECT_EQ(state.reservation_addr, 0x1000ULL);

  // SC.D r4, r2, r3 — store-conditional: *(r2) = r3; r4 = 0 if success
  state.pc = state.pc;
  std::vector<uint8_t> enc2;
  emit_escape_R(enc2, 0xC5, 4, 2, 3, 0);
  execEsc(enc2);
  EXPECT_EQ(get_reg(4), 0ULL); // success
  EXPECT_EQ(mem.read_dword(0x1000), 0xDEADULL);
}

TEST_F(CpuTest, LR_D_SC_D_fail_no_reservation) {
  mem.write_dword(0x1000, 0x42);
  set_reg(2, 0x1000);
  set_reg(3, 0xBEEF);
  state.pc = 0;

  // SC.D without LR — should fail
  std::vector<uint8_t> enc;
  emit_escape_R(enc, 0xC5, 4, 2, 3, 0);
  execEsc(enc);
  EXPECT_EQ(get_reg(4), 1ULL); // failure
  EXPECT_EQ(mem.read_dword(0x1000), 0x42ULL); // unchanged
}

// ══════════════════════════════════════════════════════════════════════════
//  SFENCE.VMA
// ══════════════════════════════════════════════════════════════════════════

TEST_F(CpuTest, SFENCE_VMA) {
  state.pc = 0;
  std::vector<uint8_t> enc;
  emit_escape_R(enc, 0xC8, 0, 0, 0, 0);
  execEsc(enc);
  EXPECT_EQ(state.pc, 8ULL);
}

// ══════════════════════════════════════════════════════════════════════════
//  HARDWARE LOOP
// ══════════════════════════════════════════════════════════════════════════

TEST_F(CpuTest, LOOP_SET_DECBR) {
  set_reg(5, 3); // count = 3
  state.pc = 0;
  std::vector<uint8_t> enc;
  emit_escape_I(enc, 0xD0, 0, 5, -4); // loop_count = 3, backedge = -4
  execEsc(enc);
  EXPECT_EQ(state.loop_count, 3ULL);
  EXPECT_EQ(state.loop_backedge, static_cast<uint64_t>(-4));

  // LOOP.DECBR: count--, if count != 0, pc += backedge*4
  state.pc = 0x1000;
  std::vector<uint8_t> enc2;
  emit_escape_B(enc2, 0xD1, 0, 0, -4); // loop.decbr -4
  execEsc(enc2);
  EXPECT_EQ(state.loop_count, 2ULL);
  EXPECT_EQ(state.pc, 0x0FF0ULL); // 0x1000 + (-4)*4 = 0xFF0
}

TEST_F(CpuTest, LOOP_DECBR_falls_through_when_zero) {
  state.loop_count = 1;
  state.pc = 0x1000;
  std::vector<uint8_t> enc;
  emit_escape_B(enc, 0xD1, 0, 0, -4);
  execEsc(enc);
  EXPECT_EQ(state.loop_count, 0ULL);
  EXPECT_EQ(state.pc, 0x1008ULL); // escaped fall-through
}

// ══════════════════════════════════════════════════════════════════════════
//  PREFETCH / HINT (all no-ops in Phase 1)
// ══════════════════════════════════════════════════════════════════════════

TEST_F(CpuTest, PREFETCH_R) {
  set_reg(1, 0x1000);
  state.pc = 0;
  std::vector<uint8_t> enc; emit_escape_I(enc, 0xD2, 0, 1, 64);
  execEsc(enc);
  EXPECT_EQ(state.pc, 8ULL);
}

TEST_F(CpuTest, PREFETCH_W) {
  state.pc = 0;
  std::vector<uint8_t> enc; emit_escape_I(enc, 0xD3, 0, 0, 0);
  execEsc(enc);
  EXPECT_EQ(state.pc, 8ULL);
}

TEST_F(CpuTest, PREFETCH_NTA) {
  state.pc = 0;
  std::vector<uint8_t> enc; emit_escape_I(enc, 0xD4, 0, 0, 0);
  execEsc(enc);
  EXPECT_EQ(state.pc, 8ULL);
}

TEST_F(CpuTest, MEMZERO_HINT) {
  state.pc = 0;
  std::vector<uint8_t> enc; emit_escape_R(enc, 0xD5, 0, 0, 0, 0);
  execEsc(enc);
  EXPECT_EQ(state.pc, 8ULL);
}

TEST_F(CpuTest, BR_HINT) {
  state.pc = 0;
  std::vector<uint8_t> enc; emit_escape_B(enc, 0xDA, 0, 0, 0);
  execEsc(enc);
  EXPECT_EQ(state.pc, 8ULL);
}

// ══════════════════════════════════════════════════════════════════════════
//  GREEN-COMPUTE: RETAIN / RELEASE / ICACHE.RNG
// ══════════════════════════════════════════════════════════════════════════

TEST_F(CpuTest, RETAIN) {
  set_reg(2, 0xCAFE);
  state.pc = 0;
  exec32(encode_R(0x06, 1, 2, 0, 0)); // r1 = r2
  EXPECT_EQ(get_reg(1), 0xCAFEULL);
}

TEST_F(CpuTest, RELEASE) {
  set_reg(2, 0xBEEF);
  state.pc = 0;
  exec32(encode_R(0x07, 1, 2, 0, 0)); // rd = 0 (no decrement to zero)
  EXPECT_EQ(get_reg(1), 0ULL);
}

TEST_F(CpuTest, ICACHE_RNG) {
  set_reg(1, 0x1000); set_reg(2, 256);
  state.pc = 0;
  exec32(encode_R(0x0B, 0, 1, 2, 0)); // r0 = base, rs1(but R encoding: rd is -, base is rs1)
  EXPECT_EQ(state.pc, 4ULL);
}

// ══════════════════════════════════════════════════════════════════════════
//  SPECIAL: ALLOC.BUMP, RDPROF, CHK.B, LD.D.NZ, DOORBELL
// ══════════════════════════════════════════════════════════════════════════

TEST_F(CpuTest, ALLOC_BUMP) {
  set_reg(2, 64);
  state.pc = 0;
  std::vector<uint8_t> enc; emit_escape_I(enc, 0xD6, 1, 2, 8);
  execEsc(enc);
  EXPECT_EQ(get_reg(1), 0ULL); // returns 0 in Phase 1
}

TEST_F(CpuTest, RDPROF) {
  set_reg(2, 0);
  state.pc = 0;
  std::vector<uint8_t> enc; emit_escape_I(enc, 0xD7, 1, 2, 0);
  execEsc(enc);
  EXPECT_EQ(get_reg(1), 0ULL); // returns 0 in Phase 1
}

TEST_F(CpuTest, CHK_B) {
  set_reg(2, 0xABCD); set_reg(3, 0x100);
  state.pc = 0;
  std::vector<uint8_t> enc; emit_escape_R(enc, 0xD8, 1, 2, 3, 0);
  execEsc(enc);
  EXPECT_EQ(get_reg(1), 0xABCDULL); // passes through ptr
}

TEST_F(CpuTest, LD_D_NZ_normal) {
  mem.write_dword(0x2008, 0xCAFEBABE);
  set_reg(2, 0x2000);
  state.pc = 0;
  std::vector<uint8_t> enc; emit_escape_I(enc, 0xD9, 1, 2, 8);
  execEsc(enc);
  EXPECT_EQ(get_reg(1), 0xCAFEBABEULL);
}

TEST_F(CpuTest, LD_D_NZ_null_raises_trap) {
  set_reg(2, 0);
  state.pc = 0;
  state.stvec = 0x8000;
  std::vector<uint8_t> enc; emit_escape_I(enc, 0xD9, 1, 2, 8);
  execEsc(enc);
  EXPECT_TRUE(state.trap_pending);
  EXPECT_EQ(state.scause, 9ULL); // illegal instruction
}

TEST_F(CpuTest, DOORBELL) {
  set_reg(1, 0x1000); set_reg(2, 0x1);
  state.pc = 0;
  std::vector<uint8_t> enc; emit_escape_R(enc, 0xDB, 0, 1, 2, 0);
  execEsc(enc);
  EXPECT_EQ(state.pc, 8ULL);
}

// ══════════════════════════════════════════════════════════════════════════
//  VECTOR: VSETVL
// ══════════════════════════════════════════════════════════════════════════

TEST_F(CpuTest, VSETVL) {
  set_reg(2, 3); // avl = 3
  set_reg(3, 0); // vtype = 0 (64-bit elements)
  state.pc = 0;
  std::vector<uint8_t> enc; emit_escape_R(enc, 0xE0, 1, 2, 3, 0);
  execEsc(enc);
  EXPECT_EQ(get_reg(1), 3ULL); // vl = min(3, 4) = 3
  EXPECT_EQ(state.vl, 3ULL);
  EXPECT_EQ(state.vstate, 2ULL); // Dirty
}

TEST_F(CpuTest, VSETVL_clamps) {
  set_reg(2, 100); // avl > MAXVL
  state.pc = 0;
  std::vector<uint8_t> enc; emit_escape_R(enc, 0xE0, 1, 2, 0, 0);
  execEsc(enc);
  EXPECT_EQ(get_reg(1), 4ULL); // clamped to 4
}

// ══════════════════════════════════════════════════════════════════════════
//  VECTOR: LOAD / STORE
// ══════════════════════════════════════════════════════════════════════════

TEST_F(CpuTest, VLD_V_VST_V) {
  mem.write_dword(0x1000, 10);
  mem.write_dword(0x1008, 20);
  mem.write_dword(0x1010, 30);
  mem.write_dword(0x1018, 40);
  state.vl = 4;
  set_reg(2, 0x1000);

  // VLD.V v1, (r2) — load 4 elements
  state.pc = 0;
  std::vector<uint8_t> enc; emit_escape_R(enc, 0xE1, 1, 0, 2, 0);
  execEsc(enc);
  EXPECT_EQ(state.vec_regs[1][0], 10ULL);
  EXPECT_EQ(state.vec_regs[1][1], 20ULL);
  EXPECT_EQ(state.vec_regs[1][2], 30ULL);
  EXPECT_EQ(state.vec_regs[1][3], 40ULL);

  // VST.V v1, (r3) — store to different location
  state.write_reg(3, 0x2000);
  state.pc = state.pc;
  std::vector<uint8_t> enc2; emit_escape_R(enc2, 0xE1, 1, 0, 3, 1);
  execEsc(enc2);
  EXPECT_EQ(mem.read_dword(0x2000), 10ULL);
  EXPECT_EQ(mem.read_dword(0x2008), 20ULL);
  EXPECT_EQ(mem.read_dword(0x2010), 30ULL);
  EXPECT_EQ(mem.read_dword(0x2018), 40ULL);
}

// ══════════════════════════════════════════════════════════════════════════
//  VECTOR: ARITHMETIC
// ══════════════════════════════════════════════════════════════════════════

TEST_F(CpuTest, VADD_VV) {
  state.vec_regs[1] = {1, 2, 3, 4};
  state.vec_regs[2] = {10, 20, 30, 40};
  state.pc = 0;
  std::vector<uint8_t> enc; emit_escape_R(enc, 0xE2, 3, 1, 2, 0);
  execEsc(enc);
  EXPECT_EQ(state.vec_regs[3][0], 11ULL);
  EXPECT_EQ(state.vec_regs[3][1], 22ULL);
  EXPECT_EQ(state.vec_regs[3][2], 33ULL);
  EXPECT_EQ(state.vec_regs[3][3], 44ULL);
}

TEST_F(CpuTest, VADD_VX) {
  state.vec_regs[1] = {1, 2, 3, 4};
  set_reg(2, 100);
  state.pc = 0;
  std::vector<uint8_t> enc; emit_escape_R(enc, 0xE2, 3, 1, 2, 1);
  execEsc(enc);
  EXPECT_EQ(state.vec_regs[3][0], 101ULL);
  EXPECT_EQ(state.vec_regs[3][1], 102ULL);
  EXPECT_EQ(state.vec_regs[3][2], 103ULL);
  EXPECT_EQ(state.vec_regs[3][3], 104ULL);
}

TEST_F(CpuTest, VSUB_VV) {
  state.vec_regs[1] = {10, 20, 30, 40};
  state.vec_regs[2] = {1, 2, 3, 4};
  state.pc = 0;
  std::vector<uint8_t> enc; emit_escape_R(enc, 0xE2, 3, 1, 2, 2);
  execEsc(enc);
  EXPECT_EQ(state.vec_regs[3][0], 9ULL);
  EXPECT_EQ(state.vec_regs[3][3], 36ULL);
}

TEST_F(CpuTest, VMUL_VV) {
  state.vec_regs[1] = {2, 3, 4, 5};
  state.vec_regs[2] = {10, 10, 10, 10};
  state.pc = 0;
  std::vector<uint8_t> enc; emit_escape_R(enc, 0xE2, 3, 1, 2, 4);
  execEsc(enc);
  EXPECT_EQ(state.vec_regs[3][0], 20ULL);
  EXPECT_EQ(state.vec_regs[3][3], 50ULL);
}

TEST_F(CpuTest, VDIV_VV) {
  state.vec_regs[1] = {100, 200, 300, 400};
  state.vec_regs[2] = {2, 3, 4, 5};
  state.pc = 0;
  std::vector<uint8_t> enc; emit_escape_R(enc, 0xE2, 3, 1, 2, 6);
  execEsc(enc);
  EXPECT_EQ(state.vec_regs[3][0], 50ULL);
  EXPECT_EQ(state.vec_regs[3][3], 80ULL);
}

// ══════════════════════════════════════════════════════════════════════════
//  VECTOR: FLOAT
// ══════════════════════════════════════════════════════════════════════════

TEST_F(CpuTest, VFMACC_VV) {
  state.vec_regs[1] = {double_to_bits(1.0), double_to_bits(2.0), 0, 0};
  state.vec_regs[2] = {double_to_bits(3.0), double_to_bits(4.0), 0, 0};
  state.vec_regs[3] = {double_to_bits(10.0), double_to_bits(10.0), 0, 0};
  state.pc = 0;
  std::vector<uint8_t> enc; emit_escape_R(enc, 0xE3, 3, 1, 2, 0);
  execEsc(enc);
  double r0 = bits_to_double(state.vec_regs[3][0]);
  double r1 = bits_to_double(state.vec_regs[3][1]);
  EXPECT_DOUBLE_EQ(r0, 13.0);  // 10 + 1*3
  EXPECT_DOUBLE_EQ(r1, 18.0);  // 10 + 2*4
}

// ══════════════════════════════════════════════════════════════════════════
//  VECTOR: COMPARE / MERGE / FIRST
// ══════════════════════════════════════════════════════════════════════════

TEST_F(CpuTest, VCOMP_VV) {
  state.vec_regs[1] = {1, 2, 3, 4};
  state.vec_regs[2] = {1, 0, 3, 0};
  state.pc = 0;
  std::vector<uint8_t> enc; emit_escape_R(enc, 0xE4, 3, 1, 2, 0);
  execEsc(enc);
  EXPECT_EQ(state.vec_regs[3][0], static_cast<uint64_t>(-1)); // true
  EXPECT_EQ(state.vec_regs[3][1], 0ULL);                      // false
  EXPECT_EQ(state.vec_regs[3][2], static_cast<uint64_t>(-1)); // true
  EXPECT_EQ(state.vec_regs[3][3], 0ULL);                      // false
}

TEST_F(CpuTest, VMERGE_VVM) {
  state.vec_regs[0] = {1, 0, 1, 0}; // mask (v0 = vmask/vret)
  state.vec_regs[1] = {10, 20, 30, 40}; // true values
  state.vec_regs[2] = {100, 200, 300, 400}; // false values
  state.pc = 0;
  std::vector<uint8_t> enc; emit_escape_R(enc, 0xE4, 3, 1, 2, 2);
  execEsc(enc);
  EXPECT_EQ(state.vec_regs[3][0], 10ULL);   // mask true → vs1
  EXPECT_EQ(state.vec_regs[3][1], 200ULL);   // mask false → vs2
  EXPECT_EQ(state.vec_regs[3][2], 30ULL);
  EXPECT_EQ(state.vec_regs[3][3], 400ULL);
}

TEST_F(CpuTest, VFIRST_M) {
  state.vec_regs[1] = {0, 0, 1, 0};
  state.pc = 0;
  std::vector<uint8_t> enc; emit_escape_R(enc, 0xE4, 10, 1, 0, 3);
  execEsc(enc);
  EXPECT_EQ(get_reg(10), 2ULL); // first set bit at index 2

  state.vec_regs[1] = {0, 0, 0, 0};
  state.pc = state.pc;
  std::vector<uint8_t> enc2; emit_escape_R(enc2, 0xE4, 10, 1, 0, 3);
  execEsc(enc2);
  EXPECT_EQ(get_reg(10), static_cast<uint64_t>(-1)); // not found
}

// ══════════════════════════════════════════════════════════════════════════
//  VECTOR: REDUCTION
// ══════════════════════════════════════════════════════════════════════════

TEST_F(CpuTest, VREDADD_VS) {
  state.vec_regs[1] = {1, 2, 3, 4};
  state.vec_regs[2] = {100, 0, 0, 0};
  state.pc = 0;
  std::vector<uint8_t> enc; emit_escape_R(enc, 0xE5, 3, 1, 2, 0);
  execEsc(enc);
  EXPECT_EQ(state.vec_regs[3][0], 110ULL); // 100 + 1 + 2 + 3 + 4
}

TEST_F(CpuTest, VREDMIN_VS) {
  state.vec_regs[1] = {5, 3, 9, 1};
  state.vec_regs[2] = {100, 0, 0, 0};
  state.pc = 0;
  std::vector<uint8_t> enc; emit_escape_R(enc, 0xE5, 3, 1, 2, 1);
  execEsc(enc);
  EXPECT_EQ(state.vec_regs[3][0], 1ULL);
}

TEST_F(CpuTest, VREDMAX_VS) {
  state.vec_regs[1] = {5, 3, 9, 1};
  state.vec_regs[2] = {0, 0, 0, 0};
  state.pc = 0;
  std::vector<uint8_t> enc; emit_escape_R(enc, 0xE5, 3, 1, 2, 2);
  execEsc(enc);
  EXPECT_EQ(state.vec_regs[3][0], 9ULL);
}

// ══════════════════════════════════════════════════════════════════════════
//  VECTOR: SHIFT / BITWISE
// ══════════════════════════════════════════════════════════════════════════

TEST_F(CpuTest, VSLL_VV) {
  state.vec_regs[1] = {1, 2, 4, 8};
  state.vec_regs[2] = {1, 2, 3, 4};
  state.pc = 0;
  std::vector<uint8_t> enc; emit_escape_R(enc, 0xE6, 3, 1, 2, 0);
  execEsc(enc);
  EXPECT_EQ(state.vec_regs[3][0], 2ULL);
  EXPECT_EQ(state.vec_regs[3][3], 128ULL);
}

TEST_F(CpuTest, VSRL_VV) {
  state.vec_regs[1] = {256, 512, 1024, 2048};
  state.vec_regs[2] = {1, 2, 3, 4};
  state.pc = 0;
  std::vector<uint8_t> enc; emit_escape_R(enc, 0xE6, 3, 1, 2, 2);
  execEsc(enc);
  EXPECT_EQ(state.vec_regs[3][0], 128ULL);
  EXPECT_EQ(state.vec_regs[3][3], 128ULL);
}

TEST_F(CpuTest, VAND_VV) {
  state.vec_regs[1] = {0xFF, 0x0F, 0xFF, 0x0F};
  state.vec_regs[2] = {0x0F, 0xFF, 0x0F, 0xFF};
  state.pc = 0;
  std::vector<uint8_t> enc; emit_escape_R(enc, 0xE7, 3, 1, 2, 0);
  execEsc(enc);
  EXPECT_EQ(state.vec_regs[3][0], 0x0FULL);
  EXPECT_EQ(state.vec_regs[3][3], 0x0FULL);
}

TEST_F(CpuTest, VOR_VV) {
  state.vec_regs[1] = {0xFF00, 0x00FF};
  state.vec_regs[2] = {0x00FF, 0xFF00};
  state.pc = 0;
  std::vector<uint8_t> enc; emit_escape_R(enc, 0xE7, 3, 1, 2, 1);
  execEsc(enc);
  EXPECT_EQ(state.vec_regs[3][0], 0xFFFFULL);
  EXPECT_EQ(state.vec_regs[3][1], 0xFFFFULL);
}

TEST_F(CpuTest, VXOR_VV) {
  state.vec_regs[1] = {0xFFFF, 0x0000};
  state.vec_regs[2] = {0x1234, 0x5678};
  state.pc = 0;
  std::vector<uint8_t> enc; emit_escape_R(enc, 0xE7, 3, 1, 2, 2);
  execEsc(enc);
  EXPECT_EQ(state.vec_regs[3][0], 0xEDCBULL); // 0xFFFF ^ 0x1234
  EXPECT_EQ(state.vec_regs[3][1], 0x5678ULL); // 0x0000 ^ 0x5678
}

// ══════════════════════════════════════════════════════════════════════════
//  EDGE CASES
// ══════════════════════════════════════════════════════════════════════════

TEST_F(CpuTest, r0_always_zero_on_read) {
  state.regs[0] = 0xDEAD; // force write for test
  EXPECT_EQ(state.read_reg(0), 0ULL);
}

TEST_F(CpuTest, r0_write_discarded) {
  state.write_reg(0, 0xBEEF);
  EXPECT_EQ(state.regs[0], 0ULL);
}

TEST_F(CpuTest, illegal_instruction_triggers_trap) {
  state.pc = 0;
  state.stvec = 0x4000;
  mem.write_word(0, 0x7FFFFFFF); // invalid opcode 0x7F all ones... 
  // Actually this is opcode = (0x7FFFFFFF >> 25) & 0x7F = 0x3F = 63
  // 63 = RET (opcode 0x63). Let me use a truly invalid opcode.
  // Reserved opcode 0x02:
  mem.write_word(0, encode_R(0x02, 0, 0, 0, 0)); // reserved opcode
  step(state, mem);
  EXPECT_TRUE(state.trap_pending);
  EXPECT_EQ(state.scause, 9ULL); // illegal instruction
}

TEST_F(CpuTest, unknown_func_triggers_trap) {
  state.pc = 0;
  state.stvec = 0x2000;
  // Opcode 0x10 with func=3 (undefined — only 0,1,2,5,6,7 exist)
  mem.write_word(0, encode_R(0x10, 1, 2, 3, 3));
  step(state, mem);
  EXPECT_TRUE(state.trap_pending);
  EXPECT_EQ(state.scause, 9ULL);
}

TEST_F(CpuTest, PC_alignment_for_JALR) {
  set_reg(1, 0x1001); // odd address
  state.pc = 0;
  exec32(encode_I(0x62, 29, 1, 0));
  EXPECT_EQ(state.pc, 0x1000ULL); // aligned to 4
}

TEST_F(CpuTest, timer_increment_on_step) {
  state.pc = 0;
  exec32(encode_R(0x00, 0, 0, 0, 0));
  EXPECT_EQ(state.stime, 1ULL); // timer ticked
}

TEST_F(CpuTest, timer_interrupt) {
  state.stime = 9;
  state.stimecmp = 10;
  state.sstatus = 1; // SIE = 1
  state.stvec = 0x8000;
  state.pc = 0;
  mem.write_word(0, encode_R(0x00, 0, 0, 0, 0)); // NOP advances stime to 10
  step(state, mem);
  EXPECT_TRUE(state.trap_pending);
  EXPECT_EQ(state.scause, 0x8000000000000000ULL);
  EXPECT_EQ(state.pc, 0x8000ULL);
}
