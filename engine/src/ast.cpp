#include "ast.h"

#include <sstream>

NodePtr makeNode(const std::string& kind) {
    auto node = std::make_unique<Node>();
    node->kind = kind;
    return node;
}

bool isStatementKind(const std::string& kind) {
    return kind == "FunctionDecl" || kind == "Declaration" || kind == "Assignment" ||
           kind == "ExprStmt" || kind == "If" || kind == "While" || kind == "For" ||
           kind == "Return" || kind == "Block";
}

bool astEqual(const Node& a, const Node& b, bool compareComments) {
    if (a.kind != b.kind || a.op != b.op || a.name != b.name || a.value != b.value ||
        a.varType != b.varType || a.children.size() != b.children.size()) {
        return false;
    }
    if (compareComments &&
        (a.leadingComments != b.leadingComments || a.trailingComment != b.trailingComment ||
         a.danglingComments != b.danglingComments)) {
        return false;
    }
    for (size_t i = 0; i < a.children.size(); ++i) {
        if (!astEqual(*a.children[i], *b.children[i], compareComments)) return false;
    }
    return true;
}

std::string astDump(const Node& node, int depth) {
    std::ostringstream out;
    out << std::string(depth * 2, ' ') << node.kind;
    if (!node.varType.empty()) out << " type=" << node.varType;
    if (!node.name.empty()) out << " name=" << node.name;
    if (!node.op.empty()) out << " op=" << node.op;
    if (!node.value.empty()) out << " value=" << node.value;
    out << " @" << node.line << ":" << node.column << "-" << node.endLine << ":" << node.endColumn;
    for (const auto& c : node.leadingComments) out << " lead=" << c;
    if (!node.trailingComment.empty()) out << " trail=" << node.trailingComment;
    for (const auto& c : node.danglingComments) out << " dangling=" << c;
    out << "\n";
    for (const auto& child : node.children) out << astDump(*child, depth + 1);
    return out.str();
}
