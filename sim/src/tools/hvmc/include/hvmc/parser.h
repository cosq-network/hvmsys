#pragma once
#include "types.h"
#include "lexer.h"
#include <memory>
#include <unordered_map>

namespace hvmc {

class Parser {
public:
    Parser(const std::string& source);
    ASTNode* parse();

private:
    Lexer lex;
    std::vector<std::unique_ptr<ASTNode>> nodes;
    std::unordered_map<std::string, CType> symbol_table;
    std::unordered_map<std::string, std::vector<std::pair<std::string, CType>>> struct_table;

    Token peek();
    Token next();
    Token expect(TokenKind kind, const std::string& msg);
    bool match(TokenKind kind);

    ASTNode* alloc(ASTKind kind);
    ASTNode* current_function;

    // Type parsing
    CType parse_type_spec();
    CType parse_declarator(CType base, const char*& name);

    // Declarations
    ASTNode* parse_declaration(bool allow_function = true);
    ASTNode* parse_function_def(CType ret_type, const char* name);
    ASTNode* parse_variable_decl(CType type, const char* name, bool is_extern, bool is_static);

    // Statements
    ASTNode* parse_statement();
    ASTNode* parse_block();
    ASTNode* parse_if();
    ASTNode* parse_while();
    ASTNode* parse_for();
    ASTNode* parse_do_while();
    ASTNode* parse_return();
    ASTNode* parse_break();
    ASTNode* parse_continue();
    ASTNode* parse_asm_stmt();

    // Expressions
    ASTNode* parse_expression();
    ASTNode* parse_assignment();
    ASTNode* parse_logical_or();
    ASTNode* parse_logical_and();
    ASTNode* parse_bitwise_or();
    ASTNode* parse_bitwise_xor();
    ASTNode* parse_bitwise_and();
    ASTNode* parse_equality();
    ASTNode* parse_relational();
    ASTNode* parse_shift();
    ASTNode* parse_additive();
    ASTNode* parse_multiplicative();
    ASTNode* parse_cast();
    ASTNode* parse_unary();
    ASTNode* parse_postfix();
    ASTNode* parse_primary();
    ASTNode* parse_func_call(const char* name);
};

} // namespace hvmc
