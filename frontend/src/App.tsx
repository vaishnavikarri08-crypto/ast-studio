import { lazy, Suspense, useCallback, useMemo, useRef, useState } from "react";
import {
  GitBranch,
  Code2,
  Sparkles,
  Play,
  AlignLeft,
  AlertCircle,
  LoaderCircle,
  Search,
  ChevronsDownUp,
  ChevronsUpDown,
  Eye,
  EyeOff,
  Cpu,
  ShieldCheck,
  BookOpen,
} from "lucide-react";

import "./App.css";

import { analyze } from "./api";
import { DEFAULT_CODE, PRESETS } from "./presets";
import type { AnalysisError, AnalyzeResponse, Mode, ViewNode } from "./ast/types";
import {
  allIds,
  ancestorIds,
  defaultExpanded,
  findDeepestAt,
  indexTree,
  searchMatches,
  toViewTree,
} from "./ast/adapter";
import Editor, { type EditorDiagnostic, type EditorHandle } from "./components/Editor";
import TreeView from "./components/TreeView";
import TokenTable from "./components/TokenTable";
import ErrorList from "./components/ErrorList";
import PrettyPrint from "./components/PrettyPrint";

// dagre + React Flow are most of the bundle, so load them only when the graph tab opens.
const GraphView = lazy(() => import("./components/GraphView"));

type Tab = "ast" | "visual" | "tokens" | "formatted";

const TABS: Array<{ id: Tab; label: string }> = [
  { id: "ast", label: "AST Tree" },
  { id: "visual", label: "Visual Graph" },
  { id: "tokens", label: "Tokens" },
  { id: "formatted", label: "Pretty Print" },
];

export default function App() {
  const editorRef = useRef<EditorHandle>(null);

  const [mode, setMode] = useState<Mode>("engine");
  const [code, setCode] = useState(DEFAULT_CODE);
  const [presetId, setPresetId] = useState("default");

  const [result, setResult] = useState<AnalyzeResponse | null>(null);
  const [analyzedCode, setAnalyzedCode] = useState("");
  const [loading, setLoading] = useState(false);
  const [requestError, setRequestError] = useState("");

  const [tab, setTab] = useState<Tab>("ast");
  const [search, setSearch] = useState("");
  const [showGenerated, setShowGenerated] = useState(false);
  const [expanded, setExpanded] = useState<Set<string>>(() => new Set());
  const [selectedId, setSelectedId] = useState<string | null>(null);

  const tree = useMemo(
    () => (result?.success ? toViewTree(result.mode, result.ast) : null),
    [result],
  );
  const index = useMemo(() => indexTree(tree), [tree]);
  const searchKeep = useMemo(() => searchMatches(tree, search), [tree, search]);
  const selectedNode = selectedId ? (index.get(selectedId)?.node ?? null) : null;

  const diagnostics = useMemo<EditorDiagnostic[]>(
    () =>
      (result?.errors ?? [])
        .filter((e) => e.line > 0)
        .map((e) => ({
          line: e.line,
          column: Math.max(e.column, 1),
          message: `${e.message} (${e.stage})`,
        })),
    [result],
  );

  // ----- actions -------------------------------------------------------------

  function applyResult(data: AnalyzeResponse, source: string) {
    setResult(data);
    setAnalyzedCode(source);
    const newTree = data.success ? toViewTree(data.mode, data.ast) : null;
    setExpanded(defaultExpanded(newTree));
    editorRef.current?.highlight(null);
  }

  function resetAnalysis() {
    setResult(null);
    setRequestError("");
    setSelectedId(null);
    setSearch("");
    editorRef.current?.highlight(null);
  }

  async function runAnalysis() {
    const source = code;
    setLoading(true);
    resetAnalysis();
    try {
      applyResult(await analyze(source, mode), source);
    } catch (err) {
      setRequestError(
        err instanceof Error
          ? `${err.message}. Check that the backend is running.`
          : "Could not connect to the backend.",
      );
    } finally {
      setLoading(false);
    }
  }

  async function loadMock() {
    // Development aid only: renders a hand-written, contract-valid engine
    // response for the default program when the backend is unavailable.
    const mock = (await import("./mock/engineSample.json")).default;
    resetAnalysis();
    setMode("engine");
    setCode(DEFAULT_CODE);
    applyResult(mock as unknown as AnalyzeResponse, DEFAULT_CODE);
  }

  function changeMode(next: Mode) {
    if (next === mode) return;
    setMode(next);
    resetAnalysis();
  }

  function choosePreset(id: string) {
    const preset = PRESETS.find((p) => p.id === id);
    if (!preset) return;
    setPresetId(id);
    setCode(preset.code);
    resetAnalysis();
  }

  const selectNode = useCallback(
    (node: ViewNode) => {
      setSelectedId(node.id);
      editorRef.current?.highlight(node.range ?? null, true);
    },
    [],
  );

  const hoverNode = useCallback(
    (node: ViewNode | null) => {
      editorRef.current?.highlight(node?.range ?? selectedNode?.range ?? null);
    },
    [selectedNode],
  );

  const toggleNode = useCallback((id: string) => {
    setExpanded((prev) => {
      const next = new Set(prev);
      if (next.has(id)) next.delete(id);
      else next.add(id);
      return next;
    });
  }, []);

  function handleCursor(line: number, column: number) {
    // Editor -> tree linking (engine mode only; clang ranges are best-effort).
    if (!tree || result?.mode !== "engine" || code !== analyzedCode) return;
    const node = findDeepestAt(tree, line, column);
    if (!node) return;
    setSelectedId(node.id);
    setExpanded((prev) => new Set([...prev, ...ancestorIds(index, node.id)]));
    editorRef.current?.highlight(node.range ?? null);
  }

  function locateError(error: AnalysisError) {
    editorRef.current?.select(error.line, Math.max(error.column, 1));
  }

  function locateToken(line: number, column: number, length: number) {
    editorRef.current?.select(line, column, line, column + length);
  }

  // ----- rendering -----------------------------------------------------------

  const failed = result !== null && !result.success;
  const languageTag = mode === "engine" ? "C subset" : "C++20";

  function renderOutput() {
    if (loading) {
      return (
        <div className="empty-state">
          <LoaderCircle className="spin" size={30} />
          <p>Parsing your code...</p>
        </div>
      );
    }

    if (requestError) {
      return (
        <div className="error-box">
          <AlertCircle size={19} />
          <div>
            <pre>{requestError}</pre>
            {import.meta.env.DEV && mode === "engine" && (
              <button className="tool-button mock-button" onClick={loadMock}>
                Load mock engine response (dev only)
              </button>
            )}
          </div>
        </div>
      );
    }

    if (!result) {
      return (
        <div className="empty-state">
          <GitBranch size={38} />
          <strong>Your AST will appear here</strong>
          <p>Enter a program and select Analyze Code.</p>
        </div>
      );
    }

    if (tab === "tokens") {
      return <TokenTable mode={result.mode} tokens={result.tokens} onLocate={locateToken} />;
    }

    if (failed) {
      return (
        <div className="empty-state">
          <AlertCircle size={30} />
          <strong>
            {tab === "formatted" ? "Formatted output" : "The AST"} is not available
          </strong>
          <p>
            Analysis stopped at the first error. Fix it (click the error to jump to it) and
            analyze again.
          </p>
        </div>
      );
    }

    if (tab === "formatted") {
      if (result.formatted === null) {
        return (
          <div className="empty-state">
            <AlertCircle size={30} />
            <strong>No formatted output</strong>
            <p>
              {result.mode === "clang"
                ? "clang-format is not installed on the server."
                : "The engine returned no formatted code."}
            </p>
          </div>
        );
      }
      return <PrettyPrint input={analyzedCode} formatted={result.formatted} />;
    }

    if (!tree) {
      return (
        <div className="empty-state">
          <AlertCircle size={30} />
          <strong>No AST returned</strong>
        </div>
      );
    }

    if (tab === "visual") {
      return (
        <Suspense fallback={<div className="empty-state">Loading graph…</div>}>
          <GraphView
            root={tree}
            selectedId={selectedId}
            onSelect={selectNode}
            onHover={hoverNode}
          />
        </Suspense>
      );
    }

    return (
      <TreeView
        root={tree}
        expanded={expanded}
        onToggle={toggleNode}
        searchKeep={searchKeep}
        showGenerated={showGenerated}
        selectedId={selectedId}
        onSelect={selectNode}
        onHover={hoverNode}
      />
    );
  }

  return (
    <main className="app-shell">
      <header className="topbar">
        <div className="brand">
          <div className="brand-icon">
            <GitBranch size={23} />
          </div>
          <div>
            <h1>
              AST<span>Studio</span>
            </h1>
            <p>Compiler Explorer</p>
          </div>
        </div>

        <div className="mode-toggle" role="radiogroup" aria-label="Analysis engine">
          <button
            role="radio"
            aria-checked={mode === "engine"}
            className={mode === "engine" ? "active" : ""}
            onClick={() => changeMode("engine")}
          >
            <Cpu size={14} /> Our Engine
          </button>
          <button
            role="radio"
            aria-checked={mode === "clang"}
            className={mode === "clang" ? "active" : ""}
            onClick={() => changeMode("clang")}
          >
            <ShieldCheck size={14} /> Clang reference
          </button>
        </div>
      </header>

      <section className="intro">
        <div className="eyebrow">
          <Sparkles size={14} /> COMPILER DESIGN PROJECT
        </div>
        <h2>
          Understand your code, <span>visually.</span>
        </h2>
        <p>
          Explore tokens, syntax trees, visual graphs and pretty-printed source — from our own
          engine or the Clang reference.
        </p>
      </section>

      <section className="workspace">
        <article className="panel editor-panel">
          <div className="panel-heading">
            <div>
              <Code2 size={17} /> <strong>Source Code</strong>
            </div>
            <div className="heading-actions">
              <label className="preset-select">
                <BookOpen size={14} />
                <select
                  aria-label="Example programs"
                  value={presetId}
                  onChange={(event) => choosePreset(event.target.value)}
                >
                  {PRESETS.map((preset) => (
                    <option key={preset.id} value={preset.id}>
                      {preset.title}
                    </option>
                  ))}
                </select>
              </label>
              <span className="language-tag">{languageTag}</span>
            </div>
          </div>

          <Editor
            ref={editorRef}
            value={code}
            onChange={setCode}
            onCursor={handleCursor}
            diagnostics={diagnostics}
          />

          <div className="editor-footer">
            <span>
              {code.split("\n").length} lines
              {result && code !== analyzedCode && (
                <span className="stale-note"> · edited since last analysis</span>
              )}
            </span>
            <button className="primary-button" onClick={runAnalysis} disabled={loading}>
              {loading ? <LoaderCircle className="spin" size={16} /> : <Play size={16} />}
              {loading ? "Analyzing..." : "Analyze Code"}
            </button>
          </div>
        </article>

        <article className="panel output-panel">
          <div className="panel-heading output-heading">
            <div>
              <GitBranch size={17} />
              <strong>Analysis Results</strong>
            </div>
            <div className="tabs">
              {TABS.map((t) => (
                <button
                  key={t.id}
                  className={tab === t.id ? "active" : ""}
                  onClick={() => setTab(t.id)}
                >
                  {t.label}
                </button>
              ))}
            </div>
          </div>

          {result && !loading && (
            <ErrorList
              mode={result.mode}
              errors={result.errors}
              diagnostics={result.diagnostics}
              onLocate={locateError}
            />
          )}

          {tree && !loading && tab === "ast" && (
            <div className="ast-toolbar">
              <div className="ast-search">
                <Search size={16} />
                <input
                  aria-label="Search AST"
                  placeholder="Search functions, variables, node types..."
                  value={search}
                  onChange={(event) => setSearch(event.target.value)}
                />
              </div>

              {result?.mode === "clang" && (
                <button className="tool-button" onClick={() => setShowGenerated(!showGenerated)}>
                  {showGenerated ? <Eye size={15} /> : <EyeOff size={15} />}
                  {showGenerated ? "Hide generated" : "Show generated"}
                </button>
              )}

              <button className="tool-button" onClick={() => setExpanded(new Set())}>
                <ChevronsDownUp size={15} /> Collapse
              </button>
              <button className="tool-button" onClick={() => setExpanded(allIds(tree))}>
                <ChevronsUpDown size={15} /> Expand
              </button>
            </div>
          )}

          {renderOutput()}
        </article>
      </section>

      <footer>
        <AlignLeft size={14} /> AST Studio <span>·</span> Powered by our engine, Clang &amp;
        React
      </footer>
    </main>
  );
}
