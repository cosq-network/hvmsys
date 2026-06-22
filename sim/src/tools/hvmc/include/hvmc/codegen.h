#pragma once
#include "types.h"
#include <string>
#include <sstream>

namespace hvmc {

class CodeGen {
public:
    CodeGen();

    std::string gen(ASTNode* node);

private:
    std::string new_label();
    std::string reg_name(Reg r);

    void gen_function_def(ASTNode* node);
    void gen_variable_decl(ASTNode* node);
    void gen_if_stmt(ASTNode* node);
    void gen_while_stmt(ASTNode* node);
    void gen_for_stmt(ASTNode* node);
    void gen_do_while_stmt(ASTNode* node);
    void gen_expression(ASTNode* node, Reg target);
    void gen_condition(ASTNode* node, const std::string& label, bool invert = false);

    void emit(const std::string& line);
    void emit_line(const std::string& line);

    std::ostringstream output;
    int label_counter;
};

} // namespace hvmc
