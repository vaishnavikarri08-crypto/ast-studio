// Lexer tests (Member 2). See docs/contract.md §2.
#include <string>
#include <vector>

#include "check.h"
#include "lexer.h"

namespace {

std::string types(const LexResult& r) {
    std::string out;
    for (const Token& t : r.tokens) {
        if (!out.empty()) out += " ";
        out += tokenTypeName(t.type);
    }
    return out;
}

std::string lexemes(const LexResult& r) {
    std::string out;
    for (const Token& t : r.tokens) {
        if (!out.empty()) out += "|";
        out += t.lexeme;
    }
    return out;
}

void checkTok(const Token& t, TokenType type, const std::string& lexeme, int line, int col) {
    CHECK_EQ(std::string(tokenTypeName(t.type)), std::string(tokenTypeName(type)));
    CHECK_EQ(t.lexeme, lexeme);
    CHECK_EQ(t.line, line);
    CHECK_EQ(t.column, col);
}

}  // namespace

TEST(prd_example) {
    LexResult r = lex("int result=a+b*c;");
    CHECK(!r.error);
    CHECK_EQ(r.tokens.size(), size_t(10));
    if (r.tokens.size() != 10) return;
    checkTok(r.tokens[0], TokenType::KEYWORD, "int", 1, 1);
    checkTok(r.tokens[1], TokenType::IDENTIFIER, "result", 1, 5);
    checkTok(r.tokens[2], TokenType::ASSIGNMENT_OPERATOR, "=", 1, 11);
    checkTok(r.tokens[3], TokenType::IDENTIFIER, "a", 1, 12);
    checkTok(r.tokens[4], TokenType::PLUS, "+", 1, 13);
    checkTok(r.tokens[5], TokenType::IDENTIFIER, "b", 1, 14);
    checkTok(r.tokens[6], TokenType::MULTIPLY, "*", 1, 15);
    checkTok(r.tokens[7], TokenType::IDENTIFIER, "c", 1, 16);
    checkTok(r.tokens[8], TokenType::SEMICOLON, ";", 1, 17);
    checkTok(r.tokens[9], TokenType::END_OF_FILE, "", 1, 18);
}

TEST(multi_line_positions) {
    LexResult r = lex("int x;\n  x = 1;\n\tif (x) {}\n");
    CHECK(!r.error);
    CHECK_EQ(r.tokens.size(), size_t(14));
    if (r.tokens.size() != 14) return;
    checkTok(r.tokens[3], TokenType::IDENTIFIER, "x", 2, 3);
    checkTok(r.tokens[5], TokenType::INT_LITERAL, "1", 2, 7);
    checkTok(r.tokens[7], TokenType::KEYWORD, "if", 3, 2);  // tab = 1 column
    checkTok(r.tokens[8], TokenType::LPAREN, "(", 3, 5);
    checkTok(r.tokens[12], TokenType::RBRACE, "}", 3, 10);
    checkTok(r.tokens[13], TokenType::END_OF_FILE, "", 4, 1);
}

TEST(crlf_line_endings) {
    LexResult r = lex("a\r\nb\r\n");
    CHECK(!r.error);
    CHECK_EQ(types(r), std::string("IDENTIFIER IDENTIFIER END_OF_FILE"));
    if (r.tokens.size() != 3) return;
    checkTok(r.tokens[1], TokenType::IDENTIFIER, "b", 2, 1);
    checkTok(r.tokens[2], TokenType::END_OF_FILE, "", 3, 1);
}

TEST(every_operator) {
    LexResult r = lex("+ - * / < > <= >= == != = ( ) { } ; ,");
    CHECK(!r.error);
    CHECK_EQ(types(r), std::string("PLUS MINUS MULTIPLY DIVIDE LT GT LE GE EQ NE ASSIGNMENT_OPERATOR "
                                   "LPAREN RPAREN LBRACE RBRACE SEMICOLON COMMA END_OF_FILE"));
    CHECK_EQ(lexemes(r), std::string("+|-|*|/|<|>|<=|>=|==|!=|=|(|)|{|}|;|,|"));
}

TEST(two_char_operators_without_spaces) {
    LexResult r = lex("a<=b>=c==d!=e<f>g=h");
    CHECK(!r.error);
    CHECK_EQ(types(r), std::string("IDENTIFIER LE IDENTIFIER GE IDENTIFIER EQ IDENTIFIER NE IDENTIFIER "
                                   "LT IDENTIFIER GT IDENTIFIER ASSIGNMENT_OPERATOR IDENTIFIER END_OF_FILE"));
    LexResult r2 = lex("x===y");  // == then =
    CHECK_EQ(types(r2), std::string("IDENTIFIER EQ ASSIGNMENT_OPERATOR IDENTIFIER END_OF_FILE"));
}

TEST(keywords_vs_identifiers) {
    LexResult r = lex("int float void if else while for return iffy _int int2 Int returns");
    CHECK(!r.error);
    CHECK_EQ(types(r), std::string("KEYWORD KEYWORD KEYWORD KEYWORD KEYWORD KEYWORD KEYWORD KEYWORD "
                                   "IDENTIFIER IDENTIFIER IDENTIFIER IDENTIFIER IDENTIFIER END_OF_FILE"));
}

TEST(number_literals) {
    LexResult r = lex("42 3.14 1. .5 0 007");
    CHECK(!r.error);
    CHECK_EQ(types(r), std::string("INT_LITERAL FLOAT_LITERAL FLOAT_LITERAL FLOAT_LITERAL INT_LITERAL "
                                   "INT_LITERAL END_OF_FILE"));
    CHECK_EQ(lexemes(r), std::string("42|3.14|1.|.5|0|007|"));
    if (r.tokens.size() < 4) return;
    checkTok(r.tokens[3], TokenType::FLOAT_LITERAL, ".5", 1, 12);
}

TEST(number_then_identifier) {
    LexResult r = lex("1.5f 12ab");
    CHECK_EQ(types(r), std::string("FLOAT_LITERAL IDENTIFIER INT_LITERAL IDENTIFIER END_OF_FILE"));
}

TEST(line_comment) {
    LexResult r = lex("int x; // hi there\nx = 1;");
    CHECK(!r.error);
    CHECK_EQ(r.comments.size(), size_t(1));
    if (r.comments.empty()) return;
    const Comment& c = r.comments[0];
    CHECK_EQ(c.text, std::string("// hi there"));
    CHECK_EQ(c.line, 1);
    CHECK_EQ(c.column, 8);
    CHECK_EQ(c.endLine, 1);
    CHECK_EQ(c.endColumn, 19);
    CHECK_EQ(types(r), std::string("KEYWORD IDENTIFIER SEMICOLON IDENTIFIER ASSIGNMENT_OPERATOR "
                                   "INT_LITERAL SEMICOLON END_OF_FILE"));
}

TEST(line_comment_crlf_and_eof) {
    LexResult r = lex("// a\r\n// b");
    CHECK(!r.error);
    CHECK_EQ(r.comments.size(), size_t(2));
    if (r.comments.size() != 2) return;
    CHECK_EQ(r.comments[0].text, std::string("// a"));
    CHECK_EQ(r.comments[0].endColumn, 5);
    CHECK_EQ(r.comments[1].text, std::string("// b"));
    CHECK_EQ(r.comments[1].line, 2);
    CHECK_EQ(r.comments[1].endColumn, 5);
    CHECK_EQ(r.tokens.size(), size_t(1));
    checkTok(r.tokens[0], TokenType::END_OF_FILE, "", 2, 5);
}

TEST(block_comment_single_line) {
    LexResult r = lex("a + /* x */ b");
    CHECK(!r.error);
    CHECK_EQ(types(r), std::string("IDENTIFIER PLUS IDENTIFIER END_OF_FILE"));
    CHECK_EQ(r.comments.size(), size_t(1));
    if (r.comments.empty()) return;
    const Comment& c = r.comments[0];
    CHECK_EQ(c.text, std::string("/* x */"));
    CHECK_EQ(c.line, 1);
    CHECK_EQ(c.column, 5);
    CHECK_EQ(c.endLine, 1);
    CHECK_EQ(c.endColumn, 12);
    checkTok(r.tokens[2], TokenType::IDENTIFIER, "b", 1, 13);
}

TEST(block_comment_multi_line) {
    LexResult r = lex("x;\n/* one\n   two */ y;\n");
    CHECK(!r.error);
    CHECK_EQ(r.comments.size(), size_t(1));
    if (r.comments.empty()) return;
    const Comment& c = r.comments[0];
    CHECK_EQ(c.text, std::string("/* one\n   two */"));
    CHECK_EQ(c.line, 2);
    CHECK_EQ(c.column, 1);
    CHECK_EQ(c.endLine, 3);
    CHECK_EQ(c.endColumn, 10);
    CHECK_EQ(r.tokens.size(), size_t(5));
    if (r.tokens.size() != 5) return;
    checkTok(r.tokens[2], TokenType::IDENTIFIER, "y", 3, 11);
    checkTok(r.tokens[4], TokenType::END_OF_FILE, "", 4, 1);
}

TEST(block_comment_edge_cases) {
    LexResult r = lex("/**/a/* // */b/***/");
    CHECK(!r.error);
    CHECK_EQ(lexemes(r), std::string("a|b|"));
    CHECK_EQ(r.comments.size(), size_t(3));
    if (r.comments.size() != 3) return;
    CHECK_EQ(r.comments[0].text, std::string("/**/"));
    CHECK_EQ(r.comments[1].text, std::string("/* // */"));
    CHECK_EQ(r.comments[2].text, std::string("/***/"));
    CHECK_EQ(r.comments[2].column, 15);
    CHECK_EQ(r.comments[2].endColumn, 20);
}

TEST(unterminated_block_comment) {
    LexResult r = lex("int x;\n  /* never closed\nint y;");
    CHECK(r.error.has_value());
    if (!r.error) return;
    CHECK_EQ(r.error->stage, std::string("lexer"));
    CHECK_EQ(r.error->message, std::string("unterminated block comment"));
    CHECK_EQ(r.error->line, 2);
    CHECK_EQ(r.error->column, 3);
    CHECK_EQ(types(r), std::string("KEYWORD IDENTIFIER SEMICOLON"));  // no END_OF_FILE
    CHECK(r.comments.empty());

    LexResult r2 = lex("/*/");
    CHECK(r2.error.has_value());
    if (r2.error) CHECK_EQ(r2.error->message, std::string("unterminated block comment"));
}

TEST(unexpected_character) {
    LexResult r = lex("int a;\na = b @ c;");
    CHECK(r.error.has_value());
    if (!r.error) return;
    CHECK_EQ(r.error->stage, std::string("lexer"));
    CHECK_EQ(r.error->message, std::string("unexpected character '@'"));
    CHECK_EQ(r.error->line, 2);
    CHECK_EQ(r.error->column, 7);
    CHECK_EQ(types(r), std::string("KEYWORD IDENTIFIER SEMICOLON IDENTIFIER ASSIGNMENT_OPERATOR IDENTIFIER"));
}

TEST(lone_bang_is_error) {
    LexResult r = lex("!x");
    CHECK(r.error.has_value());
    if (r.error) {
        CHECK_EQ(r.error->message, std::string("unexpected character '!'"));
        CHECK_EQ(r.error->column, 1);
    }
    CHECK(r.tokens.empty());
}

TEST(lone_dot_is_error) {
    LexResult r = lex("a . b");
    CHECK(r.error.has_value());
    if (r.error) {
        CHECK_EQ(r.error->message, std::string("unexpected character '.'"));
        CHECK_EQ(r.error->column, 3);
    }
}

TEST(eof_position) {
    LexResult empty = lex("");
    CHECK_EQ(empty.tokens.size(), size_t(1));
    checkTok(empty.tokens[0], TokenType::END_OF_FILE, "", 1, 1);

    LexResult r = lex("x");
    checkTok(r.tokens.back(), TokenType::END_OF_FILE, "", 1, 2);

    LexResult nl = lex("x\n");
    checkTok(nl.tokens.back(), TokenType::END_OF_FILE, "", 2, 1);

    LexResult trailing = lex("x  ");
    checkTok(trailing.tokens.back(), TokenType::END_OF_FILE, "", 1, 4);
}

TEST(token_type_names) {
    CHECK_EQ(std::string(tokenTypeName(TokenType::KEYWORD)), std::string("KEYWORD"));
    CHECK_EQ(std::string(tokenTypeName(TokenType::ASSIGNMENT_OPERATOR)), std::string("ASSIGNMENT_OPERATOR"));
    CHECK_EQ(std::string(tokenTypeName(TokenType::END_OF_FILE)), std::string("END_OF_FILE"));
}

TEST(function_with_call_and_for) {
    LexResult r = lex("int f(int a, float b) { for (i = 0; i < a; i = i + 1) { g(a, b); } return a; }");
    CHECK(!r.error);
    CHECK(r.tokens.size() > 30);
    CHECK(r.tokens.back().type == TokenType::END_OF_FILE);
}

RUN_TESTS()
