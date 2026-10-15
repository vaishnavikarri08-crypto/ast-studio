import { useEffect, useRef } from "react";
import { ChevronDown, ChevronRight } from "lucide-react";

import type { ViewNode } from "../ast/types";
import { formatRange } from "../ast/adapter";
import CommentBadge from "./CommentBadge";

type Props = {
  root: ViewNode;
  expanded: Set<string>;
  onToggle(id: string): void;
  /** Ids to keep while searching (null = no search active). */
  searchKeep: Set<string> | null;
  showGenerated: boolean;
  selectedId: string | null;
  onSelect(node: ViewNode): void;
  onHover(node: ViewNode | null): void;
};

type RowProps = Omit<Props, "root"> & { node: ViewNode; depth: number };

function TreeRow(props: RowProps) {
  const { node, depth, expanded, onToggle, searchKeep, showGenerated, selectedId, onSelect, onHover } =
    props;

  if (!showGenerated && node.isGenerated) return null;
  if (searchKeep && !searchKeep.has(node.id)) return null;

  const hasChildren = node.children.length > 0;
  // While searching, show every surviving branch.
  const isOpen = searchKeep ? true : expanded.has(node.id);

  return (
    <div className="tree-node">
      <div
        role="treeitem"
        aria-expanded={hasChildren ? isOpen : undefined}
        aria-selected={selectedId === node.id}
        tabIndex={0}
        data-node-id={node.id}
        className={`tree-row${selectedId === node.id ? " selected" : ""}`}
        style={{ paddingLeft: `${12 + depth * 18}px` }}
        title={node.range ? `${node.kind} @ ${formatRange(node.range)}` : node.kind}
        onClick={() => {
          onSelect(node);
          if (hasChildren && !isOpen && !searchKeep) onToggle(node.id);
        }}
        onKeyDown={(event) => {
          if (event.key === "Enter" || event.key === " ") {
            event.preventDefault();
            onSelect(node);
            if (hasChildren) onToggle(node.id);
          }
        }}
        onMouseEnter={() => onHover(node)}
        onMouseLeave={() => onHover(null)}
      >
        <button
          className="tree-toggle"
          tabIndex={-1}
          aria-label={isOpen ? "Collapse node" : "Expand node"}
          onClick={(event) => {
            event.stopPropagation();
            if (hasChildren) onToggle(node.id);
          }}
        >
          {hasChildren ? (
            isOpen ? <ChevronDown size={14} /> : <ChevronRight size={14} />
          ) : (
            "·"
          )}
        </button>

        <span className="node-kind">{node.kind}</span>
        {node.label && <span className="node-label">{node.label}</span>}
        <CommentBadge comments={node.comments} />
        {node.detail && <span className="node-type">{node.detail}</span>}
      </div>

      {isOpen &&
        node.children.map((child) => (
          <TreeRow key={child.id} {...props} node={child} depth={depth + 1} />
        ))}
    </div>
  );
}

export default function TreeView({ root, ...rest }: Props) {
  const containerRef = useRef<HTMLDivElement>(null);
  const { selectedId } = rest;

  useEffect(() => {
    if (!selectedId || !containerRef.current) return;
    const row = containerRef.current.querySelector(
      `[data-node-id="${CSS.escape(selectedId)}"]`,
    );
    row?.scrollIntoView({ block: "nearest", behavior: "smooth" });
  }, [selectedId]);

  return (
    <div className="tree-view" role="tree" ref={containerRef}>
      <TreeRow node={root} depth={0} {...rest} />
    </div>
  );
}
