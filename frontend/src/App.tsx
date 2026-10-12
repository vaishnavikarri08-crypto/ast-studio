import dagre from "@dagrejs/dagre";
import { useMemo, useState } from "react";
import {
  GitBranch,
  Code2,
  Sparkles,
  Play,
  AlignLeft,
  AlertCircle,
  LoaderCircle,
  Search,
  ChevronDown,
  ChevronRight,
  ChevronsDownUp,
  ChevronsUpDown,
  Eye,
  EyeOff,
} from "lucide-react";

import {
  ReactFlow,
  Background,
  Controls,
  MiniMap,
  Handle,
  Position,
  type Node,
  type Edge,
} from "@xyflow/react";

import "@xyflow/react/dist/style.css";
import "./App.css";

const API_URL = "http://127.0.0.1:8000/api/analyze";

type AstNode = {
  kind?: string;
  name?: string;
  opcode?: string;
  value?: string | number;
  type?: { qualType?: string };
  isImplicit?: boolean;
  inner?: AstNode[];
};

type AnalysisResult = {
  success: boolean;
  ast: AstNode | null;
  formatted_code: string | null;
  diagnostics: string;
};

function getLabel(node: AstNode): string {
  return (
    node.name ||
    node.opcode ||
    (node.value !== undefined ? String(node.value) : "") ||
    node.type?.qualType ||
    ""
  );
}

function isCompilerGenerated(node: AstNode): boolean {
  return (
    node.isImplicit === true ||
    node.kind === "BuiltinType" ||
    Boolean(node.name?.startsWith("__"))
  );
}

function TreeNode({
  node,
  depth = 0,
  search,
  showGenerated,
  expandAll,
}: {
  node: AstNode;
  depth?: number;
  search: string;
  showGenerated: boolean;
  expandAll: boolean | null;
}) {
  const [manualExpanded, setManualExpanded] = useState(depth < 2);
  const children = node.inner ?? [];
  const expanded = expandAll === null ? manualExpanded : expandAll;

  function matchesSearch(current: AstNode): boolean {
    const text = `${current.kind ?? ""} ${getLabel(current)} ${
      current.type?.qualType ?? ""
    }`.toLowerCase();

    return (
      text.includes(search.toLowerCase()) ||
      (current.inner ?? []).some(matchesSearch)
    );
  }

  if (!showGenerated && isCompilerGenerated(node)) {
    return null;
  }

  if (search && !matchesSearch(node)) {
    return null;
  }

  return (
    <div className="tree-node">
      <button
        className="tree-row"
        style={{ paddingLeft: `${12 + depth * 18}px` }}
        onClick={() => {
          if (children.length > 0) {
            setManualExpanded(!expanded);
          }
        }}
        title={children.length ? "Expand or collapse node" : node.kind}
      >
        <span className="tree-toggle">
          {children.length > 0 ? (
            expanded ? (
              <ChevronDown size={14} />
            ) : (
              <ChevronRight size={14} />
            )
          ) : (
            "·"
          )}
        </span>

        <span className="node-kind">{node.kind ?? "Node"}</span>

        {getLabel(node) && (
          <span className="node-label">{getLabel(node)}</span>
        )}

        {node.type?.qualType && (
          <span className="node-type">{node.type.qualType}</span>
        )}
      </button>

      {expanded &&
        children.map((child, index) => (
          <TreeNode
            key={`${child.kind}-${child.name ?? ""}-${index}`}
            node={child}
            depth={depth + 1}
            search={search}
            showGenerated={showGenerated}
            expandAll={expandAll}
          />
        ))}
    </div>
  );
}

type GraphNodeData = {
  kind: string;
  label: string;
  type: string;
   value: string | number | null;
  opcode: string | null;
  name: string | null;
  isImplicit: boolean;
  childCount: number;
  [key: string]: unknown;
};

function AstGraphNode({ data }: { data: GraphNodeData }) {
  return (
    <div className="ast-graph-node">
      <Handle type="target" position={Position.Top} />

      <div className="graph-kind">{data.kind}</div>

      {data.label && <div className="graph-label">{data.label}</div>}

      {data.type && <div className="graph-type">{data.type}</div>}

      <Handle type="source" position={Position.Bottom} />
    </div>
  );
}

const graphNodeTypes = {
  astNode: AstGraphNode,
};


function buildAstGraph(ast: AstNode) {
  const nodes: Node<GraphNodeData>[] = [];
  const edges: Edge[] = [];
  const maxNodes = 250;
  let truncated = false;

  function visit(node: AstNode, id: string, parentId?: string) {
    if (nodes.length >= maxNodes) {
      truncated = true;
      return;
    }

    nodes.push({
      id,
      type: "astNode",
      position: { x: 0, y: 0 },
      data: {
        kind: node.kind ?? "Node",
        label: getLabel(node),
        type: node.type?.qualType ?? "",
         value: node.value ?? null,
         opcode: node.opcode ?? null,
         name: node.name ?? null,
         isImplicit: node.isImplicit ?? false,
         childCount: node.inner?.length ?? 0,
      },
    });

    if (parentId) {
      edges.push({
        id: `${parentId}-${id}`,
        source: parentId,
        target: id,
        type: "smoothstep",
        style: { stroke: "#818cf8", strokeWidth: 1.5 },
      });
    }

    const children = (node.inner ?? []).filter(
      (child) => !isCompilerGenerated(child),
    );

    children.forEach((child, index) => {
      if (nodes.length < maxNodes) {
        visit(child, `${id}-${index}`, id);
      } else {
        truncated = true;
      }
    });
  }

  visit(ast, "root");

  const graph = new dagre.graphlib.Graph();
  graph.setDefaultEdgeLabel(() => ({}));
  graph.setGraph({
    rankdir: "TB",
    nodesep: 35,
    ranksep: 75,
    marginx: 30,
    marginy: 30,
  });

  nodes.forEach((node) => {
    graph.setNode(node.id, { width: 190, height: 75 });
  });

  edges.forEach((edge) => {
    graph.setEdge(edge.source, edge.target);
  });

  dagre.layout(graph);

  nodes.forEach((node) => {
    const position = graph.node(node.id);
    node.position = {
      x: position.x - 95,
      y: position.y - 37.5,
    };
  });

  return { nodes, edges, truncated };
}


export default function App() {
  const [selectedNode, setSelectedNode] = useState<GraphNodeData | null>(null);
  const [code, setCode] = useState(
    `int calculate(int a, int b) {
    int result = a + b * 2;
    return result;
}

int main() {
    return calculate(10, 20);
}`,
  );

  const [result, setResult] = useState<AnalysisResult | null>(null);
  const [loading, setLoading] = useState(false);
  const [error, setError] = useState("");
  const [tab, setTab] = useState<"ast" | "visual" | "formatted">("ast");
  const [search, setSearch] = useState("");
  const [showGenerated, setShowGenerated] = useState(false);
  const [expandAll, setExpandAll] = useState<boolean | null>(null);

  const graph = useMemo(
    () =>
      result?.success && result.ast
        ? buildAstGraph(result.ast)
        : null,
    [result],
  );

  async function analyze() {
    setLoading(true);
    setError("");
    setResult(null);
    setSearch("");
    setExpandAll(null);

    try {
      const response = await fetch(API_URL, {
        method: "POST",
        headers: { "Content-Type": "application/json" },
        body: JSON.stringify({ code, language: "cpp" }),
      });

      const data: AnalysisResult = await response.json();

      if (!response.ok) {
        throw new Error(
          (data as unknown as { detail?: string }).detail ??
            "The analysis request failed.",
        );
      }

      setResult(data);

      if (!data.success) {
        setError(
          data.diagnostics || "The compiler could not parse this code.",
        );
      }
    } catch (err) {
      setError(
        err instanceof Error
          ? `${err.message}. Check that the backend is running.`
          : "Could not connect to the backend.",
      );
    } finally {
      setLoading(false);
    }
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

        <div className="status">
          <span /> Clang Engine
        </div>
      </header>

      <section className="intro">
        <div className="eyebrow">
          <Sparkles size={14} /> COMPILER DESIGN PROJECT
        </div>

        <h2>
          Understand your code, <span>visually.</span>
        </h2>

        <p>Explore syntax trees, visual graphs and formatted C++ source code.</p>
      </section>

      <section className="workspace">
        <article className="panel editor-panel">
          <div className="panel-heading">
            <div>
              <Code2 size={17} /> <strong>Source Code</strong>
            </div>

            <span className="language-tag">C++20</span>
          </div>

          <textarea
            aria-label="C++ source code"
            spellCheck={false}
            value={code}
            onChange={(event) => setCode(event.target.value)}
          />

          <div className="editor-footer">
            <span>{code.split("\n").length} lines</span>

            <button
              className="primary-button"
              onClick={analyze}
              disabled={loading}
            >
              {loading ? (
                <LoaderCircle className="spin" size={16} />
              ) : (
                <Play size={16} />
              )}

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
              <button
                className={tab === "ast" ? "active" : ""}
                onClick={() => setTab("ast")}
              >
                AST Tree
              </button>

              <button
                className={tab === "visual" ? "active" : ""}
                onClick={() => setTab("visual")}
              >
                Visual Graph
              </button>

              <button
                className={tab === "formatted" ? "active" : ""}
                onClick={() => setTab("formatted")}
              >
                Pretty Print
              </button>
            </div>
          </div>

          {result?.success && tab === "ast" && (
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

              <button
                className="tool-button"
                onClick={() => setShowGenerated(!showGenerated)}
              >
                {showGenerated ? (
                  <Eye size={15} />
                ) : (
                  <EyeOff size={15} />
                )}

                {showGenerated ? "Hide generated" : "Show generated"}
              </button>

              <button
                className="tool-button"
                onClick={() => setExpandAll(false)}
              >
                <ChevronsDownUp size={15} /> Collapse
              </button>

              <button
                className="tool-button"
                onClick={() => setExpandAll(true)}
              >
                <ChevronsUpDown size={15} /> Expand
              </button>
            </div>
          )}

          {loading ? (
            <div className="empty-state">
              <LoaderCircle className="spin" size={30} />
              <p>Parsing your code...</p>
            </div>
          ) : error ? (
            <div className="error-box">
              <AlertCircle size={19} />
              <pre>{error}</pre>
            </div>
          ) : result?.success ? (
            tab === "ast" && result.ast ? (
              <div className="tree-view">
                <TreeNode
                  node={result.ast}
                  search={search}
                  showGenerated={showGenerated}
                  expandAll={expandAll}
                />
              </div>
            ) : tab === "visual" && graph ? (
              <div>
                {graph.truncated && (
                  <p className="graph-notice">
                    Showing the first 250 AST nodes. Use AST Tree to explore
                    the complete tree.
                  </p>
                )}

                <div className="visual-graph">
                  <ReactFlow
                    nodes={graph.nodes}
                    edges={graph.edges}
                    nodeTypes={graphNodeTypes}
                    fitView
                    fitViewOptions={{ padding: 0.15, minZoom: 0.35, maxZoom: 1 }}
                    minZoom={0.1}
                    maxZoom={2}
                    defaultViewport={{ x: 0, y: 0, zoom: 0.7 }}
                    panOnDrag
                    panOnScroll={false}
                    zoomOnScroll
                    zoomOnPinch
                    zoomOnDoubleClick={false}
                    preventScrolling={true}
                  >
                    <Background color="#30394d" gap={20} />
                    <Controls />
                  </ReactFlow>
                </div>
              </div>
            ) : (
              <pre className="formatted-code">{result.formatted_code}</pre>
            )
          ) : (
            <div className="empty-state">
              <GitBranch size={38} />
              <strong>Your AST will appear here</strong>
              <p>Enter a program and select Analyze Code.</p>
            </div>
          )}
        </article>
      </section>

      <footer>
        <AlignLeft size={14} /> AST Studio <span>·</span> Powered by Clang
        &amp; React
      </footer>
    </main>
  );
}
