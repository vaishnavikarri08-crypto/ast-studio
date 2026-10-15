// astc: reads source on stdin, writes the engine JSON (docs/contract.md §5) on stdout.
#include <iostream>
#include <iterator>
#include <string>

#include "comments.h"
#include "json.h"
#include "lexer.h"
#include "parser.h"
#include "printer.h"

int main() {
    std::string source((std::istreambuf_iterator<char>(std::cin)), std::istreambuf_iterator<char>());

    try {
        LexResult lexed = lex(source);
        if (lexed.error) {
            std::cout << responseToJson(lexed, nullptr, nullptr, {*lexed.error}) << "\n";
            return 0;
        }
        NodePtr ast;
        try {
            ast = parse(lexed.tokens);
        } catch (const CompileException& e) {
            std::cout << responseToJson(lexed, nullptr, nullptr, {e.error}) << "\n";
            return 0;
        }
        attachComments(*ast, lexed.comments);
        std::string formatted = prettyPrint(*ast);
        std::cout << responseToJson(lexed, ast.get(), &formatted, {}) << "\n";
        return 0;
    } catch (const std::exception& e) {
        std::cerr << "astc internal error: " << e.what() << "\n";
        return 2;
    }
}
