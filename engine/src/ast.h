// Shared AST. Owned by Member 1 (contract) — see docs/contract.md §3.
#pragma once

#include <memory>
#include <string>
#include <vector>

struct Node {
    std::string kind;
    int line = 0;
    int column = 0;
    int endLine = 0;    // exclusive end
    int endColumn = 0;

    // Kind-specific; empty string means "absent".
    std::string op;
    std::string name;
    std::string value;
    std::string varType;

    std::vector<std::unique_ptr<Node>> children;

    std::vector<std::string> leadingComments;
    std::string trailingComment;
    std::vector<std::string> danglingComments;
};

using NodePtr = std::unique_ptr<Node>;

NodePtr makeNode(const std::string& kind);

// True for FunctionDecl and every statement kind (what comments attach to).
bool isStatementKind(const std::string& kind);

// Structural equality ignoring positions. Compares comments when compareComments is true.
bool astEqual(const Node& a, const Node& b, bool compareComments = true);

// Debug dump, one node per line, indented. Used in test failure messages.
std::string astDump(const Node& node, int depth = 0);
