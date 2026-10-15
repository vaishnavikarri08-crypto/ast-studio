// Shapes consumed from the backend (docs/contract.md §3, §5, §6) plus the
// normalised ViewNode used by every view in the UI.

export type Mode = "engine" | "clang";

export type Token = {
  type: string;
  lexeme: string;
  line: number;
  column: number;
};

export type SourceComment = {
  text: string;
  line: number;
  column: number;
};

export type AnalysisError = {
  stage: "lexer" | "parser" | "clang" | string;
  message: string;
  line: number;
  column: number;
};

/** Node produced by our engine (`astc`), contract §3. */
export type EngineNode = {
  kind: string;
  line: number;
  column: number;
  endLine: number;
  endColumn: number;
  children: EngineNode[];
  name?: string;
  op?: string;
  value?: string;
  varType?: string;
  leadingComments?: string[];
  trailingComment?: string;
  danglingComments?: string[];
};

export type ClangLoc = {
  offset?: number;
  line?: number;
  col?: number;
  tokLen?: number;
  file?: string;
  includedFrom?: { file?: string };
  spellingLoc?: ClangLoc;
  expansionLoc?: ClangLoc;
};

/** Raw clang `-ast-dump=json` node. */
export type ClangNode = {
  id?: string;
  kind?: string;
  name?: string;
  opcode?: string;
  value?: string | number;
  type?: { qualType?: string };
  isImplicit?: boolean;
  loc?: ClangLoc;
  range?: { begin?: ClangLoc; end?: ClangLoc };
  inner?: ClangNode[];
};

export type AnalyzeResponse = {
  mode: Mode;
  success: boolean;
  tokens: Token[];
  comments?: SourceComment[];
  ast: EngineNode | ClangNode | null;
  formatted: string | null;
  errors: AnalysisError[];
  diagnostics: string;
};

/** 1-based, end exclusive (contract §3). */
export type SourceRange = {
  line: number;
  column: number;
  endLine: number;
  endColumn: number;
};

export type ViewNode = {
  id: string;
  kind: string;
  label: string;
  detail?: string;
  range?: SourceRange;
  comments?: string[];
  isGenerated: boolean;
  children: ViewNode[];
};
