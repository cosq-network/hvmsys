#include "hvmc/parser.h"
#include <cstring>
#include <stdexcept>

namespace hvmc {

Parser::Parser(const std::string& source) : lex(source) {}

Token Parser::peek() { return lex.peek(); }
Token Parser::next() { return lex.next(); }

Token Parser::expect(TokenKind kind, const std::string& msg) {
    Token t = next();
    if (t.kind != kind)
        throw std::runtime_error(msg + " at line " + std::to_string(t.line));
    return t;
}

bool Parser::match(TokenKind kind) {
    if (peek().kind == kind) { next(); return true; }
    return false;
}

ASTNode* Parser::alloc(ASTKind kind) {
    return alloc_node(kind);
}

static char* str_dup(const std::string& s) {
    char* c = new char[s.size() + 1];
    std::strcpy(c, s.c_str());
    return c;
}

// ── Type parsing ──────────────────────────────────────────────────────

CType Parser::parse_type_spec() {
    CType type{};
    bool is_unsigned = false;
    bool is_long = false;
    DataType base = DataType::NONE;

    while (true) {
        Token t = peek();
        if (t.kind == TokenKind::CONST) { next(); type.is_const = true; continue; }
        if (t.kind == TokenKind::VOLATILE) { next(); type.is_volatile = true; continue; }
        if (t.kind == TokenKind::UNSIGNED) { next(); is_unsigned = true; continue; }
        if (t.kind == TokenKind::SIGNED) { next(); continue; }
        if (t.kind == TokenKind::LONG_KW) { next(); is_long = true; continue; }
        if (t.kind == TokenKind::SHORT_KW) { next(); type.base = DataType::SHORT; break; }
        if (t.kind == TokenKind::VOID_KW) { next(); base = DataType::VOID; break; }
        if (t.kind == TokenKind::CHAR_KW) { next(); base = is_unsigned ? DataType::UCHAR : DataType::CHAR; break; }
        if (t.kind == TokenKind::INT_KW) { next(); break; }
        if (t.kind == TokenKind::STRUCT_KW) { next(); expect(TokenKind::IDENT, "expected struct name"); base = DataType::STRUCT; break; }
        break;
    }

    if (base == DataType::NONE) {
        if (is_long) base = DataType::LONG;
        else base = DataType::INT;
    }

    if (is_unsigned && base == DataType::CHAR) base = DataType::UCHAR;
    else if (is_unsigned && base == DataType::INT) base = DataType::UINT;
    else if (is_unsigned && base == DataType::LONG) base = DataType::ULONG;

    type.base = base;
    return type;
}

CType Parser::parse_declarator(CType base, const char*& name) {
    while (peek().kind == TokenKind::STAR) {
        next();
        base.pointer_depth++;
    }
    name = nullptr;
    if (peek().kind == TokenKind::IDENT) {
        Token t = next();
        name = str_dup(t.text);
    }
    return base;
}

// ── Declarations ─────────────────────────────────────────────────────

ASTNode* Parser::parse_declaration(bool) {
    bool is_extern = match(TokenKind::EXTERN);
    bool is_static = match(TokenKind::STATIC);

    CType type = parse_type_spec();
    const char* name = nullptr;

    if (peek().kind == TokenKind::IDENT) {
        Token name_tok = next();
        name = str_dup(name_tok.text);

        if (peek().kind == TokenKind::LPAREN) {
            return parse_function_def(type, name);
        }
        return parse_variable_decl(type, name, is_extern, is_static);
    }

    type = parse_declarator(type, name);
    if (name) return parse_variable_decl(type, name, is_extern, is_static);

    throw std::runtime_error("expected identifier after type");
}

ASTNode* Parser::parse_function_def(CType ret_type, const char* name) {
    ASTNode* node = alloc(ASTKind::FUNCTION_DEF);
    node->u.func.name = name;
    node->flags.func_name_alloced = 1;
    node->u.func.return_type = ret_type;

    expect(TokenKind::LPAREN, "expected ( after function name");

    if (peek().kind != TokenKind::RPAREN) {
        do {
            CType ptype = parse_type_spec();
            const char* pname = nullptr;
            ptype = parse_declarator(ptype, pname);
            if (!pname) pname = "";

            ASTNode* param = alloc(ASTKind::VARIABLE_DECL);
            param->u.var_decl.name = pname;
            param->u.var_decl.var_type = ptype;
            node->u.func.params.push(param);
        } while (match(TokenKind::COMMA));
    }
    expect(TokenKind::RPAREN, "expected ) after parameters");

    if (peek().kind == TokenKind::LBRACE) {
        node->u.func.body = parse_block();
    } else {
        expect(TokenKind::SEMICOLON, "expected ; after function declaration");
    }

    return node;
}

ASTNode* Parser::parse_variable_decl(CType type, const char* name, bool is_extern, bool is_static) {
    ASTNode* node = alloc(ASTKind::VARIABLE_DECL);
    node->u.var_decl.name = name;
    node->u.var_decl.var_type = type;
    node->u.var_decl.is_extern = is_extern;
    node->u.var_decl.is_static = is_static;
    node->u.var_decl.init = nullptr;

    if (match(TokenKind::LBRACKET)) {
        if (peek().kind == TokenKind::NUMBER) {
            Token n = next();
            type.array_size = static_cast<int>(n.intval);
        }
        expect(TokenKind::RBRACKET, "expected ]");
        node->u.var_decl.var_type = type;
    }

    if (match(TokenKind::EQ)) {
        node->u.var_decl.init = parse_assignment();
    }

    expect(TokenKind::SEMICOLON, "expected ; after variable declaration");
    return node;
}

// ── Statements ───────────────────────────────────────────────────────

ASTNode* Parser::parse_statement() {
    switch (peek().kind) {
    case TokenKind::LBRACE: return parse_block();
    case TokenKind::IF: return parse_if();
    case TokenKind::WHILE: return parse_while();
    case TokenKind::FOR: return parse_for();
    case TokenKind::DO: return parse_do_while();
    case TokenKind::RETURN: return parse_return();
    case TokenKind::BREAK: return parse_break();
    case TokenKind::CONTINUE: return parse_continue();
    case TokenKind::SEMICOLON: next(); return alloc(ASTKind::EMPTY_STMT);
    case TokenKind::INLINE_ASM: return parse_asm_stmt();
    default: {
        Token t = peek();
        if (t.kind == TokenKind::INT_KW || t.kind == TokenKind::CHAR_KW ||
            t.kind == TokenKind::SHORT_KW || t.kind == TokenKind::LONG_KW ||
            t.kind == TokenKind::VOID_KW || t.kind == TokenKind::UNSIGNED ||
            t.kind == TokenKind::SIGNED || t.kind == TokenKind::STATIC ||
            t.kind == TokenKind::EXTERN || t.kind == TokenKind::STRUCT_KW ||
            t.kind == TokenKind::CONST) {
            return parse_declaration(false);
        }

        ASTNode* expr = parse_expression();
        expect(TokenKind::SEMICOLON, "expected ; after expression");
        ASTNode* stmt = alloc(ASTKind::EXPR_STMT);
        stmt->u.expr_stmt.expr = expr;
        return stmt;
    }
    }
}

ASTNode* Parser::parse_block() {
    ASTNode* node = alloc(ASTKind::BLOCK);
    expect(TokenKind::LBRACE, "expected {");
    while (peek().kind != TokenKind::RBRACE && peek().kind != TokenKind::END) {
        node->u.block.stmts.push(parse_statement());
    }
    expect(TokenKind::RBRACE, "expected }");
    return node;
}

ASTNode* Parser::parse_if() {
    ASTNode* node = alloc(ASTKind::IF_STMT);
    expect(TokenKind::IF, "expected if");
    expect(TokenKind::LPAREN, "expected (");
    node->u.if_stmt.cond = parse_expression();
    expect(TokenKind::RPAREN, "expected )");
    node->u.if_stmt.then_body = parse_statement();
    if (match(TokenKind::ELSE)) {
        node->u.if_stmt.else_body = parse_statement();
    }
    return node;
}

ASTNode* Parser::parse_while() {
    ASTNode* node = alloc(ASTKind::WHILE_STMT);
    expect(TokenKind::WHILE, "expected while");
    expect(TokenKind::LPAREN, "expected (");
    node->u.while_stmt.cond = parse_expression();
    expect(TokenKind::RPAREN, "expected )");
    node->u.while_stmt.body = parse_statement();
    return node;
}

ASTNode* Parser::parse_for() {
    ASTNode* node = alloc(ASTKind::FOR_STMT);
    expect(TokenKind::FOR, "expected for");
    expect(TokenKind::LPAREN, "expected (");
    if (peek().kind != TokenKind::SEMICOLON) node->u.for_stmt.init = parse_expression();
    expect(TokenKind::SEMICOLON, "expected ;");
    if (peek().kind != TokenKind::SEMICOLON) node->u.for_stmt.cond = parse_expression();
    expect(TokenKind::SEMICOLON, "expected ;");
    if (peek().kind != TokenKind::RPAREN) node->u.for_stmt.inc = parse_expression();
    expect(TokenKind::RPAREN, "expected )");
    node->u.for_stmt.body = parse_statement();
    return node;
}

ASTNode* Parser::parse_do_while() {
    ASTNode* node = alloc(ASTKind::DO_WHILE_STMT);
    expect(TokenKind::DO, "expected do");
    node->u.do_while.body = parse_statement();
    expect(TokenKind::WHILE, "expected while");
    expect(TokenKind::LPAREN, "expected (");
    node->u.do_while.cond = parse_expression();
    expect(TokenKind::RPAREN, "expected )");
    expect(TokenKind::SEMICOLON, "expected ;");
    return node;
}

ASTNode* Parser::parse_return() {
    ASTNode* node = alloc(ASTKind::RETURN_STMT);
    expect(TokenKind::RETURN, "expected return");
    if (peek().kind != TokenKind::SEMICOLON) {
        node->u.ret.expr = parse_expression();
    }
    expect(TokenKind::SEMICOLON, "expected ; after return");
    return node;
}

ASTNode* Parser::parse_break() {
    expect(TokenKind::BREAK, "expected break");
    expect(TokenKind::SEMICOLON, "expected ;");
    return alloc(ASTKind::BREAK_STMT);
}

ASTNode* Parser::parse_continue() {
    expect(TokenKind::CONTINUE, "expected continue");
    expect(TokenKind::SEMICOLON, "expected ;");
    return alloc(ASTKind::CONTINUE_STMT);
}

ASTNode* Parser::parse_asm_stmt() {
    ASTNode* node = alloc(ASTKind::ASM_STMT);
    Token t = next();
    node->u.asm_stmt.code = str_dup(t.text);
    expect(TokenKind::SEMICOLON, "expected ; after asm");
    return node;
}

// ── Expressions ──────────────────────────────────────────────────────

ASTNode* Parser::parse_expression() { return parse_assignment(); }

ASTNode* Parser::parse_assignment() {
    ASTNode* node = parse_logical_or();
    Token t = peek();
    if (t.kind == TokenKind::EQ || t.kind == TokenKind::PLUS_EQ ||
        t.kind == TokenKind::MINUS_EQ || t.kind == TokenKind::STAR_EQ) {
        next();
        ASTNode* assign = alloc(ASTKind::ASSIGN);
        assign->u.assign.target = node;
        assign->u.assign.value = parse_assignment();
        return assign;
    }
    return node;
}

ASTNode* Parser::parse_logical_or() {
    ASTNode* node = parse_logical_and();
    while (match(TokenKind::OROR)) {
        ASTNode* bin = alloc(ASTKind::BINARY_OP);
        bin->u.binop.op = TokenKind::OROR;
        bin->u.binop.left = node;
        bin->u.binop.right = parse_logical_and();
        node = bin;
    }
    return node;
}

ASTNode* Parser::parse_logical_and() {
    ASTNode* node = parse_bitwise_or();
    while (match(TokenKind::ANDAND)) {
        ASTNode* bin = alloc(ASTKind::BINARY_OP);
        bin->u.binop.op = TokenKind::ANDAND;
        bin->u.binop.left = node;
        bin->u.binop.right = parse_bitwise_or();
        node = bin;
    }
    return node;
}

ASTNode* Parser::parse_bitwise_or() {
    ASTNode* node = parse_bitwise_xor();
    while (match(TokenKind::PIPE)) {
        ASTNode* bin = alloc(ASTKind::BINARY_OP);
        bin->u.binop.op = TokenKind::PIPE;
        bin->u.binop.left = node;
        bin->u.binop.right = parse_bitwise_xor();
        node = bin;
    }
    return node;
}

ASTNode* Parser::parse_bitwise_xor() {
    ASTNode* node = parse_bitwise_and();
    while (match(TokenKind::CARET)) {
        ASTNode* bin = alloc(ASTKind::BINARY_OP);
        bin->u.binop.op = TokenKind::CARET;
        bin->u.binop.left = node;
        bin->u.binop.right = parse_bitwise_and();
        node = bin;
    }
    return node;
}

ASTNode* Parser::parse_bitwise_and() {
    ASTNode* node = parse_equality();
    while (match(TokenKind::AMPER)) {
        ASTNode* bin = alloc(ASTKind::BINARY_OP);
        bin->u.binop.op = TokenKind::AMPER;
        bin->u.binop.left = node;
        bin->u.binop.right = parse_equality();
        node = bin;
    }
    return node;
}

ASTNode* Parser::parse_equality() {
    ASTNode* node = parse_relational();
    while (true) {
        if (match(TokenKind::EQEQ)) {
            ASTNode* bin = alloc(ASTKind::BINARY_OP);
            bin->u.binop.op = TokenKind::EQEQ;
            bin->u.binop.left = node;
            bin->u.binop.right = parse_relational();
            node = bin;
        } else if (match(TokenKind::NEQ)) {
            ASTNode* bin = alloc(ASTKind::BINARY_OP);
            bin->u.binop.op = TokenKind::NEQ;
            bin->u.binop.left = node;
            bin->u.binop.right = parse_relational();
            node = bin;
        } else break;
    }
    return node;
}

ASTNode* Parser::parse_relational() {
    ASTNode* node = parse_shift();
    while (true) {
        if (match(TokenKind::LT)) {
            ASTNode* bin = alloc(ASTKind::BINARY_OP);
            bin->u.binop.op = TokenKind::LT;
            bin->u.binop.left = node;
            bin->u.binop.right = parse_shift();
            node = bin;
        } else if (match(TokenKind::GT)) {
            ASTNode* bin = alloc(ASTKind::BINARY_OP);
            bin->u.binop.op = TokenKind::GT;
            bin->u.binop.left = node;
            bin->u.binop.right = parse_shift();
            node = bin;
        } else if (match(TokenKind::LE)) {
            ASTNode* bin = alloc(ASTKind::BINARY_OP);
            bin->u.binop.op = TokenKind::LE;
            bin->u.binop.left = node;
            bin->u.binop.right = parse_shift();
            node = bin;
        } else if (match(TokenKind::GE)) {
            ASTNode* bin = alloc(ASTKind::BINARY_OP);
            bin->u.binop.op = TokenKind::GE;
            bin->u.binop.left = node;
            bin->u.binop.right = parse_shift();
            node = bin;
        } else break;
    }
    return node;
}

ASTNode* Parser::parse_shift() {
    ASTNode* node = parse_additive();
    while (true) {
        if (match(TokenKind::LSHIFT)) {
            ASTNode* bin = alloc(ASTKind::BINARY_OP);
            bin->u.binop.op = TokenKind::LSHIFT;
            bin->u.binop.left = node;
            bin->u.binop.right = parse_additive();
            node = bin;
        } else if (match(TokenKind::RSHIFT)) {
            ASTNode* bin = alloc(ASTKind::BINARY_OP);
            bin->u.binop.op = TokenKind::RSHIFT;
            bin->u.binop.left = node;
            bin->u.binop.right = parse_additive();
            node = bin;
        } else break;
    }
    return node;
}

ASTNode* Parser::parse_additive() {
    ASTNode* node = parse_multiplicative();
    while (true) {
        if (match(TokenKind::PLUS)) {
            ASTNode* bin = alloc(ASTKind::BINARY_OP);
            bin->u.binop.op = TokenKind::PLUS;
            bin->u.binop.left = node;
            bin->u.binop.right = parse_multiplicative();
            node = bin;
        } else if (match(TokenKind::MINUS)) {
            ASTNode* bin = alloc(ASTKind::BINARY_OP);
            bin->u.binop.op = TokenKind::MINUS;
            bin->u.binop.left = node;
            bin->u.binop.right = parse_multiplicative();
            node = bin;
        } else break;
    }
    return node;
}

ASTNode* Parser::parse_multiplicative() {
    ASTNode* node = parse_cast();
    while (true) {
        if (match(TokenKind::STAR)) {
            ASTNode* bin = alloc(ASTKind::BINARY_OP);
            bin->u.binop.op = TokenKind::STAR;
            bin->u.binop.left = node;
            bin->u.binop.right = parse_cast();
            node = bin;
        } else if (match(TokenKind::SLASH)) {
            ASTNode* bin = alloc(ASTKind::BINARY_OP);
            bin->u.binop.op = TokenKind::SLASH;
            bin->u.binop.left = node;
            bin->u.binop.right = parse_cast();
            node = bin;
        } else if (match(TokenKind::PERCENT)) {
            ASTNode* bin = alloc(ASTKind::BINARY_OP);
            bin->u.binop.op = TokenKind::PERCENT;
            bin->u.binop.left = node;
            bin->u.binop.right = parse_cast();
            node = bin;
        } else break;
    }
    return node;
}

ASTNode* Parser::parse_cast() {
    if (peek().kind == TokenKind::LPAREN) {
        Token saved = peek();
        next();
        CType cast_type = parse_type_spec();
        if (peek().kind == TokenKind::RPAREN) {
            next();
            ASTNode* cast = alloc(ASTKind::CAST);
            cast->u.cast_expr.expr = parse_cast();
            cast->u.cast_expr.target_type = cast_type;
            return cast;
        }
        return parse_unary();
    }
    return parse_unary();
}

ASTNode* Parser::parse_unary() {
    Token t = peek();
    if (t.kind == TokenKind::PLUSPLUS || t.kind == TokenKind::MINUSMINUS) {
        next();
        ASTNode* un = alloc(ASTKind::UNARY_OP);
        un->u.unop.op = t.kind;
        un->u.unop.operand = parse_unary();
        un->u.unop.is_prefix = true;
        return un;
    }
    if (t.kind == TokenKind::AMPER) { next();
        ASTNode* addr = alloc(ASTKind::ADDRESS_OF);
        addr->u.addr_of.operand = parse_unary();
        return addr;
    }
    if (t.kind == TokenKind::STAR) { next();
        ASTNode* deref = alloc(ASTKind::DEREF);
        deref->u.deref.operand = parse_unary();
        return deref;
    }
    if (t.kind == TokenKind::PLUS) { next(); return parse_unary(); }
    if (t.kind == TokenKind::MINUS) { next();
        ASTNode* un = alloc(ASTKind::UNARY_OP);
        un->u.unop.op = TokenKind::MINUS;
        un->u.unop.operand = parse_unary();
        un->u.unop.is_prefix = true;
        return un;
    }
    if (t.kind == TokenKind::TILDE) { next();
        ASTNode* un = alloc(ASTKind::UNARY_OP);
        un->u.unop.op = TokenKind::TILDE;
        un->u.unop.operand = parse_unary();
        un->u.unop.is_prefix = true;
        return un;
    }
    if (t.kind == TokenKind::EXCLAM) { next();
        ASTNode* un = alloc(ASTKind::UNARY_OP);
        un->u.unop.op = TokenKind::EXCLAM;
        un->u.unop.operand = parse_unary();
        un->u.unop.is_prefix = true;
        return un;
    }
    if (t.kind == TokenKind::SIZEOF) { next();
        ASTNode* so = alloc(ASTKind::SIZEOF_EXPR);
        so->u.size_of.expr = parse_unary();
        return so;
    }
    return parse_postfix();
}

ASTNode* Parser::parse_postfix() {
    ASTNode* node = parse_primary();

    while (true) {
        if (match(TokenKind::LPAREN)) {
            if (node->kind == ASTKind::VARIABLE) {
                ASTNode* call = alloc(ASTKind::CALL);
                call->u.call.name = node->u.var.name;
                call->flags.call_name_alloced = 1;
                if (peek().kind != TokenKind::RPAREN) {
                    do {
                        call->u.call.args.push(parse_expression());
                    } while (match(TokenKind::COMMA));
                }
                expect(TokenKind::RPAREN, "expected )");
                node = call;
            }
        } else if (match(TokenKind::LBRACKET)) {
            ASTNode* idx = alloc(ASTKind::ARRAY_INDEX);
            idx->u.arr_idx.arr = node;
            idx->u.arr_idx.index = parse_expression();
            expect(TokenKind::RBRACKET, "expected ]");
            node = idx;
        } else if (match(TokenKind::DOT)) {
            Token name = expect(TokenKind::IDENT, "expected member name");
            ASTNode* mem = alloc(ASTKind::MEMBER_ACCESS);
            mem->u.member.obj = node;
            mem->u.member.member = str_dup(name.text);
            mem->flags.member_name_alloced = 1;
            node = mem;
        } else if (match(TokenKind::ARROW)) {
            Token name = expect(TokenKind::IDENT, "expected member name");
            ASTNode* deref = alloc(ASTKind::DEREF);
            deref->u.deref.operand = node;
            ASTNode* mem = alloc(ASTKind::MEMBER_ACCESS);
            mem->u.member.obj = deref;
            mem->u.member.member = str_dup(name.text);
            mem->flags.member_name_alloced = 1;
            node = mem;
        } else if (match(TokenKind::PLUSPLUS) || match(TokenKind::MINUSMINUS)) {
            ASTNode* un = alloc(ASTKind::UNARY_OP);
            un->u.unop.op = peek().kind == TokenKind::PLUSPLUS ? TokenKind::PLUSPLUS : TokenKind::MINUSMINUS;
            un->u.unop.operand = node;
            un->u.unop.is_prefix = false;
            node = un;
            next(); // consumed above in match
        } else break;
    }
    return node;
}

ASTNode* Parser::parse_primary() {
    Token t = peek();

    if (t.kind == TokenKind::LPAREN) { next();
        ASTNode* expr = parse_expression();
        expect(TokenKind::RPAREN, "expected )");
        return expr;
    }
    if (t.kind == TokenKind::NUMBER) { next();
        ASTNode* node = alloc(ASTKind::CONSTANT);
        node->u.constant.value = t.intval;
        return node;
    }
    if (t.kind == TokenKind::STRING) { next();
        ASTNode* node = alloc(ASTKind::STRING_LIT);
        node->u.string_lit.value = str_dup(t.text);
        return node;
    }
    if (t.kind == TokenKind::CHAR_LIT) { next();
        ASTNode* node = alloc(ASTKind::CONSTANT);
        node->u.constant.value = t.intval;
        return node;
    }
    if (t.kind == TokenKind::IDENT) { next();
        ASTNode* node = alloc(ASTKind::VARIABLE);
        node->u.var.name = str_dup(t.text);
        return node;
    }
    if (t.kind == TokenKind::INLINE_ASM) { next();
        ASTNode* node = alloc(ASTKind::ASM_STMT);
        node->u.asm_stmt.code = str_dup(t.text);
        return node;
    }

    throw std::runtime_error("unexpected token in expression: " + t.text);
}

// ── Top-level parse ──────────────────────────────────────────────────

ASTNode* Parser::parse() {
    ASTNode* program = alloc(ASTKind::PROGRAM);

    while (peek().kind != TokenKind::END) {
        program->u.func.params.push(parse_declaration(true));
    }

    return program;
}

} // namespace hvmc
