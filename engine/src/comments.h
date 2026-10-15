// Member 4. See docs/contract.md §3 "Comments".
#pragma once

#include "ast.h"
#include "token.h"

// Attaches comments to statement nodes of a parsed Program (uses node positions).
void attachComments(Node& program, const std::vector<Comment>& comments);
