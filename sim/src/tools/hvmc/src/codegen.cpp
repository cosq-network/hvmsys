#include "hvmc/codegen.h"
#include <cstring>
#include <stdexcept>
#include <sstream>

namespace hvmc {

CodeGen::CodeGen() : label_counter(0) {}

std::string CodeGen::new_label() {
    return ".L" + std::to_string(label_counter++);
}

std::string CodeGen::gen(ASTNode* node) {
    switch (node->kind) {
    case ASTKind::PROGRAM:
        for (int i = 0; i < (int)node->u.func.params.size(); i++)
            gen(node->u.func.params[i]);
        break;
    case ASTKind::FUNCTION_DEF:
        gen_function_def(node);
        break;
    case ASTKind::VARIABLE_DECL:
        gen_variable_decl(node);
        break;
    case ASTKind::BLOCK:
        for (int i = 0; i < (int)node->u.block.stmts.size(); i++)
            gen(node->u.block.stmts[i]);
        break;
    case ASTKind::EXPR_STMT:
        gen(node->u.expr_stmt.expr);
        break;
    case ASTKind::IF_STMT:
        gen_if_stmt(node);
        break;
    case ASTKind::WHILE_STMT:
        gen_while_stmt(node);
        break;
    case ASTKind::FOR_STMT:
        gen_for_stmt(node);
        break;
    case ASTKind::DO_WHILE_STMT:
        gen_do_while_stmt(node);
        break;
    case ASTKind::RETURN_STMT:
        if (node->u.ret.expr) {
            gen_expression(node->u.ret.expr, Reg::R1);
        }
        emit_line("ret");
        break;
    case ASTKind::BREAK_STMT:
        break;
    case ASTKind::CONTINUE_STMT:
        break;
    case ASTKind::ASM_STMT:
        emit_line(node->u.asm_stmt.code);
        break;
    case ASTKind::EMPTY_STMT:
        break;
    default:
        break;
    }
    return output.str();
}

void CodeGen::gen_function_def(ASTNode* node) {
    // function declarations are just forward declarations for now
    if (!node->u.func.body) return;

    emit_line(".section .text");
    emit_line(".global " + std::string(node->u.func.name));
    emit_line(node->u.func.name);
    emit_line("enter $0, $0");  // placeholder

    gen(node->u.func.body);

    emit_line("ret");
}

void CodeGen::gen_variable_decl(ASTNode* node) {
    std::string name(node->u.var_decl.name);
    if (node->u.var_decl.init) {
        gen_expression(node->u.var_decl.init, Reg::R1);
        // Store to stack or global
        emit_line("mov r1, " + name);
    }
}

void CodeGen::gen_if_stmt(ASTNode* node) {
    std::string else_lbl = new_label();
    std::string end_lbl = new_label();

    gen_condition(node->u.if_stmt.cond, else_lbl);
    gen(node->u.if_stmt.then_body);
    if (node->u.if_stmt.else_body) {
        emit_line("jmp " + end_lbl);
        emit_line(else_lbl);
        gen(node->u.if_stmt.else_body);
        emit_line(end_lbl);
    } else {
        emit_line(else_lbl);
    }
}

void CodeGen::gen_while_stmt(ASTNode* node) {
    std::string start = new_label();
    std::string end = new_label();

    emit_line(start);
    gen_condition(node->u.while_stmt.cond, end);
    gen(node->u.while_stmt.body);
    emit_line("jmp " + start);
    emit_line(end);
}

void CodeGen::gen_for_stmt(ASTNode* node) {
    std::string start = new_label();
    std::string end = new_label();

    if (node->u.for_stmt.init) gen_expression(node->u.for_stmt.init, Reg::R1);
    emit_line(start);
    if (node->u.for_stmt.cond) gen_condition(node->u.for_stmt.cond, end);
    gen(node->u.for_stmt.body);
    if (node->u.for_stmt.inc) gen_expression(node->u.for_stmt.inc, Reg::R1);
    emit_line("jmp " + start);
    emit_line(end);
}

void CodeGen::gen_do_while_stmt(ASTNode* node) {
    std::string start = new_label();
    std::string end = new_label();

    emit_line(start);
    gen(node->u.do_while.body);
    gen_condition(node->u.do_while.cond, end, true);
    emit_line("jmp " + start);
    emit_line(end);
}

void CodeGen::gen_condition(ASTNode* node, const std::string& label, bool invert) {
    if (!node) return;
    if (node->kind == ASTKind::BINARY_OP) {
        TokenKind op = node->u.binop.op;
        gen_expression(node->u.binop.left, Reg::R1);
        gen_expression(node->u.binop.right, Reg::R2);
        std::string mnemonic;
        switch (op) {
        case TokenKind::EQEQ: mnemonic = invert ? "bne" : "beq"; break;
        case TokenKind::NEQ:  mnemonic = invert ? "beq" : "bne"; break;
        case TokenKind::LT:   mnemonic = invert ? "bge" : "blt"; break;
        case TokenKind::LE:   mnemonic = invert ? "bgt" : "ble"; break;
        case TokenKind::GT:   mnemonic = invert ? "ble" : "bgt"; break;
        case TokenKind::GE:   mnemonic = invert ? "blt" : "bge"; break;
        default: break;
        }
        if (!mnemonic.empty())
            emit_line(mnemonic + " r1, r2, " + label);
        return;
    }
    // Fall through: compare against zero
    gen_expression(node, Reg::R1);
    emit_line("beq r1, r0, " + label);
}

void CodeGen::gen_expression(ASTNode* node, Reg target) {
    if (!node) return;

    switch (node->kind) {
    case ASTKind::CONSTANT:
        emit("mov " + reg_name(target) + ", " + std::to_string(node->u.constant.value));
        break;
    case ASTKind::VARIABLE: {
        std::string name(node->u.var.name);
        emit("mov " + reg_name(target) + ", " + name);
        break;
    }
    case ASTKind::BINARY_OP: {
        TokenKind op = node->u.binop.op;
        gen_expression(node->u.binop.left, Reg::R1);
        gen_expression(node->u.binop.right, Reg::R2);
        switch (op) {
        case TokenKind::PLUS:   emit_line("add r1, r2, r1"); break;
        case TokenKind::MINUS:  emit_line("sub r1, r2, r1"); break;
        case TokenKind::STAR:   emit_line("mul r1, r2, r1"); break;
        case TokenKind::SLASH:  emit_line("div r1, r2, r1"); break;
        case TokenKind::PERCENT: emit_line("rem r1, r2, r1"); break;
        case TokenKind::AMPER:  emit_line("and r1, r2, r1"); break;
        case TokenKind::PIPE:   emit_line("or r1, r2, r1"); break;
        case TokenKind::CARET:  emit_line("xor r1, r2, r1"); break;
        case TokenKind::LSHIFT: emit_line("sll r1, r1, r2"); break;
        case TokenKind::RSHIFT: emit_line("srl r1, r1, r2"); break;
        default: break;
        }
        if (target != Reg::R1)
            emit_line("mov " + reg_name(target) + ", r1");
        break;
    }
    case ASTKind::UNARY_OP: {
        gen_expression(node->u.unop.operand, target);
        break;
    }
    case ASTKind::ASSIGN: {
        gen_expression(node->u.assign.value, Reg::R1);
        emit_line("mov r1, " + reg_name(target));
        break;
    }
    case ASTKind::ADDRESS_OF: {
        if (node->u.addr_of.operand->kind == ASTKind::VARIABLE) {
            std::string var_name(node->u.addr_of.operand->u.var.name);
            emit_line("mov " + reg_name(target) + ", $" + var_name);
        }
        break;
    }
    case ASTKind::STRING_LIT: {
        std::string lbl = new_label();
        emit_line(lbl + ": .string " + std::string(node->u.string_lit.value));
        emit_line("mov " + reg_name(target) + ", $" + lbl);
        break;
    }
    case ASTKind::CALL: {
        std::string name(node->u.call.name);
        for (int i = 0; i < (int)node->u.call.args.size(); i++) {
            Reg r = static_cast<Reg>(static_cast<int>(Reg::R1) + i);
            gen_expression(node->u.call.args[i], r);
        }
        emit_line("call " + name);
        if (target != Reg::R1)
            emit_line("mov " + reg_name(target) + ", r1");
        break;
    }
    case ASTKind::DEREF: {
        Reg tmp = (target == Reg::R1) ? Reg::R2 : Reg::R1;
        gen_expression(node->u.deref.operand, target);
        emit_line("ld " + reg_name(target) + ", [" + reg_name(target) + "]");
        break;
    }
    case ASTKind::ARRAY_INDEX: {
        gen_expression(node->u.arr_idx.arr, Reg::R1);
        gen_expression(node->u.arr_idx.index, Reg::R2);
        emit_line("sll r2, r2, 3"); // assume 8-byte elements
        emit_line("add r1, r1, r2");
        emit_line("ld " + reg_name(target) + ", [r1]");
        break;
    }
    default:
        break;
    }
}

std::string CodeGen::reg_name(Reg r) {
    switch (r) {
    case Reg::R0: return "r0";
    case Reg::R1: return "r1";
    case Reg::R2: return "r2";
    case Reg::R3: return "r3";
    case Reg::R4: return "r4";
    case Reg::R5: return "r5";
    case Reg::R6: return "r6";
    case Reg::R7: return "r7";
    case Reg::R8: return "r8";
    case Reg::R29: return "r29";
    case Reg::R30: return "r30";
    case Reg::R31: return "r31";
    default: return "r" + std::to_string(static_cast<int>(r));
    }
}

void CodeGen::emit(const std::string& line) {
    output << line << "\n";
}

void CodeGen::emit_line(const std::string& line) {
    output << "  " << line << "\n";
}

} // namespace hvmc
