// Lexer for the C subset. See docs/contract.md §2.
#include "lexer.h"

const char* tokenTypeName(TokenType type) {
    switch (type) {
        case TokenType::KEYWORD: return "KEYWORD";
        case TokenType::IDENTIFIER: return "IDENTIFIER";
        case TokenType::INT_LITERAL: return "INT_LITERAL";
        case TokenType::FLOAT_LITERAL: return "FLOAT_LITERAL";
        case TokenType::PLUS: return "PLUS";
        case TokenType::MINUS: return "MINUS";
        case TokenType::MULTIPLY: return "MULTIPLY";
        case TokenType::DIVIDE: return "DIVIDE";
        case TokenType::LT: return "LT";
        case TokenType::GT: return "GT";
        case TokenType::LE: return "LE";
        case TokenType::GE: return "GE";
        case TokenType::EQ: return "EQ";
        case TokenType::NE: return "NE";
        case TokenType::ASSIGNMENT_OPERATOR: return "ASSIGNMENT_OPERATOR";
        case TokenType::LPAREN: return "LPAREN";
        case TokenType::RPAREN: return "RPAREN";
        case TokenType::LBRACE: return "LBRACE";
        case TokenType::RBRACE: return "RBRACE";
        case TokenType::SEMICOLON: return "SEMICOLON";
        case TokenType::COMMA: return "COMMA";
        case TokenType::END_OF_FILE: return "END_OF_FILE";
    }
    return "UNKNOWN";
}

namespace {

bool isKeyword(const std::string& s) {
    static const char* const kKeywords[] = {"int", "float", "void", "if", "else", "while", "for", "return"};
    for (const char* k : kKeywords)
        if (s == k) return true;
    return false;
}

bool isDigit(char c) { return c >= '0' && c <= '9'; }
bool isIdentStart(char c) { return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'; }
bool isIdentChar(char c) { return isIdentStart(c) || isDigit(c); }

class Lexer {
public:
    explicit Lexer(const std::string& src) : src_(src) {}

    LexResult run() {
        while (pos_ < src_.size()) {
            char c = src_[pos_];
            if (c == ' ' || c == '\t' || c == '\r' || c == '\n' || c == '\v' || c == '\f') {
                advance();
            } else if (c == '/' && peek(1) == '/') {
                lineComment();
            } else if (c == '/' && peek(1) == '*') {
                if (!blockComment()) return std::move(result_);
            } else if (isIdentStart(c)) {
                identifier();
            } else if (isDigit(c) || (c == '.' && isDigit(peek(1)))) {
                number();
            } else if (!symbol()) {
                fail(std::string("unexpected character '") + c + "'", line_, col_);
                return std::move(result_);
            }
        }
        result_.tokens.push_back({TokenType::END_OF_FILE, "", line_, col_});
        return std::move(result_);
    }

private:
    const std::string& src_;
    size_t pos_ = 0;
    int line_ = 1;
    int col_ = 1;
    LexResult result_;

    char peek(size_t off) const { return pos_ + off < src_.size() ? src_[pos_ + off] : '\0'; }

    // Consume one byte, tracking line/column. '\r' is plain whitespace (1 column).
    void advance() {
        if (src_[pos_] == '\n') {
            ++line_;
            col_ = 1;
        } else {
            ++col_;
        }
        ++pos_;
    }

    void fail(std::string msg, int line, int col) {
        result_.error = CompileError{"lexer", std::move(msg), line, col};
    }

    void add(TokenType type, size_t start, int line, int col) {
        result_.tokens.push_back({type, src_.substr(start, pos_ - start), line, col});
    }

    void lineComment() {
        size_t start = pos_;
        int line = line_, col = col_;
        // Stop before '\n'; strip a trailing '\r' from a CRLF line.
        while (pos_ < src_.size() && src_[pos_] != '\n') advance();
        size_t end = pos_;
        int endCol = col_;
        if (end > start && src_[end - 1] == '\r') {
            --end;
            --endCol;
        }
        result_.comments.push_back({src_.substr(start, end - start), line, col, line_, endCol});
    }

    bool blockComment() {
        size_t start = pos_;
        int line = line_, col = col_;
        advance();
        advance();
        while (pos_ < src_.size()) {
            if (src_[pos_] == '*' && peek(1) == '/') {
                advance();
                advance();
                result_.comments.push_back({src_.substr(start, pos_ - start), line, col, line_, col_});
                return true;
            }
            advance();
        }
        fail("unterminated block comment", line, col);
        return false;
    }

    void identifier() {
        size_t start = pos_;
        int line = line_, col = col_;
        while (pos_ < src_.size() && isIdentChar(src_[pos_])) advance();
        std::string text = src_.substr(start, pos_ - start);
        result_.tokens.push_back({isKeyword(text) ? TokenType::KEYWORD : TokenType::IDENTIFIER, text, line, col});
    }

    // [0-9]+ | [0-9]+ "." [0-9]* | "." [0-9]+
    void number() {
        size_t start = pos_;
        int line = line_, col = col_;
        bool isFloat = false;
        while (pos_ < src_.size() && isDigit(src_[pos_])) advance();
        if (pos_ < src_.size() && src_[pos_] == '.') {
            isFloat = true;
            advance();
            while (pos_ < src_.size() && isDigit(src_[pos_])) advance();
        }
        add(isFloat ? TokenType::FLOAT_LITERAL : TokenType::INT_LITERAL, start, line, col);
    }

    // Operators and punctuation; two-char forms are tried first.
    bool symbol() {
        size_t start = pos_;
        int line = line_, col = col_;
        char c = src_[pos_];
        char n = peek(1);
        TokenType type;
        int len = 1;
        switch (c) {
            case '<': type = n == '=' ? (len = 2, TokenType::LE) : TokenType::LT; break;
            case '>': type = n == '=' ? (len = 2, TokenType::GE) : TokenType::GT; break;
            case '=': type = n == '=' ? (len = 2, TokenType::EQ) : TokenType::ASSIGNMENT_OPERATOR; break;
            case '!':
                if (n != '=') return false;
                type = TokenType::NE;
                len = 2;
                break;
            case '+': type = TokenType::PLUS; break;
            case '-': type = TokenType::MINUS; break;
            case '*': type = TokenType::MULTIPLY; break;
            case '/': type = TokenType::DIVIDE; break;
            case '(': type = TokenType::LPAREN; break;
            case ')': type = TokenType::RPAREN; break;
            case '{': type = TokenType::LBRACE; break;
            case '}': type = TokenType::RBRACE; break;
            case ';': type = TokenType::SEMICOLON; break;
            case ',': type = TokenType::COMMA; break;
            default: return false;
        }
        for (int i = 0; i < len; ++i) advance();
        add(type, start, line, col);
        return true;
    }
};

}  // namespace

LexResult lex(const std::string& source) {
    Lexer lexer(source);
    return lexer.run();
}
