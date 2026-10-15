// Shared lexer/parser types. Owned by Member 1 (contract) — see docs/contract.md §2.
#pragma once

#include <stdexcept>
#include <string>
#include <vector>

enum class TokenType {
    KEYWORD,
    IDENTIFIER,
    INT_LITERAL,
    FLOAT_LITERAL,
    PLUS,
    MINUS,
    MULTIPLY,
    DIVIDE,
    LT,
    GT,
    LE,
    GE,
    EQ,
    NE,
    ASSIGNMENT_OPERATOR,
    LPAREN,
    RPAREN,
    LBRACE,
    RBRACE,
    SEMICOLON,
    COMMA,
    END_OF_FILE,
};

// Name used in JSON, e.g. TokenType::PLUS -> "PLUS".
const char* tokenTypeName(TokenType type);

struct Token {
    TokenType type;
    std::string lexeme;
    int line;    // 1-based
    int column;  // 1-based, bytes
};

struct Comment {
    std::string text;  // exact source text including // or /* */
    int line;
    int column;
    int endLine;       // exclusive end, like AST nodes
    int endColumn;
};

struct CompileError {
    std::string stage;  // "lexer" | "parser"
    std::string message;
    int line;
    int column;
};

struct CompileException : std::runtime_error {
    CompileError error;
    explicit CompileException(CompileError e) : std::runtime_error(e.message), error(std::move(e)) {}
};
