// Golden + round-trip tests over samples/engine (needs the real lexer and parser).
// For each samples/engine/*.c (not *.formatted.c, not errors/):
//   pretty(x) == x.formatted.c, parse(pretty(x)) == normalise(x), pretty is idempotent.
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>

#include "../src/comments.h"
#include "../src/lexer.h"
#include "../src/parser.h"
#include "../src/printer.h"
#include "check.h"

namespace fs = std::filesystem;

namespace {

std::string readFile(const fs::path& p) {
    std::ifstream in(p, std::ios::binary);
    std::ostringstream s;
    s << in.rdbuf();
    return s.str();
}

bool endsWith(const std::string& s, const std::string& suffix) {
    return s.size() >= suffix.size() && s.compare(s.size() - suffix.size(), suffix.size(), suffix) == 0;
}

// lex -> parse -> attachComments. Returns nullptr (and reports) on any error.
NodePtr build(const std::string& source, const std::string& what) {
    LexResult lexed = lex(source);
    if (lexed.error) {
        ++check::failures;
        std::cerr << what << ": lexer error: " << lexed.error->message << " at " << lexed.error->line << ":"
                  << lexed.error->column << "\n";
        return nullptr;
    }
    try {
        NodePtr ast = parse(lexed.tokens);
        attachComments(*ast, lexed.comments);
        return ast;
    } catch (const CompileException& e) {
        ++check::failures;
        std::cerr << what << ": parser error: " << e.error.message << " at " << e.error.line << ":"
                  << e.error.column << "\n";
        return nullptr;
    }
}

// Contract §4 normalisation: every non-block body of If/While/For is wrapped in a Block.
void normalise(Node& n) {
    size_t from = n.children.size();
    if (n.kind == "If" || n.kind == "While") from = 1;
    else if (n.kind == "For") from = 3;
    for (size_t i = from; i < n.children.size(); ++i) {
        if (n.children[i]->kind != "Block") {
            auto block = makeNode("Block");
            block->children.push_back(std::move(n.children[i]));
            n.children[i] = std::move(block);
        }
    }
    for (auto& c : n.children) normalise(*c);
}

std::vector<fs::path> samples() {
    std::vector<fs::path> out;
    for (const auto& entry : fs::directory_iterator(SAMPLES_DIR)) {
        std::string name = entry.path().filename().string();
        if (entry.is_regular_file() && endsWith(name, ".c") && !endsWith(name, ".formatted.c")) {
            out.push_back(entry.path());
        }
    }
    std::sort(out.begin(), out.end());
    return out;
}

}  // namespace

TEST(samples_exist) { CHECK(samples().size() >= 4); }

TEST(golden_and_roundtrip) {
    for (const auto& path : samples()) {
        std::string name = path.filename().string();
        fs::path golden = path;
        golden.replace_extension(".formatted.c");
        if (!fs::exists(golden)) {
            ++check::failures;
            std::cerr << name << ": missing " << golden.filename() << "\n";
            continue;
        }

        NodePtr original = build(readFile(path), name);
        if (!original) continue;
        std::string pretty = prettyPrint(*original);
        if (pretty != readFile(golden)) {
            ++check::failures;
            std::cerr << name << ": output differs from golden\n--- got ---\n" << pretty << "--- end ---\n";
        }

        NodePtr reparsed = build(pretty, name + " (formatted)");
        if (!reparsed) continue;
        // Idempotence, checked before normalise() rewrites the trees.
        std::string again = prettyPrint(*reparsed);
        if (again != pretty) {
            ++check::failures;
            std::cerr << name << ": pretty printing is not idempotent\n--- second ---\n" << again << "--- end ---\n";
        }
        normalise(*original);
        normalise(*reparsed);
        if (!astEqual(*original, *reparsed)) {
            ++check::failures;
            std::cerr << name << ": parse(pretty(ast)) != ast\n--- original ---\n"
                      << astDump(*original) << "--- reparsed ---\n" << astDump(*reparsed);
        }
        std::cout << "  checked " << name << "\n";
    }
}

TEST(golden_files_are_fixed_points) {
    // The goldens themselves must format to themselves.
    for (const auto& path : samples()) {
        fs::path golden = path;
        golden.replace_extension(".formatted.c");
        if (!fs::exists(golden)) continue;
        std::string text = readFile(golden);
        NodePtr ast = build(text, golden.filename().string());
        if (ast) CHECK_EQ(prettyPrint(*ast), text);
    }
}

TEST(missing_semicolon_is_parser_error) {
    std::string source = readFile(fs::path(SAMPLES_DIR) / "errors" / "missing_semicolon.c");
    CHECK(!source.empty());
    LexResult lexed = lex(source);
    CHECK(!lexed.error);
    if (lexed.error) return;
    bool threw = false;
    try {
        parse(lexed.tokens);
    } catch (const CompileException& e) {
        threw = true;
        CHECK_EQ(e.error.stage, std::string("parser"));
        CHECK(e.error.message.rfind("expected ", 0) == 0);
    }
    CHECK(threw);
}

RUN_TESTS()
