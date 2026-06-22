#include "hvmc/lexer.h"
#include <cctype>
#include <stdexcept>

namespace hvmc {

Lexer::Lexer(const std::string& source) : src_(source) {}

char Lexer::peek_char() {
    if (pos_ >= src_.size()) return '\0';
    return src_[pos_];
}

char Lexer::get() {
    if (pos_ >= src_.size()) return '\0';
    char c = src_[pos_++];
    if (c == '\n') { line_++; col_ = 1; }
    else col_++;
    return c;
}

void Lexer::skip_whitespace_and_comments() {
    while (pos_ < src_.size()) {
        char c = peek_char();
        if (c == ' ' || c == '\t' || c == '\r') { get(); continue; }
        if (c == '\n') break;
        if (c == '/' && pos_ + 1 < src_.size()) {
            if (src_[pos_ + 1] == '/') {
                while (pos_ < src_.size() && src_[pos_] != '\n') get();
                continue;
            }
            if (src_[pos_ + 1] == '*') {
                get(); get();
                while (pos_ + 1 < src_.size() && !(src_[pos_] == '*' && src_[pos_ + 1] == '/')) get();
                if (pos_ + 1 < src_.size()) { get(); get(); }
                continue;
            }
        }
        break;
    }
}

Token Lexer::next() {
    if (has_peeked_) {
        has_peeked_ = false;
        return peeked_;
    }

    skip_whitespace_and_comments();

    Token tok{};
    tok.line = line_;
    tok.col = col_;

    char c = peek_char();
    if (c == '\0') { tok.kind = TokenKind::END; return tok; }

    if (c == '\n') {
        get();
        return next();
    }

    if (std::isalpha(c) || c == '_') return read_ident_or_keyword();
    if (std::isdigit(c)) return read_number();
    if (c == '"') return read_string();
    if (c == '\'') return read_char();

    if (c == '=') {
        get();
        if (peek_char() == '=') { get(); tok.kind = TokenKind::EQEQ; return tok; }
        tok.kind = TokenKind::EQ;
        tok.text = "=";
        return tok;
    }
    if (c == '!') {
        get();
        if (peek_char() == '=') { get(); tok.kind = TokenKind::NEQ; return tok; }
        tok.kind = TokenKind::EXCLAM;
        return tok;
    }
    if (c == '<') {
        get();
        if (peek_char() == '=') { get(); tok.kind = TokenKind::LE; return tok; }
        if (peek_char() == '<') {
            get();
            if (peek_char() == '=') { get(); tok.kind = TokenKind::LSHIFT_EQ; return tok; }
            tok.kind = TokenKind::LSHIFT;
            return tok;
        }
        tok.kind = TokenKind::LT;
        return tok;
    }
    if (c == '>') {
        get();
        if (peek_char() == '=') { get(); tok.kind = TokenKind::GE; return tok; }
        if (peek_char() == '>') {
            get();
            if (peek_char() == '=') { get(); tok.kind = TokenKind::RSHIFT_EQ; return tok; }
            tok.kind = TokenKind::RSHIFT;
            return tok;
        }
        tok.kind = TokenKind::GT;
        return tok;
    }
    if (c == '&') {
        get();
        if (peek_char() == '&') { get(); tok.kind = TokenKind::ANDAND; return tok; }
        if (peek_char() == '=') { get(); tok.kind = TokenKind::AMPER_EQ; return tok; }
        tok.kind = TokenKind::AMPER;
        return tok;
    }
    if (c == '|') {
        get();
        if (peek_char() == '|') { get(); tok.kind = TokenKind::OROR; return tok; }
        if (peek_char() == '=') { get(); tok.kind = TokenKind::PIPE_EQ; return tok; }
        tok.kind = TokenKind::PIPE;
        return tok;
    }
    if (c == '+') {
        get();
        if (peek_char() == '+') { get(); tok.kind = TokenKind::PLUSPLUS; return tok; }
        if (peek_char() == '=') { get(); tok.kind = TokenKind::PLUS_EQ; return tok; }
        tok.kind = TokenKind::PLUS;
        return tok;
    }
    if (c == '-') {
        get();
        if (peek_char() == '-') { get(); tok.kind = TokenKind::MINUSMINUS; return tok; }
        if (peek_char() == '=') { get(); tok.kind = TokenKind::MINUS_EQ; return tok; }
        if (peek_char() == '>') { get(); tok.kind = TokenKind::ARROW; return tok; }
        tok.kind = TokenKind::MINUS;
        return tok;
    }
    if (c == '*') {
        get();
        if (peek_char() == '=') { get(); tok.kind = TokenKind::STAR_EQ; return tok; }
        tok.kind = TokenKind::STAR;
        return tok;
    }
    if (c == '/') {
        get();
        if (peek_char() == '=') { get(); tok.kind = TokenKind::SLASH_EQ; return tok; }
        tok.kind = TokenKind::SLASH;
        return tok;
    }
    if (c == '%') {
        get();
        if (peek_char() == '=') { get(); tok.kind = TokenKind::PERCENT_EQ; return tok; }
        tok.kind = TokenKind::PERCENT;
        return tok;
    }
    if (c == '^') {
        get();
        if (peek_char() == '=') { get(); tok.kind = TokenKind::CARET_EQ; return tok; }
        tok.kind = TokenKind::CARET;
        return tok;
    }

    char ch = get();
    tok.text = std::string(1, ch);
    switch (ch) {
    case '(': tok.kind = TokenKind::LPAREN; break;
    case ')': tok.kind = TokenKind::RPAREN; break;
    case '{': tok.kind = TokenKind::LBRACE; break;
    case '}': tok.kind = TokenKind::RBRACE; break;
    case '[': tok.kind = TokenKind::LBRACKET; break;
    case ']': tok.kind = TokenKind::RBRACKET; break;
    case ';': tok.kind = TokenKind::SEMICOLON; break;
    case ',': tok.kind = TokenKind::COMMA; break;
    case '.': tok.kind = TokenKind::DOT; break;
    case '~': tok.kind = TokenKind::TILDE; break;
    case '?': tok.kind = TokenKind::QUESTION; break;
    case ':': tok.kind = TokenKind::COLON; break;
    default:
        throw std::runtime_error(std::string("unexpected character: ") + ch);
    }
    return tok;
}

Token Lexer::peek() {
    if (!has_peeked_) {
        peeked_ = next();
        has_peeked_ = true;
    }
    return peeked_;
}

Token Lexer::expect(TokenKind kind, const std::string& errmsg) {
    Token tok = next();
    if (tok.kind != kind) {
        throw std::runtime_error(errmsg + " at line " + std::to_string(line_));
    }
    return tok;
}

bool Lexer::match(TokenKind kind) {
    if (peek().kind == kind) {
        next();
        return true;
    }
    return false;
}

Token Lexer::read_ident_or_keyword() {
    Token tok{};
    tok.line = line_;
    tok.col = col_;
    std::string text;
    while (pos_ < src_.size() && (std::isalnum(src_[pos_]) || src_[pos_] == '_')) {
        text += get();
    }
    tok.text = text;

    if (text == "asm") {
        skip_whitespace_and_comments();
        if (peek_char() == '{') {
            get();
            std::string asm_code;
            int depth = 1;
            while (pos_ < src_.size() && depth > 0) {
                char c = get();
                if (c == '{') depth++;
                else if (c == '}') depth--;
                if (depth > 0) asm_code += c;
            }
            tok.kind = TokenKind::INLINE_ASM;
            tok.text = asm_code;
            return tok;
        }
    }

    if (text == "if") { tok.kind = TokenKind::IF; return tok; }
    if (text == "else") { tok.kind = TokenKind::ELSE; return tok; }
    if (text == "while") { tok.kind = TokenKind::WHILE; return tok; }
    if (text == "for") { tok.kind = TokenKind::FOR; return tok; }
    if (text == "do") { tok.kind = TokenKind::DO; return tok; }
    if (text == "switch") { tok.kind = TokenKind::SWITCH; return tok; }
    if (text == "case") { tok.kind = TokenKind::CASE; return tok; }
    if (text == "default") { tok.kind = TokenKind::DEFAULT; return tok; }
    if (text == "break") { tok.kind = TokenKind::BREAK; return tok; }
    if (text == "continue") { tok.kind = TokenKind::CONTINUE; return tok; }
    if (text == "return") { tok.kind = TokenKind::RETURN; return tok; }
    if (text == "goto") { tok.kind = TokenKind::GOTO; return tok; }
    if (text == "int") { tok.kind = TokenKind::INT_KW; return tok; }
    if (text == "char") { tok.kind = TokenKind::CHAR_KW; return tok; }
    if (text == "short") { tok.kind = TokenKind::SHORT_KW; return tok; }
    if (text == "long") { tok.kind = TokenKind::LONG_KW; return tok; }
    if (text == "unsigned") { tok.kind = TokenKind::UNSIGNED; return tok; }
    if (text == "signed") { tok.kind = TokenKind::SIGNED; return tok; }
    if (text == "void") { tok.kind = TokenKind::VOID_KW; return tok; }
    if (text == "static") { tok.kind = TokenKind::STATIC; return tok; }
    if (text == "extern") { tok.kind = TokenKind::EXTERN; return tok; }
    if (text == "const") { tok.kind = TokenKind::CONST; return tok; }
    if (text == "volatile") { tok.kind = TokenKind::VOLATILE; return tok; }
    if (text == "typedef") { tok.kind = TokenKind::TYPEDEF; return tok; }
    if (text == "struct") { tok.kind = TokenKind::STRUCT_KW; return tok; }
    if (text == "sizeof") { tok.kind = TokenKind::SIZEOF; return tok; }

    tok.kind = TokenKind::IDENT;
    return tok;
}

Token Lexer::read_number() {
    Token tok{};
    tok.line = line_;
    tok.col = col_;

    std::string num;
    if (peek_char() == '0' && pos_ + 1 < src_.size() &&
        (src_[pos_ + 1] == 'x' || src_[pos_ + 1] == 'X')) {
        num += get(); num += get();
        while (pos_ < src_.size() && std::isxdigit(src_[pos_])) num += get();
        tok.intval = std::strtoull(num.c_str(), nullptr, 16);
    } else {
        while (pos_ < src_.size() && std::isdigit(src_[pos_])) num += get();
        tok.intval = std::strtoull(num.c_str(), nullptr, 10);
    }

    tok.kind = TokenKind::NUMBER;
    tok.text = num;
    return tok;
}

Token Lexer::read_string() {
    Token tok{};
    tok.line = line_;
    tok.col = col_;
    tok.kind = TokenKind::STRING;

    get();
    std::string s;
    while (pos_ < src_.size() && src_[pos_] != '"') {
        if (src_[pos_] == '\\' && pos_ + 1 < src_.size()) {
            pos_++; col_++;
            switch (src_[pos_]) {
            case 'n': s += '\n'; break;
            case 'r': s += '\r'; break;
            case 't': s += '\t'; break;
            case '0': s += '\0'; break;
            case '\\': s += '\\'; break;
            case '"': s += '"'; break;
            default: s += src_[pos_]; break;
            }
            pos_++; col_++;
        } else {
            s += get();
        }
    }
    if (pos_ < src_.size()) get();
    tok.text = s;
    return tok;
}

Token Lexer::read_char() {
    Token tok{};
    tok.line = line_;
    tok.col = col_;
    tok.kind = TokenKind::CHAR_LIT;

    get();
    if (pos_ < src_.size() && src_[pos_] == '\\') {
        get();
        if (pos_ < src_.size()) {
            switch (src_[pos_]) {
            case 'n': tok.intval = '\n'; break;
            case 'r': tok.intval = '\r'; break;
            case 't': tok.intval = '\t'; break;
            case '0': tok.intval = '\0'; break;
            default: tok.intval = src_[pos_]; break;
            }
            get();
        }
    } else if (pos_ < src_.size()) {
        tok.intval = src_[pos_];
        get();
    }
    if (pos_ < src_.size() && src_[pos_] == '\'') get();
    return tok;
}

} // namespace hvmc
