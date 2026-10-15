// Member 3. See docs/contract.md §5.
#pragma once

#include <string>
#include <vector>

#include "ast.h"
#include "lexer.h"

std::string jsonEscape(const std::string& s);  // returns a quoted JSON string
std::string astToJson(const Node& node);

// Full engine response. ast/formatted may be null.
std::string responseToJson(const LexResult& lexed, const Node* ast, const std::string* formatted,
                           const std::vector<CompileError>& errors);
