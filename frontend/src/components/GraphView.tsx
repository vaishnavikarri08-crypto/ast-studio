import dagre from "@dagrejs/dagre";
import { useMemo } from "react";
import {
  ReactFlow,
  Background,
  Controls,
  Handle,
  Position,
  type Node,
  type Edge,
  type NodeProps,
} from "@xyflow/react";
import "@xyflow/react/dist/style.css";

import type { ViewNode } from "../ast/types";
import CommentBadge from "./CommentBadge";

const MAX_GRAPH_NODES = 250;

type GraphNodeData = {
  view: ViewNode;
  [key: string]: unknown;
};

type AstFlowNode = Node<GraphNodeData, "astNode">;

function AstGraphNode({ data }: NodeProps<AstFlowNode>) {
  const { view } = data;
  return (
    <div className="ast-graph-node">
      <Handle type="target" position={Position.Top} />
      <div className="graph-kind">
        {view.kind}
        <CommentBadge comments={view.comments} />
      </div>
      {view.label && <div className="graph-label">{view.label}</div>}
      {view.detail && <div className="graph-type">{view.detail}</div>}
      <Handle type="source" position={Position.Bottom} />
    </div>
  );
}

const graphNodeTypes = { astNode: AstGraphNode };

function buildAstGraph(root: ViewNode) {
  const nodes: AstFlowNode[] = [];
  const edges: Edge[] = [];
  let truncated = false;

  function visit(node: ViewNode, parentId?: string) {
    if (nodes.length >= MAX_GRAPH_NODES) {
      truncated = true;
      return;
    }
    nodes.push({
      id: node.id,
      type: "astNode",
      position: { x: 0, y: 0 },
      data: { view: node },
    });
    if (parentId) {
      edges.push({
        id: `${parentId}->${node.id}`,
        source: parentId,
        target: node.id,
        type: "smoothstep",
        style: { stroke: "#818cf8", strokeWidth: 1.5 },
      });
    }
    for (const child of node.children) {
      if (!child.isGenerated) visit(child, node.id);
    }
  }

  visit(root);

  const graph = new dagre.graphlib.Graph();
  graph.setDefaultEdgeLabel(() => ({}));
  graph.setGraph({ rankdir: "TB", nodesep: 35, ranksep: 75, marginx: 30, marginy: 30 });
  nodes.forEach((node) => graph.setNode(node.id, { width: 190, height: 75 }));
  edges.forEach((edge) => graph.setEdge(edge.source, edge.target));
  dagre.layout(graph);
  nodes.forEach((node) => {
    const position = graph.node(node.id);
    node.position = { x: position.x - 95, y: position.y - 37.5 };
  });

  return { nodes, edges, truncated };
}

type Props = {
  root: ViewNode;
  selectedId: string | null;
  onSelect(node: ViewNode): void;
  onHover(node: ViewNode | null): void;
};

export default function GraphView({ root, selectedId, onSelect, onHover }: Props) {
  const graph = useMemo(() => buildAstGraph(root), [root]);

  const nodes = useMemo(
    () =>
      graph.nodes.map((node) =>
        node.id === selectedId ? { ...node, className: "selected" } : node,
      ),
    [graph, selectedId],
  );

  return (
    <div>
      {graph.truncated && (
        <p className="graph-notice">
          Showing the first {MAX_GRAPH_NODES} AST nodes. Use AST Tree to explore the
          complete tree.
        </p>
      )}
      <div className="visual-graph">
        <ReactFlow
          nodes={nodes}
          edges={graph.edges}
          nodeTypes={graphNodeTypes}
          nodesDraggable={false}
          nodesConnectable={false}
          onNodeClick={(_, node) => onSelect((node as AstFlowNode).data.view)}
          onNodeMouseEnter={(_, node) => onHover((node as AstFlowNode).data.view)}
          onNodeMouseLeave={() => onHover(null)}
          fitView
          fitViewOptions={{ padding: 0.15, minZoom: 0.35, maxZoom: 1 }}
          minZoom={0.1}
          maxZoom={2}
          panOnDrag
          panOnScroll={false}
          zoomOnScroll
          zoomOnPinch
          zoomOnDoubleClick={false}
          preventScrolling
        >
          <Background color="#30394d" gap={20} />
          <Controls />
        </ReactFlow>
      </div>
    </div>
  );
}
