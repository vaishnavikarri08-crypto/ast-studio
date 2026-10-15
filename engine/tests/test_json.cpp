// JSON serializer tests. Member 3.
#include "check.h"
#include "json.h"

TEST(escape_basic) {
    CHECK_EQ(jsonEscape("abc"), std::string("\"abc\""));
    CHECK_EQ(jsonEscape(""), std::string("\"\""));
    CHECK_EQ(jsonEscape("say \"hi\""), std::string("\"say \\\"hi\\\"\""));
    CHECK_EQ(jsonEscape("a\\b"), std::string("\"a\\\\b\""));
    CHECK_EQ(jsonEscape("x\ny\rz\tw"), std::string("\"x\\ny\\rz\\tw\""));
}

TEST(escape_control_chars) {
    CHECK_EQ(jsonEscape(std::string("\x01", 1)), std::string("\"\\u0001\""));
    CHECK_EQ(jsonEscape(std::string("\x1f", 1)), std::string("\"\\u001f\""));
    CHECK_EQ(jsonEscape(std::string("a\0b", 3)), std::string("\"a\\u0000b\""));
    CHECK_EQ(jsonEscape("\b\f"), std::string("\"\\u0008\\u000c\""));
    CHECK_EQ(jsonEscape("/* é */"), std::string("\"/* é */\""));  // UTF-8 passes through
}

TEST(node_minimal) {
    auto n = makeNode("Empty");
    n->line = 1; n->column = 2; n->endLine = 3; n->endColumn = 4;
    CHECK_EQ(astToJson(*n),
             std::string("{\"kind\":\"Empty\",\"line\":1,\"column\":2,\"endLine\":3,\"endColumn\":4,\"children\":[]}"));
}

TEST(node_key_order_and_children) {
    auto decl = makeNode("Declaration");
    decl->name = "x";
    decl->varType = "int";
    decl->line = decl->column = decl->endLine = 1;
    decl->endColumn = 11;
    decl->leadingComments = {"// a", "/* \"b\" */"};
    decl->trailingComment = "// t";
    auto lit = makeNode("IntLiteral");
    lit->value = "1";
    lit->line = lit->endLine = 1;
    lit->column = 9;
    lit->endColumn = 10;
    decl->children.push_back(std::move(lit));
    auto neg = makeNode("UnaryOp");
    neg->op = "-";
    decl->children.push_back(std::move(neg));

    CHECK_EQ(astToJson(*decl),
             std::string("{\"kind\":\"Declaration\",\"name\":\"x\",\"varType\":\"int\","
                         "\"line\":1,\"column\":1,\"endLine\":1,\"endColumn\":11,"
                         "\"leadingComments\":[\"// a\",\"/* \\\"b\\\" */\"],\"trailingComment\":\"// t\","
                         "\"children\":[{\"kind\":\"IntLiteral\",\"value\":\"1\",\"line\":1,\"column\":9,"
                         "\"endLine\":1,\"endColumn\":10,\"children\":[]},"
                         "{\"kind\":\"UnaryOp\",\"op\":\"-\",\"line\":0,\"column\":0,\"endLine\":0,"
                         "\"endColumn\":0,\"children\":[]}]}"));
}

TEST(node_all_optional_fields_order) {
    auto n = makeNode("K");
    n->op = "o"; n->name = "n"; n->value = "v"; n->varType = "t";
    n->leadingComments = {"l"}; n->trailingComment = "tr"; n->danglingComments = {"d1", "d2"};
    CHECK_EQ(astToJson(*n),
             std::string("{\"kind\":\"K\",\"op\":\"o\",\"name\":\"n\",\"value\":\"v\",\"varType\":\"t\","
                         "\"line\":0,\"column\":0,\"endLine\":0,\"endColumn\":0,"
                         "\"leadingComments\":[\"l\"],\"trailingComment\":\"tr\","
                         "\"danglingComments\":[\"d1\",\"d2\"],\"children\":[]}"));
}

TEST(response_success) {
    LexResult lexed;
    lexed.tokens = {{TokenType::IDENTIFIER, "x", 1, 1}, {TokenType::SEMICOLON, ";", 1, 2},
                    {TokenType::END_OF_FILE, "", 1, 3}};
    lexed.comments = {{"// hi", 1, 4, 1, 9}};
    auto prog = makeNode("Program");
    prog->line = prog->column = prog->endLine = 1;
    prog->endColumn = 3;
    std::string formatted = "x;\n";
    CHECK_EQ(responseToJson(lexed, prog.get(), &formatted, {}),
             std::string("{\"success\":true,"
                         "\"tokens\":[{\"type\":\"IDENTIFIER\",\"lexeme\":\"x\",\"line\":1,\"column\":1},"
                         "{\"type\":\"SEMICOLON\",\"lexeme\":\";\",\"line\":1,\"column\":2},"
                         "{\"type\":\"END_OF_FILE\",\"lexeme\":\"\",\"line\":1,\"column\":3}],"
                         "\"comments\":[{\"text\":\"// hi\",\"line\":1,\"column\":4}],"
                         "\"ast\":{\"kind\":\"Program\",\"line\":1,\"column\":1,\"endLine\":1,\"endColumn\":3,"
                         "\"children\":[]},"
                         "\"formatted\":\"x;\\n\",\"errors\":[]}"));
}

TEST(response_error) {
    LexResult lexed;
    lexed.tokens = {{TokenType::KEYWORD, "int", 1, 1}};
    CompileError e{"parser", "expected identifier after 'int', found '='", 1, 5};
    CHECK_EQ(responseToJson(lexed, nullptr, nullptr, {e}),
             std::string("{\"success\":false,"
                         "\"tokens\":[{\"type\":\"KEYWORD\",\"lexeme\":\"int\",\"line\":1,\"column\":1}],"
                         "\"comments\":[],\"ast\":null,\"formatted\":null,"
                         "\"errors\":[{\"stage\":\"parser\",\"message\":\"expected identifier after 'int', "
                         "found '='\",\"line\":1,\"column\":5}]}"));
}

TEST(response_success_requires_ast) {
    LexResult lexed;
    std::string r = responseToJson(lexed, nullptr, nullptr, {});
    CHECK(r.rfind("{\"success\":false,", 0) == 0);
}

RUN_TESTS()
