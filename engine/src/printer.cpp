// Pretty printer (docs/contract.md §4). Member 4.
#include "printer.h"

namespace {

// Expression precedence levels (higher binds tighter).
enum Prec { COMPARISON = 1, ADDITIVE = 2, TERM = 3, UNARY = 4, PRIMARY = 5 };

int precOf(const Node& e) {
    if (e.kind == "BinaryOp") {
        if (e.op == "+" || e.op == "-") return ADDITIVE;
        if (e.op == "*" || e.op == "/") return TERM;
        return COMPARISON;
    }
    if (e.kind == "UnaryOp") return UNARY;
    return PRIMARY;
}

std::string expr(const Node& e);

std::string wrapIf(bool cond, const std::string& s) { return cond ? "(" + s + ")" : s; }

std::string expr(const Node& e) {
    if (e.kind == "BinaryOp" && e.children.size() == 2) {
        int p = precOf(e);
        const Node& l = *e.children[0];
        const Node& r = *e.children[1];
        // Left-assoc: equal precedence on the left is fine, except for non-associative comparison.
        bool lp = precOf(l) < p || (p == COMPARISON && precOf(l) == COMPARISON);
        bool rp = precOf(r) <= p;
        return wrapIf(lp, expr(l)) + " " + e.op + " " + wrapIf(rp, expr(r));
    }
    if (e.kind == "UnaryOp" && e.children.size() == 1) {
        const Node& o = *e.children[0];
        return e.op + wrapIf(precOf(o) < UNARY, expr(o));
    }
    if (e.kind == "Call") {
        std::string s = e.name + "(";
        for (size_t i = 0; i < e.children.size(); ++i) {
            if (i) s += ", ";
            s += expr(*e.children[i]);
        }
        return s + ")";
    }
    if (e.kind == "Identifier") return e.name;
    if (e.kind == "IntLiteral" || e.kind == "FloatLiteral") return e.value;
    return "";  // Empty or unknown
}

// Simple statement text without the `;` (also used inside a for header).
std::string simple(const Node& s) {
    if (s.kind == "Declaration") {
        std::string out = s.varType + " " + s.name;
        if (!s.children.empty()) out += " = " + expr(*s.children[0]);
        return out;
    }
    if (s.kind == "Assignment") {
        return s.name + " = " + (s.children.empty() ? "" : expr(*s.children[0]));
    }
    if (s.kind == "ExprStmt") return s.children.empty() ? "" : expr(*s.children[0]);
    if (s.kind == "Empty") return "";
    return expr(s);  // bare expression (for update/condition)
}

class Printer {
public:
    std::string out;

    void program(const Node& p) {
        for (size_t i = 0; i < p.children.size(); ++i) {
            const Node& c = *p.children[i];
            if (i > 0 && (c.kind == "FunctionDecl" || p.children[i - 1]->kind == "FunctionDecl")) {
                out += "\n";
            }
            statement(c, 0);
        }
        comments(p.danglingComments, 0);
    }

private:
    static std::string pad(int indent) { return std::string(indent * 4, ' '); }

    void comments(const std::vector<std::string>& list, int indent) {
        for (const auto& c : list) out += pad(indent) + c + "\n";
    }

    // Ends the current statement line, appending the trailing comment if any.
    void endLine(const Node& s) {
        if (!s.trailingComment.empty()) out += " " + s.trailingComment;
        out += "\n";
    }

    // Prints `{ ... }` for a body; a non-block body is wrapped. No newline after `}`.
    void body(const Node& b, int indent) {
        out += "{\n";
        if (b.kind == "Block") {
            for (const auto& c : b.children) statement(*c, indent + 1);
            comments(b.danglingComments, indent + 1);
        } else {
            statement(b, indent + 1);
        }
        out += pad(indent) + "}";
    }

    static bool hasComments(const Node& n) {
        return !n.leadingComments.empty() || !n.trailingComment.empty() ||
               !n.danglingComments.empty();
    }

    // `if (c) {...}` chain without leading comments / final newline.
    void ifChain(const Node& s, int indent) {
        out += "if (" + expr(*s.children[0]) + ") ";
        body(*s.children[1], indent);
        if (s.children.size() > 2) {
            const Node& e = *s.children[2];
            out += " else ";
            if (e.kind == "If" && !hasComments(e)) ifChain(e, indent);
            else body(e, indent);
        }
    }

    void statement(const Node& s, int indent) {
        comments(s.leadingComments, indent);
        out += pad(indent);
        if (s.kind == "If") {
            ifChain(s, indent);
        } else if (s.kind == "While") {
            out += "while (" + expr(*s.children[0]) + ") ";
            body(*s.children[1], indent);
        } else if (s.kind == "For") {
            // Empty parts print as nothing: `for (;;)`, `for (int i = 0;; i = i + 1)`.
            std::string cond = simple(*s.children[1]);
            std::string update = simple(*s.children[2]);
            out += "for (" + simple(*s.children[0]) + ";" + (cond.empty() ? "" : " " + cond) +
                   ";" + (update.empty() ? "" : " " + update) + ") ";
            body(*s.children[3], indent);
        } else if (s.kind == "FunctionDecl") {
            out += s.varType + " " + s.name + "(";
            size_t n = s.children.size();
            for (size_t i = 0; i + 1 < n; ++i) {
                if (i) out += ", ";
                out += s.children[i]->varType + " " + s.children[i]->name;
            }
            out += ") ";
            if (n > 0) body(*s.children[n - 1], indent);
            else out += "{\n" + pad(indent) + "}";
        } else if (s.kind == "Block") {
            body(s, indent);
        } else if (s.kind == "Return") {
            out += s.children.empty() ? "return;" : "return " + expr(*s.children[0]) + ";";
        } else {
            out += simple(s) + ";";
        }
        endLine(s);
    }
};

}  // namespace

std::string prettyPrint(const Node& program) {
    Printer p;
    p.program(program);
    return p.out;
}
