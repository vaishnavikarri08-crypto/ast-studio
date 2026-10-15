// Compact JSON serializer. Member 3. See docs/contract.md §5.
#include "json.h"

#include <cstdio>

std::string jsonEscape(const std::string& s) {
    std::string out = "\"";
    out.reserve(s.size() + 2);
    for (char ch : s) {
        unsigned char c = static_cast<unsigned char>(ch);
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof buf, "\\u%04x", c);
                    out += buf;
                } else {
                    out += ch;  // UTF-8 bytes pass through unchanged
                }
        }
    }
    out += "\"";
    return out;
}

namespace {

void appendField(std::string& out, const char* key, const std::string& value) {
    out += ",\"";
    out += key;
    out += "\":";
    out += jsonEscape(value);
}

void appendInt(std::string& out, const char* key, int value) {
    out += ",\"";
    out += key;
    out += "\":";
    out += std::to_string(value);
}

void appendStringArray(std::string& out, const char* key, const std::vector<std::string>& items) {
    out += ",\"";
    out += key;
    out += "\":[";
    for (size_t i = 0; i < items.size(); ++i) {
        if (i) out += ",";
        out += jsonEscape(items[i]);
    }
    out += "]";
}

void appendNode(std::string& out, const Node& n) {
    out += "{\"kind\":";
    out += jsonEscape(n.kind);
    if (!n.op.empty()) appendField(out, "op", n.op);
    if (!n.name.empty()) appendField(out, "name", n.name);
    if (!n.value.empty()) appendField(out, "value", n.value);
    if (!n.varType.empty()) appendField(out, "varType", n.varType);
    appendInt(out, "line", n.line);
    appendInt(out, "column", n.column);
    appendInt(out, "endLine", n.endLine);
    appendInt(out, "endColumn", n.endColumn);
    if (!n.leadingComments.empty()) appendStringArray(out, "leadingComments", n.leadingComments);
    if (!n.trailingComment.empty()) appendField(out, "trailingComment", n.trailingComment);
    if (!n.danglingComments.empty()) appendStringArray(out, "danglingComments", n.danglingComments);
    out += ",\"children\":[";
    for (size_t i = 0; i < n.children.size(); ++i) {
        if (i) out += ",";
        appendNode(out, *n.children[i]);
    }
    out += "]}";
}

}  // namespace

std::string astToJson(const Node& node) {
    std::string out;
    appendNode(out, node);
    return out;
}

std::string responseToJson(const LexResult& lexed, const Node* ast, const std::string* formatted,
                           const std::vector<CompileError>& errors) {
    bool success = errors.empty() && ast != nullptr;
    std::string out = "{\"success\":";
    out += success ? "true" : "false";

    out += ",\"tokens\":[";
    for (size_t i = 0; i < lexed.tokens.size(); ++i) {
        const Token& t = lexed.tokens[i];
        if (i) out += ",";
        out += "{\"type\":";
        out += jsonEscape(tokenTypeName(t.type));
        appendField(out, "lexeme", t.lexeme);
        appendInt(out, "line", t.line);
        appendInt(out, "column", t.column);
        out += "}";
    }
    out += "]";

    out += ",\"comments\":[";
    for (size_t i = 0; i < lexed.comments.size(); ++i) {
        const Comment& c = lexed.comments[i];
        if (i) out += ",";
        out += "{\"text\":";
        out += jsonEscape(c.text);
        appendInt(out, "line", c.line);
        appendInt(out, "column", c.column);
        out += "}";
    }
    out += "]";

    out += ",\"ast\":";
    out += ast ? astToJson(*ast) : "null";
    out += ",\"formatted\":";
    out += formatted ? jsonEscape(*formatted) : "null";

    out += ",\"errors\":[";
    for (size_t i = 0; i < errors.size(); ++i) {
        const CompileError& e = errors[i];
        if (i) out += ",";
        out += "{\"stage\":";
        out += jsonEscape(e.stage);
        appendField(out, "message", e.message);
        appendInt(out, "line", e.line);
        appendInt(out, "column", e.column);
        out += "}";
    }
    out += "]}";
    return out;
}
