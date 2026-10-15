// Recursive-descent parser. Member 3. See docs/contract.md §1 and §3.
#include "parser.h"

namespace {

class Parser {
public:
    explicit Parser(const std::vector<Token>& tokens) : tokens_(tokens) {
        eof_ = {TokenType::END_OF_FILE, "", 1, 1};
        if (!tokens_.empty()) {
            eof_.line = tokens_.back().line;
            eof_.column = tokens_.back().column;
        }
    }

    NodePtr parseProgram() {
        auto program = makeNode("Program");
        program->line = program->column = program->endLine = program->endColumn = 1;
        if (!atEnd()) start(*program, peek());
        while (!atEnd()) {
            if (isFunctionStart()) {
                program->children.push_back(parseFunction());
            } else {
                program->children.push_back(parseStatement());
            }
        }
        if (prev_) finish(*program);
        return program;
    }

private:
    const std::vector<Token>& tokens_;
    Token eof_;
    size_t pos_ = 0;
    const Token* prev_ = nullptr;  // last consumed token

    // ---- token helpers ----

    const Token& peek(size_t ahead = 0) const {
        size_t i = pos_ + ahead;
        return i < tokens_.size() ? tokens_[i] : eof_;
    }
    bool atEnd() const { return peek().type == TokenType::END_OF_FILE; }
    bool check(TokenType type, size_t ahead = 0) const { return peek(ahead).type == type; }
    bool checkKeyword(const char* kw, size_t ahead = 0) const {
        const Token& t = peek(ahead);
        return t.type == TokenType::KEYWORD && t.lexeme == kw;
    }
    bool isType(size_t ahead = 0) const {
        return checkKeyword("int", ahead) || checkKeyword("float", ahead) || checkKeyword("void", ahead);
    }
    bool isFunctionStart() const {
        return isType() && check(TokenType::IDENTIFIER, 1) && check(TokenType::LPAREN, 2);
    }

    const Token& advance() {
        const Token& t = peek();
        if (pos_ < tokens_.size()) ++pos_;
        prev_ = &t;
        return t;
    }

    [[noreturn]] void fail(const std::string& message, const Token& at) const {
        throw CompileException(CompileError{"parser", message, at.line, at.column});
    }
    [[noreturn]] void error(const std::string& what) const {
        const Token& t = peek();
        std::string found = t.type == TokenType::END_OF_FILE ? "end of input" : "'" + t.lexeme + "'";
        fail("expected " + what + ", found " + found, t);
    }

    const Token& expect(TokenType type, const std::string& what) {
        if (!check(type)) error(what);
        return advance();
    }

    // ---- positions ----

    static void start(Node& n, const Token& t) {
        n.line = t.line;
        n.column = t.column;
    }
    void finish(Node& n) const {
        n.endLine = prev_->line;
        n.endColumn = prev_->column + static_cast<int>(prev_->lexeme.size());
    }
    NodePtr startNode(const std::string& kind) const {
        auto n = makeNode(kind);
        start(*n, peek());
        return n;
    }
    // Zero-width placeholder at the current token (For parts that are missing).
    NodePtr emptyNode() const {
        auto n = startNode("Empty");
        n->endLine = n->line;
        n->endColumn = n->column;
        return n;
    }

    // ---- declarations ----

    NodePtr parseFunction() {
        auto fn = startNode("FunctionDecl");
        fn->varType = advance().lexeme;
        fn->name = advance().lexeme;
        advance();  // (
        if (!check(TokenType::RPAREN)) {
            do {
                auto param = startNode("Param");
                if (!isType()) error("parameter type");
                param->varType = advance().lexeme;
                param->name = expect(TokenType::IDENTIFIER, "parameter name").lexeme;
                finish(*param);
                fn->children.push_back(std::move(param));
            } while (check(TokenType::COMMA) && (advance(), true));
        }
        expect(TokenType::RPAREN, "')' after parameters");
        if (!check(TokenType::LBRACE)) error("'{' to start function body");
        fn->children.push_back(parseBlock());
        finish(*fn);
        return fn;
    }

    // type IDENT [= expr], without the ';'.
    NodePtr parseDeclarationCore() {
        auto decl = startNode("Declaration");
        const Token& type = advance();
        decl->varType = type.lexeme;
        decl->name = expect(TokenType::IDENTIFIER, "identifier after '" + type.lexeme + "'").lexeme;
        if (check(TokenType::ASSIGNMENT_OPERATOR)) {
            advance();
            decl->children.push_back(parseExpression());
        }
        finish(*decl);
        return decl;
    }

    // IDENT = expr, without the ';'.
    NodePtr parseAssignmentCore() {
        auto assign = startNode("Assignment");
        assign->name = advance().lexeme;
        advance();  // =
        assign->children.push_back(parseExpression());
        finish(*assign);
        return assign;
    }

    bool isAssignmentStart() const {
        return check(TokenType::IDENTIFIER) && check(TokenType::ASSIGNMENT_OPERATOR, 1);
    }

    // ---- statements ----

    NodePtr parseStatement() {
        if (isFunctionStart()) fail("function definitions are only allowed at top level", peek());
        if (isType()) {
            auto decl = parseDeclarationCore();
            expect(TokenType::SEMICOLON, "';' after declaration");
            finish(*decl);
            return decl;
        }
        if (isAssignmentStart()) {
            auto assign = parseAssignmentCore();
            expect(TokenType::SEMICOLON, "';' after assignment");
            finish(*assign);
            return assign;
        }
        if (checkKeyword("if")) return parseIf();
        if (checkKeyword("while")) return parseWhile();
        if (checkKeyword("for")) return parseFor();
        if (checkKeyword("return")) return parseReturn();
        if (check(TokenType::LBRACE)) return parseBlock();

        auto stmt = startNode("ExprStmt");
        if (check(TokenType::RBRACE)) error("statement");  // stray '}' at top level
        stmt->children.push_back(parseExpression());
        expect(TokenType::SEMICOLON, "';' after expression");
        finish(*stmt);
        return stmt;
    }

    NodePtr parseBlock() {
        auto block = startNode("Block");
        advance();  // {
        while (!check(TokenType::RBRACE)) {
            if (atEnd()) error("'}'");
            block->children.push_back(parseStatement());
        }
        advance();  // }
        finish(*block);
        return block;
    }

    NodePtr parseCondition(const std::string& keyword) {
        expect(TokenType::LPAREN, "'(' after '" + keyword + "'");
        auto cond = parseExpression();
        expect(TokenType::RPAREN, "')' after condition");
        return cond;
    }

    NodePtr parseIf() {
        auto node = startNode("If");
        advance();  // if
        node->children.push_back(parseCondition("if"));
        node->children.push_back(parseStatement());
        if (checkKeyword("else")) {
            advance();
            node->children.push_back(parseStatement());
        }
        finish(*node);
        return node;
    }

    NodePtr parseWhile() {
        auto node = startNode("While");
        advance();  // while
        node->children.push_back(parseCondition("while"));
        node->children.push_back(parseStatement());
        finish(*node);
        return node;
    }

    NodePtr parseFor() {
        auto node = startNode("For");
        advance();  // for
        expect(TokenType::LPAREN, "'(' after 'for'");

        if (check(TokenType::SEMICOLON)) {
            node->children.push_back(emptyNode());
        } else if (isType()) {
            node->children.push_back(parseDeclarationCore());
        } else if (isAssignmentStart()) {
            node->children.push_back(parseAssignmentCore());
        } else {
            error("declaration, assignment or ';' in for initializer");
        }
        expect(TokenType::SEMICOLON, "';' after for initializer");

        if (check(TokenType::SEMICOLON)) {
            node->children.push_back(emptyNode());
        } else {
            node->children.push_back(parseExpression());
        }
        expect(TokenType::SEMICOLON, "';' after for condition");

        if (check(TokenType::RPAREN)) {
            node->children.push_back(emptyNode());
        } else if (isAssignmentStart()) {
            node->children.push_back(parseAssignmentCore());
        } else {
            node->children.push_back(parseExpression());
        }
        expect(TokenType::RPAREN, "')' after for clauses");

        node->children.push_back(parseStatement());
        finish(*node);
        return node;
    }

    NodePtr parseReturn() {
        auto node = startNode("Return");
        advance();  // return
        if (!check(TokenType::SEMICOLON)) node->children.push_back(parseExpression());
        expect(TokenType::SEMICOLON, "';' after return");
        finish(*node);
        return node;
    }

    // ---- expressions ----

    NodePtr parseExpression() { return parseComparison(); }

    static bool isComparison(TokenType t) {
        return t == TokenType::LT || t == TokenType::GT || t == TokenType::LE || t == TokenType::GE ||
               t == TokenType::EQ || t == TokenType::NE;
    }

    // Spans from the first token of the left operand (which may be a '(')
    // to the last consumed token (which may be a ')').
    NodePtr binary(const Token& first, NodePtr left, const Token& op, NodePtr right) const {
        auto node = makeNode("BinaryOp");
        node->op = op.lexeme;
        start(*node, first);
        finish(*node);
        node->children.push_back(std::move(left));
        node->children.push_back(std::move(right));
        return node;
    }

    NodePtr parseComparison() {
        const Token& first = peek();
        auto left = parseAdditive();
        if (isComparison(peek().type)) {  // non-associative: at most one
            const Token& op = advance();
            left = binary(first, std::move(left), op, parseAdditive());
        }
        return left;
    }

    NodePtr parseAdditive() {
        const Token& first = peek();
        auto left = parseTerm();
        while (check(TokenType::PLUS) || check(TokenType::MINUS)) {
            const Token& op = advance();
            left = binary(first, std::move(left), op, parseTerm());
        }
        return left;
    }

    NodePtr parseTerm() {
        const Token& first = peek();
        auto left = parseUnary();
        while (check(TokenType::MULTIPLY) || check(TokenType::DIVIDE)) {
            const Token& op = advance();
            left = binary(first, std::move(left), op, parseUnary());
        }
        return left;
    }

    NodePtr parseUnary() {
        if (check(TokenType::MINUS)) {
            auto node = startNode("UnaryOp");
            node->op = advance().lexeme;
            node->children.push_back(parseUnary());
            finish(*node);
            return node;
        }
        return parsePrimary();
    }

    NodePtr parsePrimary() {
        const Token& t = peek();
        if (t.type == TokenType::INT_LITERAL || t.type == TokenType::FLOAT_LITERAL) {
            auto node = startNode(t.type == TokenType::INT_LITERAL ? "IntLiteral" : "FloatLiteral");
            node->value = advance().lexeme;
            finish(*node);
            return node;
        }
        if (t.type == TokenType::IDENTIFIER) {
            if (check(TokenType::LPAREN, 1)) {
                auto call = startNode("Call");
                call->name = advance().lexeme;
                advance();  // (
                if (!check(TokenType::RPAREN)) {
                    do {
                        call->children.push_back(parseExpression());
                    } while (check(TokenType::COMMA) && (advance(), true));
                }
                expect(TokenType::RPAREN, "')' after arguments");
                finish(*call);
                return call;
            }
            auto node = startNode("Identifier");
            node->name = advance().lexeme;
            finish(*node);
            return node;
        }
        if (t.type == TokenType::LPAREN) {
            advance();
            auto inner = parseExpression();
            expect(TokenType::RPAREN, "')'");
            return inner;  // parentheses are not nodes
        }
        error("expression");
    }
};

}  // namespace

NodePtr parse(const std::vector<Token>& tokens) {
    Parser parser(tokens);
    return parser.parseProgram();
}
