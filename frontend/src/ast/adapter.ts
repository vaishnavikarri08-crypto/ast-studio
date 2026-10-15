import type {
  ClangLoc,
  ClangNode,
  EngineNode,
  Mode,
  SourceRange,
  ViewNode,
} from "./types";

// ---------------------------------------------------------------------------
// Engine -> ViewNode
// ---------------------------------------------------------------------------

function engineComments(node: EngineNode): string[] | undefined {
  const out: string[] = [];
  for (const c of node.leadingComments ?? []) out.push(`leading: ${c}`);
  if (node.trailingComment) out.push(`trailing: ${node.trailingComment}`);
  for (const c of node.danglingComments ?? []) out.push(`dangling: ${c}`);
  return out.length ? out : undefined;
}

function fromEngine(node: EngineNode, id: string): ViewNode {
  const label = node.op ?? node.name ?? node.value ?? "";
  const hasRange =
    typeof node.line === "number" && typeof node.column === "number";

  return {
    id,
    kind: node.kind ?? "Node",
    label,
    detail: node.varType,
    range: hasRange
      ? {
          line: node.line,
          column: node.column,
          endLine: node.endLine ?? node.line,
          endColumn: node.endColumn ?? node.column + 1,
        }
      : undefined,
    comments: engineComments(node),
    isGenerated: false,
    children: (node.children ?? []).map((child, index) =>
      fromEngine(child, `${id}.${index}`),
    ),
  };
}

// ---------------------------------------------------------------------------
// Clang -> ViewNode
// ---------------------------------------------------------------------------

// clang's JSON dumper omits `line` (and `file`) when it is unchanged since the
// previously printed location, so locations must be resolved in document
// order while remembering the last line / file seen.
type LocState = { line: number | null; fromMain: boolean };

type Point = { line: number; col: number; tokLen: number; fromMain: boolean };

function resolveLoc(loc: ClangLoc | undefined, state: LocState): Point | null {
  if (!loc) return null;
  // For macro expansions, clang nests spelling/expansion locations; both are
  // printed in that order and both update the "last" state.
  if (loc.spellingLoc || loc.expansionLoc) {
    resolveLoc(loc.spellingLoc, state);
    return resolveLoc(loc.expansionLoc, state);
  }
  // `file` is printed only when it changes; `includedFrom` marks a header.
  if (loc.file !== undefined) state.fromMain = loc.includedFrom === undefined;
  if (loc.line !== undefined) state.line = loc.line;
  if (loc.col === undefined || state.line === null) return null;
  return {
    line: state.line,
    col: loc.col,
    tokLen: loc.tokLen ?? 1,
    fromMain: state.fromMain,
  };
}

function isClangGenerated(node: ClangNode): boolean {
  return (
    node.isImplicit === true ||
    node.kind === "BuiltinType" ||
    Boolean(node.name?.startsWith("__"))
  );
}

function fromClang(node: ClangNode, id: string, state: LocState): ViewNode {
  // Keep this order: it matches clang's output order (loc, range.begin, range.end).
  resolveLoc(node.loc, state);
  const begin = resolveLoc(node.range?.begin, state);
  const end = resolveLoc(node.range?.end, state);

  let range: SourceRange | undefined;
  if (begin && end && begin.fromMain && end.fromMain) {
    range = {
      line: begin.line,
      column: begin.col,
      endLine: end.line,
      endColumn: end.col + end.tokLen,
    };
  }

  const label =
    node.name ??
    node.opcode ??
    (node.value !== undefined ? String(node.value) : "");

  return {
    id,
    kind: node.kind ?? "Node",
    label,
    detail: node.type?.qualType,
    range,
    isGenerated: isClangGenerated(node),
    children: (node.inner ?? []).map((child, index) =>
      fromClang(child, `${id}.${index}`, state),
    ),
  };
}

// ---------------------------------------------------------------------------
// Public helpers
// ---------------------------------------------------------------------------

export function toViewTree(mode: Mode, ast: unknown): ViewNode | null {
  if (!ast || typeof ast !== "object") return null;
  if (mode === "engine") return fromEngine(ast as EngineNode, "0");
  return fromClang(ast as ClangNode, "0", { line: null, fromMain: true });
}

export type NodeIndex = Map<string, { node: ViewNode; parentId: string | null }>;

export function indexTree(root: ViewNode | null): NodeIndex {
  const index: NodeIndex = new Map();
  if (!root) return index;
  const stack: Array<[ViewNode, string | null]> = [[root, null]];
  while (stack.length) {
    const [node, parentId] = stack.pop()!;
    index.set(node.id, { node, parentId });
    for (const child of node.children) stack.push([child, node.id]);
  }
  return index;
}

export function ancestorIds(index: NodeIndex, id: string): string[] {
  const out: string[] = [];
  let current = index.get(id)?.parentId ?? null;
  while (current) {
    out.push(current);
    current = index.get(current)?.parentId ?? null;
  }
  return out;
}

/** Ids of nodes expanded by default (depth < 2). */
export function defaultExpanded(root: ViewNode | null): Set<string> {
  const out = new Set<string>();
  if (!root) return out;
  out.add(root.id);
  for (const child of root.children) out.add(child.id);
  return out;
}

export function allIds(root: ViewNode | null): Set<string> {
  return new Set(indexTree(root).keys());
}

function before(aLine: number, aCol: number, bLine: number, bCol: number) {
  return aLine < bLine || (aLine === bLine && aCol <= bCol);
}

export function rangeContains(
  range: SourceRange,
  line: number,
  column: number,
): boolean {
  return (
    before(range.line, range.column, line, column) &&
    before(line, column, range.endLine, range.endColumn)
  );
}

/** Deepest node whose range contains the 1-based (line, column). */
export function findDeepestAt(
  root: ViewNode | null,
  line: number,
  column: number,
): ViewNode | null {
  if (!root) return null;
  let best: ViewNode | null = null;

  function visit(node: ViewNode) {
    if (node.range && rangeContains(node.range, line, column)) best = node;
    for (const child of node.children) {
      // Children without a range (rare) are still searched for descendants.
      if (!child.range || rangeContains(child.range, line, column)) {
        visit(child);
      }
    }
  }

  visit(root);
  return best;
}

export function nodeText(node: ViewNode): string {
  return `${node.kind} ${node.label} ${node.detail ?? ""}`.toLowerCase();
}

/** Ids of nodes that match `query` or have a matching descendant. */
export function searchMatches(
  root: ViewNode | null,
  query: string,
): Set<string> | null {
  const q = query.trim().toLowerCase();
  if (!root || !q) return null;
  const keep = new Set<string>();

  function visit(node: ViewNode): boolean {
    let hit = nodeText(node).includes(q);
    for (const child of node.children) {
      if (visit(child)) hit = true;
    }
    if (hit) keep.add(node.id);
    return hit;
  }

  visit(root);
  return keep;
}

export function formatRange(range: SourceRange): string {
  return `${range.line}:${range.column}–${range.endLine}:${range.endColumn}`;
}
