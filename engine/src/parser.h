// Member 3. See docs/contract.md §1 and §3.
#pragma once

#include "ast.h"
#include "token.h"

// tokens must end with END_OF_FILE. Throws CompileException (stage "parser") on the first error.
NodePtr parse(const std::vector<Token>& tokens);
