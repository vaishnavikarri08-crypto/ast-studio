# Engine Contract (frozen — M1)

This is the single source of truth shared by the lexer, parser, pretty printer, backend and frontend.
Change it only by agreement; update every consumer in the same change.

## 1. Language (v1.5)

v1 scope from the PRD, plus functions, `return`, calls and `for` so that `samples/example.cpp` parses.

```ebnf
program      = { function_def | statement } EOF ;
function_def = type IDENT "(" [ param { "," param } ] ")" block ;      (* type IDENT followed by "(" *)
param        = type IDENT ;
type         = "int" | "float" | "void" ;
statement    = declaration | assignment | expr_stmt | if_stmt | while_stmt
             | for_stmt | return_stmt | block ;
declaration  = type IDENT [ "=" expression ] ";" ;
assignment   = IDENT "=" expression ";" ;
expr_stmt    = expression ";" ;
if_stmt      = "if" "(" expression ")" statement [ "else" statement ] ;
while_stmt   = "while" "(" expression ")" statement ;
for_stmt     = "for" "(" [ for_init ] ";" [ expression ] ";" [ for_update ] ")" statement ;
for_init     = type IDENT [ "=" expression ] | IDENT "=" expression ;
for_update   = IDENT "=" expression | expression ;
return_stmt  = "return" [ expression ] ";" ;
block        = "{" { statement } "}" ;
expression   = comparison ;
comparison   = additive [ ( "<" | ">" | "<=" | ">=" | "==" | "!=" ) additive ] ;   (* non-associative *)
additive     = term { ( "+" | "-" ) term } ;                                        (* left-assoc *)
term         = unary { ( "*" | "/" ) unary } ;                                       (* left-assoc *)
unary        = "-" unary | primary ;
primary      = INT_LIT | FLOAT_LIT | IDENT [ "(" [ expression { "," expression } ] ")" ]
             | "(" expression ")" ;
```

- `void` is only meaningful as a function return type, but the parser does not enforce that (no semantic analysis).
- Function definitions may appear only at top level. Nested `function_def` is a parse error.
- Errors: stop at the first error. Message format: `expected <what>, found '<lexeme>'` (use `end of input` for EOF).

## 2. Tokens

| type | lexemes |
|---|---|
| `KEYWORD` | `int float void if else while for return` |
| `IDENTIFIER` | `[A-Za-z_][A-Za-z0-9_]*` that is not a keyword |
| `INT_LITERAL` | `[0-9]+` |
| `FLOAT_LITERAL` | `[0-9]+ "." [0-9]+` (also `[0-9]+ "."` and `"." [0-9]+`) |
| `PLUS MINUS MULTIPLY DIVIDE` | `+ - * /` |
| `LT GT LE GE EQ NE` | `< > <= >= == !=` |
| `ASSIGNMENT_OPERATOR` | `=` |
| `LPAREN RPAREN LBRACE RBRACE SEMICOLON COMMA` | `( ) { } ; ,` |
| `END_OF_FILE` | empty lexeme, position just after the last character |

- Lines and columns are **1-based**. Columns count bytes. A tab counts as one column.
- `//` and `/* */` comments are not tokens. They go into the comment list. An unterminated `/*` is a lexer error.
- Any other character is a lexer error: `unexpected character '<c>'`.

## 3. AST nodes

Every node has `kind`, `line`, `column`, `endLine`, `endColumn`, `children`.
`endLine`/`endColumn` point **one past** the last character of the node's last token (exclusive end).

| kind | fields | children (ordered) |
|---|---|---|
| `Program` | — | top-level functions and statements |
| `FunctionDecl` | `name`, `varType` (return type) | `Param`…, then the body `Block` (always last) |
| `Param` | `name`, `varType` | — |
| `Declaration` | `name`, `varType` | `[init?]` |
| `Assignment` | `name` | `[value]` |
| `ExprStmt` | — | `[expression]` |
| `If` | — | `[condition, then, else?]` |
| `While` | — | `[condition, body]` |
| `For` | — | `[init, condition, update, body]`. A missing part is an `Empty` node |
| `Return` | — | `[value?]` |
| `Block` | — | statements |
| `Empty` | — | — (placeholder in `For` only) |
| `BinaryOp` | `op` (`+ - * / < > <= >= == !=`) | `[left, right]` |
| `UnaryOp` | `op` (`-`) | `[operand]` |
| `Call` | `name` | arguments |
| `Identifier` | `name` | — |
| `IntLiteral` | `value` (lexeme as string) | — |
| `FloatLiteral` | `value` (lexeme as string) | — |

- `For` init uses a `Declaration` or `Assignment` node, and update uses an `Assignment` or expression node. Inside a `for` header these nodes do not include the `;`.
- Parentheses are not nodes. The tree structure encodes them.

### Comments

- Statement-level nodes (`FunctionDecl` and every statement kind) may carry
  `leadingComments: string[]` and `trailingComment: string`.
- `Block` and `Program` may carry `danglingComments: string[]` for comments after the last statement.
- Comment strings are the exact source text, including `//` or `/* */`.
- Attachment rules:
  1. A comment on the same line as the end of a statement, after that statement, becomes its `trailingComment`. If there are several, they are joined with one space.
  2. Any other comment becomes a `leadingComment` of the next statement that starts after it, at the innermost block level containing the comment.
  3. If no statement follows in that block, it goes into that block's (or Program's) `danglingComments`.
  4. Comments inside an expression or a statement header (e.g. `a + /* x */ b`) become `leadingComments` of the enclosing statement.
- Fields are omitted from JSON when empty.

## 4. Pretty-print rules

- 4-space indent per block level. One statement per line. `\n` line endings, and the output ends with exactly one `\n`.
- Single spaces around binary operators and `=`. No space for unary minus (`-x`). `, ` between params and args.
- `if (c) {`, `} else {`, `} else if (c) {`, `while (c) {`, `for (int i = 0; i < n; i = i + 1) {`, `int f(int a, int b) {`.
- A non-block body of `if`/`else`/`while`/`for` is printed wrapped in `{ }`. This is the canonical form, so the round-trip compares ASTs after this normalisation.
- One blank line between top-level function definitions, and between a function and adjacent top-level statements.
- Parentheses only where precedence or associativity needs them (e.g. `a - (b - c)`, `(a + b) * c`, `-(a + b)`).
- Leading comments are printed on their own lines at the statement's indent, and a trailing comment after the statement, separated by one space.
- Invariant: `parse(pretty(ast)) == ast`, ignoring positions, with the `{}` normalisation above applied to both sides. Comment text is included in the comparison.

## 5. Engine I/O (`astc`)

`astc` reads source from **stdin** and writes one JSON object to **stdout**. The exit code is 0 even for source errors, and non-zero only on internal failure.

```json
{
  "success": true,
  "tokens":   [ { "type": "KEYWORD", "lexeme": "int", "line": 1, "column": 1 } ],
  "comments": [ { "text": "// hi", "line": 1, "column": 10 } ],
  "ast": { "kind": "Program", "line": 1, "column": 1, "endLine": 1, "endColumn": 10, "children": [ ... ] },
  "formatted": "int x = 1;\n",
  "errors": []
}
```

On error: `success: false`, `ast: null`, `formatted: null`, and `errors: [{ "stage": "lexer"|"parser", "message", "line", "column" }]`.
`tokens` holds the tokens produced before the error. After a lexer error, it holds those tokens without `END_OF_FILE`.

## 6. HTTP API (`POST /api/analyze`)

Request: `{ "code": string, "language": "c" | "cpp" = "cpp", "mode": "engine" | "clang" = "engine" }`.

Response (both modes): `{ "mode", "success", "tokens", "ast", "formatted", "errors", "diagnostics" }`.
- `engine`: the fields come from `astc`. `diagnostics` is `""`.
- `clang`: `tokens` is `[]`, `ast` is the raw clang JSON (children in `inner`), `formatted` is clang-format output (or `null` if clang-format is not installed), `errors` is parsed from clang's `file:line:col: error: msg` lines (stage `"clang"`), and `diagnostics` is the raw stderr.

HTTP 4xx/5xx with `{ "detail" }` is used only for bad requests (too large, bad mode), timeouts and missing tools.

## 7. Clarifications settled during implementation

These were pinned by tests and now count as part of the contract.

**Lexer**
- `\r` is whitespace, and a `//` comment's text excludes a trailing `\r`. `\v` and `\f` are whitespace.
- `1.` and `.5` are floats. A lone `.` is `unexpected character '.'`. `12ab` lexes as INT `12` then IDENT `ab` (the parser rejects it).
- `/*/` does not close itself. Comments collected before a lexer error are kept.

**Parser**
- A missing `For` part is a zero-width `Empty` node at the following `;` or `)`.
- Operator node ranges include parentheses around operands, e.g. `(a + b) * c` starts at the `(`.
- Nested function message: `function definitions are only allowed at top level` (not in the `expected …` format).
- `a < b < c` fails at the second `<`. `void v;` is accepted (no semantic analysis).

**JSON**
- Only `\" \\ \n \r \t` get short escapes. Other control bytes become `\u00XX`, and bytes ≥ 0x7F pass through unchanged.

**Printer**
- Empty `for` parts print as nothing: `for (;;) {`, `for (; ok;) {`.
- An empty block prints as `{` then `}` on the next line. An empty program prints as `""`.
- `else if` is printed in that form only when the inner `if` has no comments of its own. Otherwise it is printed as `else { if … }`.
- A comparison nested on either side of another comparison is parenthesised. Nested unary minus prints `--x`, which is safe because there is no `--` token.
- The blank line around a top-level function goes before its leading comments.

**Comments**
- Comments in a statement's header (condition, `for` header, parameter list, between `}` and `else`) become leading comments of that statement.
- A same-line comment after an unbraced `then` branch with an `else` attaches to the inner statement.
