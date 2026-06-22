#include "hvm-sim/core/executor.hpp"
#include <cstring>
#include <cmath>
#include <algorithm>

namespace hvm {

namespace {

uint64_t sext(int64_t val, int bits) {
  int64_t m = 1LL << (bits - 1);
  return static_cast<uint64_t>((val ^ m) - m);
}

double bits_to_double(uint64_t v) {
  double d;
  std::memcpy(&d, &v, sizeof(d));
  return d;
}

uint64_t double_to_bits(double d) {
  uint64_t v;
  std::memcpy(&v, &d, sizeof(v));
  return v;
}

int vec_elems(uint64_t vtype) {
  (void)vtype;
  return 4;
}

void tick_timer(HvmCpuState& state) {
  state.stime++;
  if (state.stime >= state.stimecmp && (state.sstatus & 1)) {
    state.signal_trap(0x8000000000000000ULL, 0);
  }
}

} // anonymous namespace

void execute_inst(HvmCpuState& state, MemoryAccess& mem, const DecodedInst& di) {
  switch (di.mnemonic) {

    case HvmMnemonic::kNOP:
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kMOV:
      state.write_reg(di.rd, state.read_reg(di.rs1));
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kMOVZ:
      state.write_reg(di.rd, state.read_reg(di.rs1) | static_cast<uint64_t>(di.imm & 0x7FFF));
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kLUI: {
      uint64_t imm_shifted = static_cast<uint64_t>(di.imm & 0x7FFF) << 49;
      state.write_reg(di.rd, state.read_reg(di.rs1) | imm_shifted);
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kADDI:
      state.write_reg(di.rd, state.read_reg(di.rs1) + static_cast<uint64_t>(di.imm));
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kRETAIN:
      state.write_reg(di.rd, state.read_reg(di.rs1));
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kRELEASE:
      state.write_reg(di.rd, 0);
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kICACHE_RNG:
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kADD:
      state.write_reg(di.rd, state.read_reg(di.rs1) + state.read_reg(di.rs2));
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kSUB:
      state.write_reg(di.rd, state.read_reg(di.rs1) - state.read_reg(di.rs2));
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kMUL:
      state.write_reg(di.rd, state.read_reg(di.rs1) * state.read_reg(di.rs2));
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kDIV: {
      int64_t a = static_cast<int64_t>(state.read_reg(di.rs1));
      int64_t b = static_cast<int64_t>(state.read_reg(di.rs2));
      int64_t result = (b == 0) ? -1 : (a / b);
      state.write_reg(di.rd, static_cast<uint64_t>(result));
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kDIVU: {
      uint64_t a = state.read_reg(di.rs1);
      uint64_t b = state.read_reg(di.rs2);
      uint64_t result = (b == 0) ? static_cast<uint64_t>(-1) : (a / b);
      state.write_reg(di.rd, result);
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kREM: {
      int64_t a = static_cast<int64_t>(state.read_reg(di.rs1));
      int64_t b = static_cast<int64_t>(state.read_reg(di.rs2));
      int64_t result = (b == 0) ? a : (a % b);
      state.write_reg(di.rd, static_cast<uint64_t>(result));
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kSHL:
      state.write_reg(di.rd, state.read_reg(di.rs1) << (state.read_reg(di.rs2) & 63));
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kSHR:
      state.write_reg(di.rd, state.read_reg(di.rs1) >> (state.read_reg(di.rs2) & 63));
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kSAR: {
      int64_t a = static_cast<int64_t>(state.read_reg(di.rs1));
      int sh = static_cast<int>(state.read_reg(di.rs2) & 63);
      state.write_reg(di.rd, static_cast<uint64_t>(a >> sh));
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kAND:
      state.write_reg(di.rd, state.read_reg(di.rs1) & state.read_reg(di.rs2));
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kOR:
      state.write_reg(di.rd, state.read_reg(di.rs1) | state.read_reg(di.rs2));
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kXOR:
      state.write_reg(di.rd, state.read_reg(di.rs1) ^ state.read_reg(di.rs2));
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kNOT:
      state.write_reg(di.rd, ~state.read_reg(di.rs1));
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kCMPEQ:
      state.write_reg(di.rd, (state.read_reg(di.rs1) == state.read_reg(di.rs2)) ? 1 : 0);
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kCMPNE:
      state.write_reg(di.rd, (state.read_reg(di.rs1) != state.read_reg(di.rs2)) ? 1 : 0);
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kCMPLT:
      state.write_reg(di.rd,
        (static_cast<int64_t>(state.read_reg(di.rs1)) < static_cast<int64_t>(state.read_reg(di.rs2))) ? 1 : 0);
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kCMPLE:
      state.write_reg(di.rd,
        (static_cast<int64_t>(state.read_reg(di.rs1)) <= static_cast<int64_t>(state.read_reg(di.rs2))) ? 1 : 0);
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kFCMPEQ: {
      double a = bits_to_double(state.read_reg(di.rs1));
      double b = bits_to_double(state.read_reg(di.rs2));
      state.write_reg(di.rd, (a == b) ? 1 : 0);
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kFCMPLT: {
      double a = bits_to_double(state.read_reg(di.rs1));
      double b = bits_to_double(state.read_reg(di.rs2));
      state.write_reg(di.rd, (a < b) ? 1 : 0);
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kFCMPLE: {
      double a = bits_to_double(state.read_reg(di.rs1));
      double b = bits_to_double(state.read_reg(di.rs2));
      state.write_reg(di.rd, (a <= b) ? 1 : 0);
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kFADD: {
      double a = bits_to_double(state.read_reg(di.rs1));
      double b = bits_to_double(state.read_reg(di.rs2));
      state.write_reg(di.rd, double_to_bits(a + b));
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kFSUB: {
      double a = bits_to_double(state.read_reg(di.rs1));
      double b = bits_to_double(state.read_reg(di.rs2));
      state.write_reg(di.rd, double_to_bits(a - b));
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kFMUL: {
      double a = bits_to_double(state.read_reg(di.rs1));
      double b = bits_to_double(state.read_reg(di.rs2));
      state.write_reg(di.rd, double_to_bits(a * b));
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kFDIV: {
      double a = bits_to_double(state.read_reg(di.rs1));
      double b = bits_to_double(state.read_reg(di.rs2));
      state.write_reg(di.rd, double_to_bits(a / b));
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kBEQ: {
      int64_t target = static_cast<int64_t>(state.pc) + di.imm * 4;
      state.pc = (state.read_reg(di.rs1) == state.read_reg(di.rs2))
                    ? static_cast<uint64_t>(target) : state.pc + 4;
      break;
    }

    case HvmMnemonic::kBNE: {
      int64_t target = static_cast<int64_t>(state.pc) + di.imm * 4;
      state.pc = (state.read_reg(di.rs1) != state.read_reg(di.rs2))
                    ? static_cast<uint64_t>(target) : state.pc + 4;
      break;
    }

    case HvmMnemonic::kBLT: {
      int64_t target = static_cast<int64_t>(state.pc) + di.imm * 4;
      state.pc = (static_cast<int64_t>(state.read_reg(di.rs1)) < static_cast<int64_t>(state.read_reg(di.rs2)))
                    ? static_cast<uint64_t>(target) : state.pc + 4;
      break;
    }

    case HvmMnemonic::kBLE: {
      int64_t target = static_cast<int64_t>(state.pc) + di.imm * 4;
      state.pc = (static_cast<int64_t>(state.read_reg(di.rs1)) <= static_cast<int64_t>(state.read_reg(di.rs2)))
                    ? static_cast<uint64_t>(target) : state.pc + 4;
      break;
    }

    case HvmMnemonic::kJMP: {
      int64_t target = static_cast<int64_t>(state.pc) + di.imm * 4;
      state.pc = static_cast<uint64_t>(target);
      break;
    }

    case HvmMnemonic::kJAL: {
      uint64_t ra = state.pc + 4;
      int64_t target = static_cast<int64_t>(state.pc) + di.imm * 4;
      state.write_reg(di.rd, ra);
      state.pc = static_cast<uint64_t>(target);
      break;
    }

    case HvmMnemonic::kJALR: {
      uint64_t ra = state.pc + 4;
      state.write_reg(di.rd, ra);
      state.pc = (state.read_reg(di.rs1) + static_cast<uint64_t>(di.imm)) & ~0x3ULL;
      break;
    }

    case HvmMnemonic::kRET:
      state.pc = state.read_reg(29) & ~0x3ULL;
      break;

    case HvmMnemonic::kLD_B: {
      uint64_t addr = state.read_reg(di.rs1) + static_cast<uint64_t>(di.imm);
      int8_t v = static_cast<int8_t>(mem.read_byte(addr));
      state.write_reg(di.rd, static_cast<uint64_t>(static_cast<int64_t>(v)));
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kLD_BU: {
      uint64_t addr = state.read_reg(di.rs1) + static_cast<uint64_t>(di.imm);
      state.write_reg(di.rd, mem.read_byte(addr));
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kLD_H: {
      uint64_t addr = state.read_reg(di.rs1) + static_cast<uint64_t>(di.imm);
      int16_t v = static_cast<int16_t>(mem.read_half(addr));
      state.write_reg(di.rd, static_cast<uint64_t>(static_cast<int64_t>(v)));
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kLD_HU: {
      uint64_t addr = state.read_reg(di.rs1) + static_cast<uint64_t>(di.imm);
      state.write_reg(di.rd, mem.read_half(addr));
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kLD_W: {
      uint64_t addr = state.read_reg(di.rs1) + static_cast<uint64_t>(di.imm);
      int32_t v = static_cast<int32_t>(mem.read_word(addr));
      state.write_reg(di.rd, static_cast<uint64_t>(static_cast<int64_t>(v)));
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kLD_WU: {
      uint64_t addr = state.read_reg(di.rs1) + static_cast<uint64_t>(di.imm);
      state.write_reg(di.rd, mem.read_word(addr));
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kLD_D: {
      uint64_t addr = state.read_reg(di.rs1) + static_cast<uint64_t>(di.imm);
      state.write_reg(di.rd, mem.read_dword(addr));
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kLD_P: {
      uint64_t addr = state.read_reg(di.rs2);
      state.write_reg(di.rd, mem.read_dword(addr));
      state.write_reg(di.rs1, mem.read_dword(addr + 8));
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kST_B: {
      uint64_t addr = state.read_reg(di.rs1) + static_cast<uint64_t>(di.imm);
      mem.write_byte(addr, static_cast<uint8_t>(state.read_reg(di.rd) & 0xFF));
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kST_H: {
      uint64_t addr = state.read_reg(di.rs1) + static_cast<uint64_t>(di.imm);
      mem.write_half(addr, static_cast<uint16_t>(state.read_reg(di.rd) & 0xFFFF));
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kST_W: {
      uint64_t addr = state.read_reg(di.rs1) + static_cast<uint64_t>(di.imm);
      mem.write_word(addr, static_cast<uint32_t>(state.read_reg(di.rd) & 0xFFFFFFFF));
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kST_D: {
      uint64_t addr = state.read_reg(di.rs1) + static_cast<uint64_t>(di.imm);
      mem.write_dword(addr, state.read_reg(di.rd));
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kST_P: {
      uint64_t addr = state.read_reg(di.rs2);
      mem.write_dword(addr, state.read_reg(di.rd));
      mem.write_dword(addr + 8, state.read_reg(di.rs1));
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kLDA:
      state.write_reg(di.rd, state.read_reg(di.rs1) + static_cast<uint64_t>(di.imm));
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kPUSH: {
      state.write_reg(31, state.read_reg(31) - 8);
      mem.write_dword(state.read_reg(31), state.read_reg(di.rd));
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kPOP: {
      state.write_reg(di.rd, mem.read_dword(state.read_reg(31)));
      state.write_reg(31, state.read_reg(31) + 8);
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kENTER: {
      uint64_t sp = state.read_reg(31);
      sp -= 16;
      mem.write_dword(sp, state.read_reg(29));
      mem.write_dword(sp + 8, state.read_reg(30));
      state.write_reg(30, sp);
      sp -= static_cast<uint64_t>(di.imm);
      state.write_reg(31, sp);
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kLEAVE: {
      uint64_t sp = state.read_reg(30);
      state.write_reg(29, mem.read_dword(sp));
      state.write_reg(30, mem.read_dword(sp + 8));
      state.write_reg(31, sp + 16);
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kADJSP:
      state.write_reg(31, state.read_reg(31) + static_cast<uint64_t>(di.imm));
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kFRAME:
      state.write_reg(di.rd, state.read_reg(30) + static_cast<uint64_t>(di.imm));
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kCALL: {
      uint64_t ra = state.pc + (di.is_escape ? 8 : 4);
      int64_t target = static_cast<int64_t>(state.pc) + di.imm * 4;
      state.write_reg(di.rd, ra);
      state.pc = static_cast<uint64_t>(target);
      break;
    }

    case HvmMnemonic::kTAILCALL: {
      int64_t target = static_cast<int64_t>(state.pc) + di.imm * 4;
      state.pc = static_cast<uint64_t>(target);
      break;
    }

    case HvmMnemonic::kSYSCALL: {
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kBREAK:
      state.signal_trap(10, state.pc);
      break;

    case HvmMnemonic::kECALL:
      state.signal_trap(8, state.pc);
      break;

    case HvmMnemonic::kTRAPRET: {
      uint64_t sie = (state.sstatus >> 1) & 1;
      uint64_t spp = (state.sstatus >> 8) & 1;
      state.sstatus = (state.sstatus & ~1ULL) | sie;
      state.privilege = (spp == 1) ? PrivilegeLevel::kSupervisor : PrivilegeLevel::kUser;
      state.pc = state.sepc;
      break;
    }

    case HvmMnemonic::kLR_D: {
      uint64_t addr = state.read_reg(di.rs1);
      state.write_reg(di.rd, mem.read_dword(addr));
      state.reservation_addr = addr;
      state.reservation_valid = true;
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kSC_D: {
      uint64_t addr = state.read_reg(di.rs1);
      if (state.reservation_valid && state.reservation_addr == addr) {
        mem.write_dword(addr, state.read_reg(di.rs2));
        state.write_reg(di.rd, 0);
        state.reservation_valid = false;
      } else {
        state.write_reg(di.rd, 1);
      }
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kCSRRW: {
      uint16_t csr_addr = static_cast<uint16_t>(di.imm & 0xFFF);
      uint64_t old = state.read_csr(csr_addr);
      if (di.rs1 != 0) {
        state.write_csr(csr_addr, state.read_reg(di.rs1));
      }
      state.write_reg(di.rd, old);
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kSFENCE_VMA:
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kLOOP_SET:
      state.loop_count = state.read_reg(di.rs1);
      state.loop_backedge = static_cast<uint64_t>(static_cast<int64_t>(di.imm));
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kLOOP_DECBR: {
      if (state.loop_count != 0) {
        state.loop_count--;
        if (state.loop_count != 0) {
          int64_t target = static_cast<int64_t>(state.pc) + di.imm * 4;
          state.pc = static_cast<uint64_t>(target);
          break;
        }
      }
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kPREFETCH_R:
    case HvmMnemonic::kPREFETCH_W:
    case HvmMnemonic::kPREFETCH_NTA:
    case HvmMnemonic::kMEMZERO_HINT:
    case HvmMnemonic::kBR_HINT:
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kALLOC_BUMP:
      state.write_reg(di.rd, 0);
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kRDPROF: {
      state.write_reg(di.rd, 0);
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kCHK_B:
      state.write_reg(di.rd, state.read_reg(di.rs1));
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kLD_D_NZ: {
      uint64_t addr = state.read_reg(di.rs1);
      if (addr == 0) {
        state.signal_trap(9, state.pc);
      } else {
        uint64_t ea = addr + static_cast<uint64_t>(di.imm);
        state.write_reg(di.rd, mem.read_dword(ea));
        state.pc += di.is_escape ? 8 : 4;
      }
      break;
    }

    case HvmMnemonic::kDOORBELL:
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kVSETVL: {
      uint64_t avl = state.read_reg(di.rs1);
      uint64_t new_vtype = state.read_reg(di.rs2);
      uint64_t maxvl = 4;
      uint64_t new_vl = (avl < maxvl) ? avl : maxvl;
      state.vl = new_vl;
      state.vtype = new_vtype;
      state.vstate = 2;
      state.write_reg(di.rd, new_vl);
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kVLD_V: {
      uint64_t base = state.read_reg(di.rs2);
      int n = vec_elems(state.vtype);
      int active = static_cast<int>(state.vl > static_cast<uint64_t>(n) ? static_cast<uint64_t>(n) : state.vl);
      for (int i = 0; i < active; i++) {
        state.vec_regs[di.rd][i] = mem.read_dword(base + static_cast<uint64_t>(i) * 8);
      }
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kVST_V: {
      uint64_t base = state.read_reg(di.rs2);
      int n = vec_elems(state.vtype);
      int active = static_cast<int>(state.vl > static_cast<uint64_t>(n) ? static_cast<uint64_t>(n) : state.vl);
      for (int i = 0; i < active; i++) {
        mem.write_dword(base + static_cast<uint64_t>(i) * 8, state.vec_regs[di.rd][i]);
      }
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kVLDS_V: {
      uint64_t base = state.read_reg(di.rs2);
      uint64_t stride = state.read_reg(di.rs1);
      int n = vec_elems(state.vtype);
      int active = static_cast<int>(state.vl > static_cast<uint64_t>(n) ? static_cast<uint64_t>(n) : state.vl);
      for (int i = 0; i < active; i++) {
        state.vec_regs[di.rd][i] = mem.read_dword(base + stride * static_cast<uint64_t>(i));
      }
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kVSTS_V: {
      uint64_t base = state.read_reg(di.rs2);
      uint64_t stride = state.read_reg(di.rs1);
      int n = vec_elems(state.vtype);
      int active = static_cast<int>(state.vl > static_cast<uint64_t>(n) ? static_cast<uint64_t>(n) : state.vl);
      for (int i = 0; i < active; i++) {
        mem.write_dword(base + stride * static_cast<uint64_t>(i), state.vec_regs[di.rd][i]);
      }
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kVLDX_V: {
      uint64_t base = state.read_reg(di.rs2);
      int n = vec_elems(state.vtype);
      int active = static_cast<int>(state.vl > static_cast<uint64_t>(n) ? static_cast<uint64_t>(n) : state.vl);
      for (int i = 0; i < active; i++) {
        uint64_t offset = state.vec_regs[di.rs1][i];
        state.vec_regs[di.rd][i] = mem.read_dword(base + offset);
      }
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kVSTX_V: {
      uint64_t base = state.read_reg(di.rs2);
      int n = vec_elems(state.vtype);
      int active = static_cast<int>(state.vl > static_cast<uint64_t>(n) ? static_cast<uint64_t>(n) : state.vl);
      for (int i = 0; i < active; i++) {
        uint64_t offset = state.vec_regs[di.rs1][i];
        mem.write_dword(base + offset, state.vec_regs[di.rd][i]);
      }
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kVADD_VV:
      for (int i = 0; i < kVecLanes; i++)
        state.vec_regs[di.rd][i] = state.vec_regs[di.rs1][i] + state.vec_regs[di.rs2][i];
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kVADD_VX:
      for (int i = 0; i < kVecLanes; i++)
        state.vec_regs[di.rd][i] = state.vec_regs[di.rs1][i] + state.read_reg(di.rs2);
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kVSUB_VV:
      for (int i = 0; i < kVecLanes; i++)
        state.vec_regs[di.rd][i] = state.vec_regs[di.rs1][i] - state.vec_regs[di.rs2][i];
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kVSUB_VX:
      for (int i = 0; i < kVecLanes; i++)
        state.vec_regs[di.rd][i] = state.vec_regs[di.rs1][i] - state.read_reg(di.rs2);
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kVMUL_VV:
      for (int i = 0; i < kVecLanes; i++)
        state.vec_regs[di.rd][i] = state.vec_regs[di.rs1][i] * state.vec_regs[di.rs2][i];
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kVMUL_VX:
      for (int i = 0; i < kVecLanes; i++)
        state.vec_regs[di.rd][i] = state.vec_regs[di.rs1][i] * state.read_reg(di.rs2);
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kVDIV_VV:
      for (int i = 0; i < kVecLanes; i++) {
        uint64_t b = state.vec_regs[di.rs2][i];
        state.vec_regs[di.rd][i] = (b == 0) ? static_cast<uint64_t>(-1) : (state.vec_regs[di.rs1][i] / b);
      }
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kVDIV_VX: {
      uint64_t b = state.read_reg(di.rs2);
      uint64_t div = (b == 0) ? static_cast<uint64_t>(-1) : b;
      for (int i = 0; i < kVecLanes; i++)
        state.vec_regs[di.rd][i] = (b == 0) ? static_cast<uint64_t>(-1) : (state.vec_regs[di.rs1][i] / b);
      (void)div;
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kVFMACC_VV:
      for (int i = 0; i < kVecLanes; i++) {
        double a = bits_to_double(state.vec_regs[di.rd][i]);
        double b = bits_to_double(state.vec_regs[di.rs1][i]);
        double c = bits_to_double(state.vec_regs[di.rs2][i]);
        state.vec_regs[di.rd][i] = double_to_bits(a + b * c);
      }
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kVFMACC_VF:
      for (int i = 0; i < kVecLanes; i++) {
        double a = bits_to_double(state.vec_regs[di.rd][i]);
        double b = bits_to_double(state.read_reg(di.rs1));
        double c = bits_to_double(state.vec_regs[di.rs2][i]);
        state.vec_regs[di.rd][i] = double_to_bits(a + b * c);
      }
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kVCOMP_VV:
      for (int i = 0; i < kVecLanes; i++) {
        uint64_t a = state.vec_regs[di.rs1][i];
        uint64_t b = state.vec_regs[di.rs2][i];
        state.vec_regs[di.rd][i] = (a == b) ? static_cast<uint64_t>(-1) : 0;
      }
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kVCOMP_VX:
      for (int i = 0; i < kVecLanes; i++) {
        uint64_t a = state.vec_regs[di.rs1][i];
        uint64_t b = state.read_reg(di.rs2);
        state.vec_regs[di.rd][i] = (a == b) ? static_cast<uint64_t>(-1) : 0;
      }
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kVMERGE_VVM:
      for (int i = 0; i < kVecLanes; i++) {
        bool mask = (state.vec_regs[0][i] >> 0) & 1;
        state.vec_regs[di.rd][i] = mask ? state.vec_regs[di.rs1][i] : state.vec_regs[di.rs2][i];
      }
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kVFIRST_M: {
      int64_t first = -1;
      for (int i = 0; i < kVecLanes; i++) {
        if (state.vec_regs[di.rs1][i] & 1) {
          first = i;
          break;
        }
      }
      state.write_reg(di.rd, static_cast<uint64_t>(first));
      state.pc += di.is_escape ? 8 : 4;
      break;
    }

    case HvmMnemonic::kVREDADD_VS:
      state.vec_regs[di.rd][0] = state.vec_regs[di.rs2][0];
      for (int i = 0; i < kVecLanes; i++)
        state.vec_regs[di.rd][0] += state.vec_regs[di.rs1][i];
      for (int i = 1; i < kVecLanes; i++)
        state.vec_regs[di.rd][i] = 0;
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kVREDMIN_VS:
      state.vec_regs[di.rd][0] = state.vec_regs[di.rs2][0];
      for (int i = 0; i < kVecLanes; i++)
        if (state.vec_regs[di.rs1][i] < state.vec_regs[di.rd][0])
          state.vec_regs[di.rd][0] = state.vec_regs[di.rs1][i];
      for (int i = 1; i < kVecLanes; i++)
        state.vec_regs[di.rd][i] = 0;
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kVREDMAX_VS:
      state.vec_regs[di.rd][0] = state.vec_regs[di.rs2][0];
      for (int i = 0; i < kVecLanes; i++)
        if (state.vec_regs[di.rs1][i] > state.vec_regs[di.rd][0])
          state.vec_regs[di.rd][0] = state.vec_regs[di.rs1][i];
      for (int i = 1; i < kVecLanes; i++)
        state.vec_regs[di.rd][i] = 0;
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kVSLL_VV:
      for (int i = 0; i < kVecLanes; i++)
        state.vec_regs[di.rd][i] = state.vec_regs[di.rs1][i] << (state.vec_regs[di.rs2][i] & 63);
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kVSLL_VX:
      for (int i = 0; i < kVecLanes; i++)
        state.vec_regs[di.rd][i] = state.vec_regs[di.rs1][i] << (state.read_reg(di.rs2) & 63);
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kVSRL_VV:
      for (int i = 0; i < kVecLanes; i++)
        state.vec_regs[di.rd][i] = state.vec_regs[di.rs1][i] >> (state.vec_regs[di.rs2][i] & 63);
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kVSRL_VX:
      for (int i = 0; i < kVecLanes; i++)
        state.vec_regs[di.rd][i] = state.vec_regs[di.rs1][i] >> (state.read_reg(di.rs2) & 63);
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kVAND_VV:
      for (int i = 0; i < kVecLanes; i++)
        state.vec_regs[di.rd][i] = state.vec_regs[di.rs1][i] & state.vec_regs[di.rs2][i];
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kVOR_VV:
      for (int i = 0; i < kVecLanes; i++)
        state.vec_regs[di.rd][i] = state.vec_regs[di.rs1][i] | state.vec_regs[di.rs2][i];
      state.pc += di.is_escape ? 8 : 4;
      break;

    case HvmMnemonic::kVXOR_VV:
      for (int i = 0; i < kVecLanes; i++)
        state.vec_regs[di.rd][i] = state.vec_regs[di.rs1][i] ^ state.vec_regs[di.rs2][i];
      state.pc += di.is_escape ? 8 : 4;
      break;
  }
}

bool step(HvmCpuState& state, MemoryAccess& mem) {
  DecodedInst di = decode_inst(state, mem);
  if (!di.valid) {
    state.signal_trap(9, state.pc);
    return true;
  }
  execute_inst(state, mem, di);
  tick_timer(state);
  return state.trap_pending;
}

} // namespace hvm
