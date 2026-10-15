// Pretty printer unit tests on hand-built ASTs (no lexer/parser needed).
#include "../src/printer.h"
#include "check.h"

namespace {

NodePtr id(const std::string& name) {
    auto n = makeNode("Identifier");
    n->name = name;
    return n;
}

NodePtr lit(const std::string& v) {
    auto n = makeNode(v.find('.') == std::string::npos ? "IntLiteral" : "FloatLiteral");
    n->value = v;
    return n;
}

NodePtr bin(const std::string& op, NodePtr l, NodePtr r) {
    auto n = makeNode("BinaryOp");
    n->op = op;
    n->children.push_back(std::move(l));
    n->children.push_back(std::move(r));
    return n;
}

NodePtr neg(NodePtr e) {
    auto n = makeNode("UnaryOp");
    n->op = "-";
    n->children.push_back(std::move(e));
    return n;
}

NodePtr call(const std::string& name, std::vector<NodePtr> args) {
    auto n = makeNode("Call");
    n->name = name;
    for (auto& a : args) n->children.push_back(std::move(a));
    return n;
}

NodePtr decl(const std::string& type, const std::string& name, NodePtr init = nullptr) {
    auto n = makeNode("Declaration");
    n->varType = type;
    n->name = name;
    if (init) n->children.push_back(std::move(init));
    return n;
}

NodePtr assign(const std::string& name, NodePtr value) {
    auto n = makeNode("Assignment");
    n->name = name;
    n->children.push_back(std::move(value));
    return n;
}

NodePtr exprStmt(NodePtr e) {
    auto n = makeNode("ExprStmt");
    n->children.push_back(std::move(e));
    return n;
}

NodePtr ret(NodePtr e = nullptr) {
    auto n = makeNode("Return");
    if (e) n->children.push_back(std::move(e));
    return n;
}

NodePtr block(std::vector<NodePtr> stmts) {
    auto n = makeNode("Block");
    for (auto& s : stmts) n->children.push_back(std::move(s));
    return n;
}

NodePtr withKids(const std::string& kind, std::vector<NodePtr> kids) {
    auto n = makeNode(kind);
    for (auto& k : kids) n->children.push_back(std::move(k));
    return n;
}

template <typename... T>
std::vector<NodePtr> list(T&&... items) {
    std::vector<NodePtr> v;
    (v.push_back(std::move(items)), ...);
    return v;
}

NodePtr program(std::vector<NodePtr> stmts) { return withKids("Program", std::move(stmts)); }

// Prints a single expression via `x = <e>;`.
std::string printExpr(NodePtr e) {
    std::string s = prettyPrint(*program(list(assign("x", std::move(e)))));
    return s.substr(4, s.size() - 6);  // strip "x = " and ";\n"
}

}  // namespace

TEST(simple_statements) {
    auto p = program(list(decl("int", "a"), decl("float", "b", lit("1.5")), assign("a", lit("2")),
                          exprStmt(call("f", list(id("a"), lit("3")))), ret(), ret(id("a"))));
    CHECK_EQ(prettyPrint(*p),
             std::string("int a;\nfloat b = 1.5;\na = 2;\nf(a, 3);\nreturn;\nreturn a;\n"));
}

TEST(empty_program) { CHECK_EQ(prettyPrint(*program({})), std::string("")); }

TEST(precedence_minimal_parens) {
    CHECK_EQ(printExpr(bin("+", id("a"), bin("*", id("b"), id("c")))), std::string("a + b * c"));
    CHECK_EQ(printExpr(bin("*", bin("+", id("a"), id("b")), id("c"))), std::string("(a + b) * c"));
    CHECK_EQ(printExpr(bin("-", id("a"), bin("-", id("b"), id("c")))), std::string("a - (b - c)"));
    CHECK_EQ(printExpr(bin("-", bin("-", id("a"), id("b")), id("c"))), std::string("a - b - c"));
    CHECK_EQ(printExpr(bin("/", id("a"), bin("*", id("b"), id("c")))), std::string("a / (b * c)"));
    CHECK_EQ(printExpr(bin("+", id("a"), bin("+", id("b"), id("c")))), std::string("a + (b + c)"));
    CHECK_EQ(printExpr(bin("<", bin("+", id("a"), lit("1")), bin("*", id("b"), lit("2")))),
             std::string("a + 1 < b * 2"));
}

TEST(comparison_non_associative) {
    CHECK_EQ(printExpr(bin("==", bin("<", id("a"), id("b")), id("c"))), std::string("(a < b) == c"));
    CHECK_EQ(printExpr(bin("==", id("a"), bin("<", id("b"), id("c")))), std::string("a == (b < c)"));
    CHECK_EQ(printExpr(bin("*", bin("<", id("a"), id("b")), id("c"))), std::string("(a < b) * c"));
}

TEST(unary_minus) {
    CHECK_EQ(printExpr(neg(id("x"))), std::string("-x"));
    CHECK_EQ(printExpr(neg(bin("+", id("a"), id("b")))), std::string("-(a + b)"));
    CHECK_EQ(printExpr(neg(neg(id("x")))), std::string("--x"));
    CHECK_EQ(printExpr(bin("-", id("a"), neg(id("b")))), std::string("a - -b"));
    CHECK_EQ(printExpr(bin("*", neg(id("a")), id("b"))), std::string("-a * b"));
    CHECK_EQ(printExpr(neg(call("f", list(bin("+", id("a"), id("b")))))), std::string("-f(a + b)"));
}

TEST(if_else_chain_and_wrapping) {
    auto inner = withKids("If", list(id("b"), assign("x", lit("2")), assign("x", lit("3"))));
    auto outer = withKids("If", list(id("a"), block(list(assign("x", lit("1")))), std::move(inner)));
    CHECK_EQ(prettyPrint(*program(list(std::move(outer)))),
             std::string("if (a) {\n    x = 1;\n} else if (b) {\n    x = 2;\n} else {\n    x = 3;\n}\n"));
}

TEST(else_if_with_comments_is_not_chained) {
    auto inner = withKids("If", list(id("b"), block({})));
    inner->leadingComments.push_back("// c");
    auto outer = withKids("If", list(id("a"), block({}), std::move(inner)));
    CHECK_EQ(prettyPrint(*program(list(std::move(outer)))),
             std::string("if (a) {\n} else {\n    // c\n    if (b) {\n    }\n}\n"));
}

TEST(while_and_for) {
    auto w = withKids("While", list(bin("<", id("i"), id("n")), assign("i", bin("+", id("i"), lit("1")))));
    auto f = withKids("For", list(decl("int", "i", lit("0")), bin("<", id("i"), lit("10")),
                                  assign("i", bin("+", id("i"), lit("1"))),
                                  block(list(exprStmt(call("g", list(id("i"))))))));
    CHECK_EQ(prettyPrint(*program(list(std::move(w), std::move(f)))),
             std::string("while (i < n) {\n    i = i + 1;\n}\n"
                         "for (int i = 0; i < 10; i = i + 1) {\n    g(i);\n}\n"));
}

TEST(for_with_empty_parts) {
    auto f1 = withKids("For", list(makeNode("Empty"), makeNode("Empty"), makeNode("Empty"), block({})));
    auto f2 = withKids("For", list(assign("i", lit("0")), makeNode("Empty"), call("step", {}), block({})));
    auto f3 = withKids("For", list(makeNode("Empty"), id("ok"), makeNode("Empty"), block({})));
    CHECK_EQ(prettyPrint(*program(list(std::move(f1), std::move(f2), std::move(f3)))),
             std::string("for (;;) {\n}\nfor (i = 0;; step()) {\n}\nfor (; ok;) {\n}\n"));
}

TEST(functions_and_blank_lines) {
    auto param = [](const std::string& t, const std::string& n) {
        auto p = makeNode("Param");
        p->varType = t;
        p->name = n;
        return p;
    };
    auto f = withKids("FunctionDecl", list(param("int", "a"), param("float", "b"), block(list(ret(id("a"))))));
    f->varType = "int";
    f->name = "f";
    auto m = withKids("FunctionDecl", list(block({})));
    m->varType = "void";
    m->name = "main";
    auto p = program(list(decl("int", "g"), decl("int", "h"), std::move(f), std::move(m), assign("g", lit("1"))));
    CHECK_EQ(prettyPrint(*p), std::string("int g;\nint h;\n\nint f(int a, float b) {\n    return a;\n}\n\n"
                                          "void main() {\n}\n\ng = 1;\n"));
}

TEST(nested_blocks_indent) {
    auto p = program(list(block(list(block(list(assign("a", lit("1"))))))));
    CHECK_EQ(prettyPrint(*p), std::string("{\n    {\n        a = 1;\n    }\n}\n"));
}

TEST(comments_printed) {
    auto a = assign("a", lit("1"));
    a->leadingComments = {"// one", "/* two */"};
    a->trailingComment = "// t";
    auto b = block(list(decl("int", "z")));
    b->danglingComments = {"// inside"};
    auto w = withKids("While", list(id("x"), std::move(b)));
    w->trailingComment = "/* after */";
    auto p = program(list(std::move(a), std::move(w)));
    p->danglingComments = {"// eof"};
    CHECK_EQ(prettyPrint(*p), std::string("// one\n/* two */\na = 1; // t\nwhile (x) {\n    int z;\n"
                                          "    // inside\n} /* after */\n// eof\n"));
}

RUN_TESTS()
