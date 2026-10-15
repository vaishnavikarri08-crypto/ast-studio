import { Info } from "lucide-react";

import type { Mode, Token } from "../ast/types";

type TokenCategory =
  | "keyword"
  | "identifier"
  | "literal"
  | "operator"
  | "punctuation"
  | "eof";

const OPERATORS = new Set([
  "PLUS", "MINUS", "MULTIPLY", "DIVIDE",
  "LT", "GT", "LE", "GE", "EQ", "NE",
  "ASSIGNMENT_OPERATOR",
]);

const PUNCTUATION = new Set(["LPAREN", "RPAREN", "LBRACE", "RBRACE", "SEMICOLON", "COMMA"]);

function tokenCategory(type: string): TokenCategory {
  if (type === "KEYWORD") return "keyword";
  if (type === "IDENTIFIER") return "identifier";
  if (type.endsWith("_LITERAL")) return "literal";
  if (OPERATORS.has(type)) return "operator";
  if (PUNCTUATION.has(type)) return "punctuation";
  if (type === "END_OF_FILE") return "eof";
  return "punctuation";
}

const LEGEND: TokenCategory[] = ["keyword", "identifier", "literal", "operator", "punctuation"];

type Props = {
  mode: Mode;
  tokens: Token[];
  onLocate(line: number, column: number, length: number): void;
};

export default function TokenTable({ mode, tokens, onLocate }: Props) {
  if (mode === "clang") {
    return (
      <div className="empty-state">
        <Info size={30} />
        <strong>Tokens are only available from our engine</strong>
        <p>Switch the header toggle to “Our Engine” to see the lexer output.</p>
      </div>
    );
  }

  if (!tokens.length) {
    return (
      <div className="empty-state">
        <Info size={30} />
        <strong>No tokens</strong>
        <p>The lexer produced no tokens for this input.</p>
      </div>
    );
  }

  return (
    <div className="token-view">
      <div className="token-legend">
        {LEGEND.map((cat) => (
          <span key={cat} className={`token-chip cat-${cat}`}>
            {cat}
          </span>
        ))}
        <span className="token-count">{tokens.length} tokens</span>
      </div>
      <table className="token-table">
        <thead>
          <tr>
            <th>#</th>
            <th>Type</th>
            <th>Lexeme</th>
            <th>Line</th>
            <th>Col</th>
          </tr>
        </thead>
        <tbody>
          {tokens.map((token, index) => {
            const cat = tokenCategory(token.type);
            return (
              <tr
                key={index}
                onClick={() => onLocate(token.line, token.column, token.lexeme.length)}
                title="Show in editor"
              >
                <td className="muted">{index + 1}</td>
                <td>
                  <span className={`token-chip cat-${cat}`}>{token.type}</span>
                </td>
                <td className="lexeme">
                  {token.lexeme === "" ? <em className="muted">(eof)</em> : token.lexeme}
                </td>
                <td>{token.line}</td>
                <td>{token.column}</td>
              </tr>
            );
          })}
        </tbody>
      </table>
    </div>
  );
}
