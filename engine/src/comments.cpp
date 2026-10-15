// Comment attachment (docs/contract.md §3 "Comments"). Member 4.
//
// Each comment is routed down the tree by position:
//   - Inside a container (Program/Block): if it lies inside a statement, descend into it;
//     otherwise it is trailing (same line as the previous statement's end), leading of the
//     next statement, or dangling.
//   - Inside a statement: if it lies inside a body statement (If/While/For bodies, function
//     body), descend; otherwise it is in the header/expression -> leading of the statement.
//     Exception: right after a non-block `then` body on the same line, it trails that body
//     (so `if (x) a = 1; // c` + `else` keeps the comment next to `a = 1;`).
#include "comments.h"

namespace {

struct Pos {
    int line;
    int column;
};

bool before(Pos a, Pos b) { return a.line < b.line || (a.line == b.line && a.column < b.column); }
bool beforeOrAt(Pos a, Pos b) { return !before(b, a); }

Pos startOf(const Node& n) { return {n.line, n.column}; }
Pos endOf(const Node& n) { return {n.endLine, n.endColumn}; }

bool contains(const Node& n, Pos p) { return beforeOrAt(startOf(n), p) && before(p, endOf(n)); }

// Children of a statement that are themselves statement bodies (never For init/update).
std::vector<Node*> bodyChildren(const Node& s) {
    std::vector<Node*> out;
    size_t from = 0;
    if (s.kind == "If" || s.kind == "While") from = 1;
    else if (s.kind == "For") from = 3;
    else if (s.kind == "FunctionDecl") from = s.children.empty() ? 0 : s.children.size() - 1;
    else return out;
    for (size_t i = from; i < s.children.size(); ++i) {
        if (isStatementKind(s.children[i]->kind)) out.push_back(s.children[i].get());
    }
    return out;
}

void addTrailing(Node& s, const std::string& text) {
    if (!s.trailingComment.empty()) s.trailingComment += " ";
    s.trailingComment += text;
}

void attachToStatement(Node& s, const Comment& c);

// Program or Block: route c among its statements.
void attachToContainer(Node& container, const Comment& c) {
    Pos p{c.line, c.column};
    Node* prev = nullptr;  // last statement ending at or before c
    for (auto& child : container.children) {
        if (!isStatementKind(child->kind)) continue;
        if (contains(*child, p)) {
            attachToStatement(*child, c);
            return;
        }
        if (beforeOrAt(endOf(*child), p)) {
            prev = child.get();
            continue;
        }
        // child starts after c.
        if (prev && prev->endLine == c.line) addTrailing(*prev, c.text);
        else child->leadingComments.push_back(c.text);
        return;
    }
    if (prev && prev->endLine == c.line) addTrailing(*prev, c.text);
    else container.danglingComments.push_back(c.text);
}

void attachToStatement(Node& s, const Comment& c) {
    if (s.kind == "Block") {
        attachToContainer(s, c);
        return;
    }
    Pos p{c.line, c.column};
    Node* prevBody = nullptr;
    for (Node* body : bodyChildren(s)) {
        if (contains(*body, p)) {
            attachToStatement(*body, c);
            return;
        }
        if (beforeOrAt(endOf(*body), p)) prevBody = body;
    }
    if (s.kind == "If" && prevBody && prevBody->kind != "Block" && prevBody->endLine == c.line) {
        addTrailing(*prevBody, c.text);
        return;
    }
    s.leadingComments.push_back(c.text);
}

}  // namespace

void attachComments(Node& program, const std::vector<Comment>& comments) {
    for (const auto& c : comments) attachToContainer(program, c);
}
