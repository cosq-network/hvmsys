#pragma once
#include "types.h"
#include <string>
#include <vector>

namespace hvmc {

class Lexer {
public:
    Lexer(const std::string& source);

    Token next();
    Token peek();
    Token expect(TokenKind kind, const std::string& errmsg);
    bool match(TokenKind kind);

    int line() const { return line_; }
    int col() const { return col_; }

private:
    std::string src_;
    size_t pos_ = 0;
    int line_ = 1;
    int col_ = 1;
    Token peeked_;
    bool has_peeked_ = false;

    char peek_char();
    char get();
    void skip_whitespace_and_comments();
    Token read_ident_or_keyword();
    Token read_number();
    Token read_string();
    Token read_char();
};

} // namespace hvmc
