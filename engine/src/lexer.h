// Member 2. See docs/contract.md §2.
#pragma once

#include <optional>

#include "token.h"

struct LexResult {
    std::vector<Token> tokens;      // ends with END_OF_FILE on success
    std::vector<Comment> comments;  // in source order
    std::optional<CompileError> error;  // set on failure; tokens holds what was lexed before it
};

LexResult lex(const std::string& source);
