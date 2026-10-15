// Parser tests. Member 3. Tokens are built by a tiny local tokenizer so these
// tests do not depend on the real lexer.
#include <cctype>

#include "check.h"
#include "parser.h"

namespace {

// Minimal tokenizer for test input (no comments, no error handling).
std::vector<Token> toks(const std::string& src) {
    std::vector<Token> out;
    int line = 1, col = 1;
    size_t i = 0;
    auto push = [&](TokenType t, size_t len) {
        out.push_back({t, src.substr(i, len), line, col});
        i += len;
        col += static_cast<int>(len);
    };
    while (i < src.size()) {
        char c = src[i];
        if (c == '\n') { ++line; col = 1; ++i; continue; }
        if (std::isspace(static_cast<unsigned char>(c))) { ++col; ++i; continue; }
        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            size_t j = i;
            while (j < src.size() && (std::isalnum(static_cast<unsigned char>(src[j])) || src[j] == '_')) ++j;
            std::string w = src.substr(i, j - i);
            bool kw = w == "int" || w == "float" || w == "void" || w == "if" || w == "else" ||
                      w == "while" || w == "for" || w == "return";
            push(kw ? TokenType::KEYWORD : TokenType::IDENTIFIER, j - i);
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(c))) {
            size_t j = i;
            while (j < src.size() && std::isdigit(static_cast<unsigned char>(src[j]))) ++j;
            bool isFloat = j < src.size() && src[j] == '.';
            if (isFloat) {
                ++j;
                while (j < src.size() && std::isdigit(static_cast<unsigned char>(src[j]))) ++j;
            }
            push(isFloat ? TokenType::FLOAT_LITERAL : TokenType::INT_LITERAL, j - i);
            continue;
        }
        std::string two = src.substr(i, 2);
        if (two == "<=") { push(TokenType::LE, 2); continue; }
        if (two == ">=") { push(TokenType::GE, 2); continue; }
        if (two == "==") { push(TokenType::EQ, 2); continue; }
        if (two == "!=") { push(TokenType::NE, 2); continue; }
        switch (c) {
            case '+': push(TokenType::PLUS, 1); break;
            case '-': push(TokenType::MINUS, 1); break;
            case '*': push(TokenType::MULTIPLY, 1); break;
            case '/': push(TokenType::DIVIDE, 1); break;
            case '<': push(TokenType::LT, 1); break;
            case '>': push(TokenType::GT, 1); break;
            case '=': push(TokenType::ASSIGNMENT_OPERATOR, 1); break;
            case '(': push(TokenType::LPAREN, 1); break;
            case ')': push(TokenType::RPAREN, 1); break;
            case '{': push(TokenType::LBRACE, 1); break;
            case '}': push(TokenType::RBRACE, 1); break;
            case ';': push(TokenType::SEMICOLON, 1); break;
            case ',': push(TokenType::COMMA, 1); break;
            default: ++i; ++col; break;
        }
    }
    out.push_back({TokenType::END_OF_FILE, "", line, col});
    return out;
}

NodePtr p(const std::string& src) { return parse(toks(src)); }

// Compact structural form: Kind[attr](child child ...), no positions.
std::string sx(const Node& n) {
    std::string s = n.kind;
    std::string attr = n.varType;
    for (const std::string* f : {&n.name, &n.op, &n.value}) {
        if (f->empty()) continue;
        if (!attr.empty()) attr += " ";
        attr += *f;
    }
    if (!attr.empty()) s += "[" + attr + "]";
    if (!n.children.empty()) {
        s += "(";
        for (size_t i = 0; i < n.children.size(); ++i) {
            if (i) s += " ";
            s += sx(*n.children[i]);
        }
        s += ")";
    }
    return s;
}

std::string sxp(const std::string& src) { return sx(*p(src)); }

std::string pos(const Node& n) {
    return std::to_string(n.line) + ":" + std::to_string(n.column) + "-" + std::to_string(n.endLine) +
           ":" + std::to_string(n.endColumn);
}

// Returns "line:col message" of the parse error, or "" if parsing succeeded.
std::string err(const std::string& src) {
    try {
        p(src);
    } catch (const CompileException& e) {
        CHECK_EQ(e.error.stage, std::string("parser"));
        return std::to_string(e.error.line) + ":" + std::to_string(e.error.column) + " " + e.error.message;
    }
    return "";
}

}  // namespace

TEST(prd_example) {
    CHECK_EQ(sxp("int result = a + b * c;"),
             std::string("Program(Declaration[int result](BinaryOp[+](Identifier[a] "
                         "BinaryOp[*](Identifier[b] Identifier[c]))))"));
}

TEST(empty_program) {
    auto prog = p("");
    CHECK_EQ(sx(*prog), std::string("Program"));
    CHECK_EQ(pos(*prog), std::string("1:1-1:1"));
}

TEST(left_associativity) {
    CHECK_EQ(sxp("x = a - b - c;"),
             std::string("Program(Assignment[x](BinaryOp[-](BinaryOp[-](Identifier[a] Identifier[b]) "
                         "Identifier[c])))"));
    CHECK_EQ(sxp("x = a / b * c;"),
             std::string("Program(Assignment[x](BinaryOp[*](BinaryOp[/](Identifier[a] Identifier[b]) "
                         "Identifier[c])))"));
}

TEST(precedence_and_parens) {
    CHECK_EQ(sxp("x = (a + b) * c;"),
             std::string("Program(Assignment[x](BinaryOp[*](BinaryOp[+](Identifier[a] Identifier[b]) "
                         "Identifier[c])))"));
    CHECK_EQ(sxp("x = a - (b - c);"),
             std::string("Program(Assignment[x](BinaryOp[-](Identifier[a] BinaryOp[-](Identifier[b] "
                         "Identifier[c]))))"));
    CHECK_EQ(sxp("x = a + 1 < b * 2;"),
             std::string("Program(Assignment[x](BinaryOp[<](BinaryOp[+](Identifier[a] IntLiteral[1]) "
                         "BinaryOp[*](Identifier[b] IntLiteral[2]))))"));
}

TEST(unary_minus) {
    CHECK_EQ(sxp("x = -a * --b;"),
             std::string("Program(Assignment[x](BinaryOp[*](UnaryOp[-](Identifier[a]) "
                         "UnaryOp[-](UnaryOp[-](Identifier[b])))))"));
    CHECK_EQ(sxp("x = -(a + b);"),
             std::string("Program(Assignment[x](UnaryOp[-](BinaryOp[+](Identifier[a] Identifier[b]))))"));
}

TEST(literals_and_types) {
    CHECK_EQ(sxp("float f = 3.14; int n; void v;"),
             std::string("Program(Declaration[float f](FloatLiteral[3.14]) Declaration[int n] "
                         "Declaration[void v])"));
}

TEST(if_else_chain) {
    CHECK_EQ(sxp("if (a < b) x = 1; else if (a == b) { x = 2; } else x = 3;"),
             std::string("Program(If(BinaryOp[<](Identifier[a] Identifier[b]) "
                         "Assignment[x](IntLiteral[1]) "
                         "If(BinaryOp[==](Identifier[a] Identifier[b]) Block(Assignment[x](IntLiteral[2])) "
                         "Assignment[x](IntLiteral[3]))))"));
    CHECK_EQ(sxp("if (a) { }"), std::string("Program(If(Identifier[a] Block))"));
}

TEST(while_loop) {
    CHECK_EQ(sxp("while (i != 0) { i = i - 1; }"),
             std::string("Program(While(BinaryOp[!=](Identifier[i] IntLiteral[0]) "
                         "Block(Assignment[i](BinaryOp[-](Identifier[i] IntLiteral[1])))))"));
}

TEST(for_all_parts) {
    CHECK_EQ(sxp("for (int i = 0; i < n; i = i + 1) s = s + i;"),
             std::string("Program(For(Declaration[int i](IntLiteral[0]) "
                         "BinaryOp[<](Identifier[i] Identifier[n]) "
                         "Assignment[i](BinaryOp[+](Identifier[i] IntLiteral[1])) "
                         "Assignment[s](BinaryOp[+](Identifier[s] Identifier[i]))))"));
    CHECK_EQ(sxp("for (i = 0; i >= 0; f(i)) {}"),
             std::string("Program(For(Assignment[i](IntLiteral[0]) BinaryOp[>=](Identifier[i] IntLiteral[0]) "
                         "Call[f](Identifier[i]) Block))"));
}

TEST(for_missing_parts) {
    auto prog = p("for (;;) {}");
    CHECK_EQ(sx(*prog), std::string("Program(For(Empty Empty Empty Block))"));
    const Node& f = *prog->children[0];
    CHECK_EQ(f.children.size(), size_t(4));
    // Empty placeholders are zero-width at the following token.
    CHECK_EQ(pos(*f.children[0]), std::string("1:6-1:6"));
    CHECK_EQ(pos(*f.children[1]), std::string("1:7-1:7"));
    CHECK_EQ(pos(*f.children[2]), std::string("1:8-1:8"));
    CHECK_EQ(sxp("for (int i; ; ) x = 1;"),
             std::string("Program(For(Declaration[int i] Empty Empty Assignment[x](IntLiteral[1])))"));
}

TEST(function_params_return_call) {
    CHECK_EQ(sxp("int add(int a, float b) { return a + b; }\nvoid main() { return; }\nint r = add(1, 2.5);"),
             std::string("Program(FunctionDecl[int add](Param[int a] Param[float b] "
                         "Block(Return(BinaryOp[+](Identifier[a] Identifier[b])))) "
                         "FunctionDecl[void main](Block(Return)) "
                         "Declaration[int r](Call[add](IntLiteral[1] FloatLiteral[2.5])))"));
}

TEST(expression_statement_call) {
    CHECK_EQ(sxp("print(x, y * 2); g(); 5;"),
             std::string("Program(ExprStmt(Call[print](Identifier[x] BinaryOp[*](Identifier[y] IntLiteral[2]))) "
                         "ExprStmt(Call[g]) ExprStmt(IntLiteral[5]))"));
    // a comparison is an expression statement, not an assignment
    CHECK_EQ(sxp("a == b;"), std::string("Program(ExprStmt(BinaryOp[==](Identifier[a] Identifier[b])))"));
}

TEST(nested_blocks) {
    CHECK_EQ(sxp("{ int a = 1; { { a = 2; } } {} }"),
             std::string("Program(Block(Declaration[int a](IntLiteral[1]) "
                         "Block(Block(Assignment[a](IntLiteral[2]))) Block))"));
}

TEST(positions) {
    auto prog = p("int result = a + b * c;\nif (x) {\n    y = -z;\n}\n");
    CHECK_EQ(pos(*prog), std::string("1:1-4:2"));
    const Node& decl = *prog->children[0];
    CHECK_EQ(pos(decl), std::string("1:1-1:24"));  // includes ';'
    const Node& plus = *decl.children[0];
    CHECK_EQ(pos(plus), std::string("1:14-1:23"));
    CHECK_EQ(pos(*plus.children[1]), std::string("1:18-1:23"));
    CHECK_EQ(pos(*plus.children[0]), std::string("1:14-1:15"));
    const Node& ifs = *prog->children[1];
    CHECK_EQ(pos(ifs), std::string("2:1-4:2"));
    const Node& block = *ifs.children[1];
    CHECK_EQ(pos(block), std::string("2:8-4:2"));
    const Node& assign = *block.children[0];
    CHECK_EQ(pos(assign), std::string("3:5-3:12"));
    CHECK_EQ(pos(*assign.children[0]), std::string("3:9-3:11"));
}

TEST(positions_parens_and_for_header) {
    auto prog = p("x = (a + b) * c;");
    const Node& mul = *prog->children[0]->children[0];
    CHECK_EQ(pos(mul), std::string("1:5-1:16"));               // starts at '('
    CHECK_EQ(pos(*mul.children[0]), std::string("1:6-1:11"));  // inner excludes parens

    auto loop = p("for (int i = 0; i < 10; i = i + 1) {}");
    const Node& f = *loop->children[0];
    CHECK_EQ(pos(f), std::string("1:1-1:38"));
    CHECK_EQ(pos(*f.children[0]), std::string("1:6-1:15"));   // no ';'
    CHECK_EQ(pos(*f.children[2]), std::string("1:25-1:34"));  // no ')'

    auto fn = p("int f(int a) {\n}");
    const Node& decl = *fn->children[0];
    CHECK_EQ(pos(decl), std::string("1:1-2:2"));
    CHECK_EQ(pos(*decl.children[0]), std::string("1:7-1:12"));
}

TEST(errors) {
    CHECK_EQ(err("int x = 5\nint y;"), std::string("2:1 expected ';' after declaration, found 'int'"));
    CHECK_EQ(err("x = (a + b;"), std::string("1:11 expected ')', found ';'"));
    CHECK_EQ(err("x = a + b);"), std::string("1:10 expected ';' after assignment, found ')'"));
    CHECK_EQ(err("f(a;"), std::string("1:4 expected ')' after arguments, found ';'"));
    CHECK_EQ(err("int main() { int g() { return 1; } }"),
             std::string("1:14 function definitions are only allowed at top level"));
    CHECK_EQ(err("int = 5;"), std::string("1:5 expected identifier after 'int', found '='"));
    CHECK_EQ(err("int x = "), std::string("1:9 expected expression, found end of input"));
    CHECK_EQ(err("while (x) {"), std::string("1:12 expected '}', found end of input"));
    CHECK_EQ(err("a < b < c;"), std::string("1:7 expected ';' after expression, found '<'"));
    CHECK_EQ(err("}"), std::string("1:1 expected statement, found '}'"));
    CHECK_EQ(err("if x) y = 1;"), std::string("1:4 expected '(' after 'if', found 'x'"));
    CHECK_EQ(err("x = 1 +;"), std::string("1:8 expected expression, found ';'"));
    CHECK_EQ(err("f(1)"), std::string("1:5 expected ';' after expression, found end of input"));
    CHECK_EQ(err("int f(int a,) {}"), std::string("1:13 expected parameter type, found ')'"));
    CHECK_EQ(err("int f() return 1;"), std::string("1:9 expected '{' to start function body, found 'return'"));
    CHECK_EQ(err("else x = 1;"), std::string("1:1 expected expression, found 'else'"));
}

RUN_TESTS()
