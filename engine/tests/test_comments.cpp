// Comment attachment unit tests on hand-built, positioned ASTs (no lexer/parser needed).
#include "../src/comments.h"
#include "check.h"

namespace {

// Node with position (end is exclusive).
NodePtr at(const std::string& kind, int l, int c, int el, int ec, std::vector<NodePtr> kids = {}) {
    auto n = makeNode(kind);
    n->line = l;
    n->column = c;
    n->endLine = el;
    n->endColumn = ec;
    for (auto& k : kids) n->children.push_back(std::move(k));
    return n;
}

template <typename... T>
std::vector<NodePtr> list(T&&... items) {
    std::vector<NodePtr> v;
    (v.push_back(std::move(items)), ...);
    return v;
}

Comment cm(const std::string& text, int l, int c) {
    // End only matters for completeness; single-line comments.
    return {text, l, c, l, c + static_cast<int>(text.size())};
}

using Strings = std::vector<std::string>;

std::string join(const Strings& v) {
    std::string s;
    for (const auto& x : v) s += "[" + x + "]";
    return s;
}

}  // namespace

// 1: int x = 1; // t
// 2: // lead
// 3: x = 2;
TEST(trailing_and_leading) {
    auto p = at("Program", 1, 1, 3, 7,
                list(at("Declaration", 1, 1, 1, 11, list(at("IntLiteral", 1, 9, 1, 10))),
                     at("Assignment", 3, 1, 3, 7, list(at("IntLiteral", 3, 5, 3, 6)))));
    attachComments(*p, {cm("// t", 1, 12), cm("// lead", 2, 1)});
    CHECK_EQ(p->children[0]->trailingComment, std::string("// t"));
    CHECK_EQ(join(p->children[0]->leadingComments), std::string(""));
    CHECK_EQ(join(p->children[1]->leadingComments), std::string("[// lead]"));
    CHECK(p->danglingComments.empty());
}

// 1: a = 1; /* x */ /* y */ b = 2;
TEST(several_trailing_joined) {
    auto p = at("Program", 1, 1, 1, 30,
                list(at("Assignment", 1, 1, 1, 7, list(at("IntLiteral", 1, 5, 1, 6))),
                     at("Assignment", 1, 24, 1, 30, list(at("IntLiteral", 1, 28, 1, 29)))));
    attachComments(*p, {cm("/* x */", 1, 8), cm("/* y */", 1, 16)});
    CHECK_EQ(p->children[0]->trailingComment, std::string("/* x */ /* y */"));
    CHECK(p->children[1]->leadingComments.empty());
}

// 1: {
// 2:     x = 3;
// 3:     // d
// 4: }
// 5: // eof
TEST(dangling_in_block_and_program) {
    auto p = at("Program", 1, 1, 4, 2,
                list(at("Block", 1, 1, 4, 2, list(at("Assignment", 2, 5, 2, 11, list(at("IntLiteral", 2, 9, 2, 10)))))));
    attachComments(*p, {cm("// d", 3, 5), cm("// eof", 5, 1)});
    CHECK_EQ(join(p->children[0]->danglingComments), std::string("[// d]"));
    CHECK_EQ(join(p->danglingComments), std::string("[// eof]"));
    CHECK(p->children[0]->children[0]->trailingComment.empty());
}

// 1: y = a + /* e */ b;
TEST(comment_inside_expression) {
    auto e = at("BinaryOp", 1, 5, 1, 17, list(at("Identifier", 1, 5, 1, 6), at("Identifier", 1, 16, 1, 17)));
    auto p = at("Program", 1, 1, 1, 18, list(at("Assignment", 1, 1, 1, 18, list(std::move(e)))));
    attachComments(*p, {cm("/* e */", 1, 9)});
    CHECK_EQ(join(p->children[0]->leadingComments), std::string("[/* e */]"));
    CHECK(p->children[0]->trailingComment.empty());
}

// 1: int f(int a) { // first
// 2:     return a;
// 3: } // after f
// 4: int g(/* p */) {
// 5: }
TEST(function_comments) {
    auto f = at("FunctionDecl", 1, 1, 3, 2,
                list(at("Param", 1, 7, 1, 12),
                     at("Block", 1, 14, 3, 2, list(at("Return", 2, 5, 2, 14, list(at("Identifier", 2, 12, 2, 13)))))));
    auto g = at("FunctionDecl", 4, 1, 5, 2, list(at("Block", 4, 16, 5, 2)));
    auto p = at("Program", 1, 1, 5, 2, list(std::move(f), std::move(g)));
    attachComments(*p, {cm("// first", 1, 16), cm("// after f", 3, 3), cm("/* p */", 4, 7)});
    const Node& body = *p->children[0]->children[1];
    CHECK_EQ(join(body.children[0]->leadingComments), std::string("[// first]"));
    CHECK_EQ(p->children[0]->trailingComment, std::string("// after f"));
    CHECK_EQ(join(p->children[1]->leadingComments), std::string("[/* p */]"));
    CHECK(p->children[1]->children[0]->danglingComments.empty());
}

// 1: if (x) a = 1; // t
// 2: else /* h */ b = 2; // end
TEST(if_non_block_bodies) {
    auto i = at("If", 1, 1, 2, 19,
                list(at("Identifier", 1, 5, 1, 6), at("Assignment", 1, 8, 1, 14, list(at("IntLiteral", 1, 12, 1, 13))),
                     at("Assignment", 2, 13, 2, 19, list(at("IntLiteral", 2, 17, 2, 18)))));
    auto p = at("Program", 1, 1, 2, 19, list(std::move(i)));
    attachComments(*p, {cm("// t", 1, 15), cm("/* h */", 2, 6), cm("// end", 2, 20)});
    const Node& s = *p->children[0];
    CHECK_EQ(s.children[1]->trailingComment, std::string("// t"));
    CHECK_EQ(join(s.leadingComments), std::string("[/* h */]"));
    CHECK_EQ(s.trailingComment, std::string("// end"));
}

// 1: while (x) {
// 2:     // only
// 3: }
// 4: for (i = 0; /* c */ i < 3; i = i + 1) x = i;
TEST(while_dangling_and_for_header) {
    auto w = at("While", 1, 1, 3, 2, list(at("Identifier", 1, 8, 1, 9), at("Block", 1, 11, 3, 2)));
    auto f = at("For", 4, 1, 4, 46,
                list(at("Assignment", 4, 6, 4, 11, list(at("IntLiteral", 4, 10, 4, 11))),
                     at("BinaryOp", 4, 21, 4, 26, list(at("Identifier", 4, 21, 4, 22), at("IntLiteral", 4, 25, 4, 26))),
                     at("Assignment", 4, 28, 4, 37,
                        list(at("BinaryOp", 4, 32, 4, 37,
                                list(at("Identifier", 4, 32, 4, 33), at("IntLiteral", 4, 36, 4, 37))))),
                     at("Assignment", 4, 39, 4, 46, list(at("Identifier", 4, 43, 4, 44)))));
    auto p = at("Program", 1, 1, 4, 46, list(std::move(w), std::move(f)));
    attachComments(*p, {cm("// only", 2, 5), cm("/* c */", 4, 13)});
    CHECK_EQ(join(p->children[0]->children[1]->danglingComments), std::string("[// only]"));
    CHECK_EQ(join(p->children[1]->leadingComments), std::string("[/* c */]"));
    CHECK(p->children[1]->children[0]->leadingComments.empty());
}

// 1: /* a */
// 2: {   // b
// 3:     { x = 1; } // c
// 4: }
TEST(innermost_block_level) {
    auto inner = at("Block", 3, 5, 3, 15, list(at("Assignment", 3, 7, 3, 13, list(at("IntLiteral", 3, 11, 3, 12)))));
    auto outer = at("Block", 2, 1, 4, 2, list(std::move(inner)));
    auto p = at("Program", 2, 1, 4, 2, list(std::move(outer)));
    attachComments(*p, {cm("/* a */", 1, 1), cm("// b", 2, 5), cm("// c", 3, 16)});
    const Node& o = *p->children[0];
    CHECK_EQ(join(o.leadingComments), std::string("[/* a */]"));
    CHECK_EQ(join(o.children[0]->leadingComments), std::string("[// b]"));
    CHECK_EQ(o.children[0]->trailingComment, std::string("// c"));
}

RUN_TESTS()
