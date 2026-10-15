import { useEffect, useImperativeHandle, useLayoutEffect, useRef, type Ref } from "react";
import { basicSetup } from "codemirror";
import { EditorState, StateEffect, StateField, type Text } from "@codemirror/state";
import { Decoration, EditorView, type DecorationSet } from "@codemirror/view";
import { cpp } from "@codemirror/lang-cpp";
import { lintGutter, setDiagnostics, type Diagnostic } from "@codemirror/lint";
import { oneDark } from "@codemirror/theme-one-dark";

import type { SourceRange } from "../ast/types";

export type EditorDiagnostic = {
  line: number;
  column: number;
  message: string;
  severity?: "error" | "warning" | "info";
};

export type EditorHandle = {
  /** Highlight (but do not select) a source range; null clears it. */
  highlight(range: SourceRange | null, scroll?: boolean): void;
  /** Move the cursor / selection to a location and focus the editor. */
  select(line: number, column: number, endLine?: number, endColumn?: number): void;
};

type Props = {
  value: string;
  onChange(value: string): void;
  /** Called when the user moves the cursor with the mouse or keyboard (1-based). */
  onCursor?(line: number, column: number): void;
  diagnostics?: EditorDiagnostic[];
  ref?: Ref<EditorHandle>;
};

/** 1-based line/column -> document offset, clamped to the document. */
function toPos(doc: Text, line: number, column: number): number {
  const ln = doc.line(Math.min(Math.max(line, 1), doc.lines));
  return Math.min(ln.from + Math.max(column - 1, 0), ln.to);
}

const setHighlight = StateEffect.define<{ from: number; to: number } | null>();
const highlightMark = Decoration.mark({ class: "cm-ast-highlight" });

const highlightField = StateField.define<DecorationSet>({
  create: () => Decoration.none,
  update(deco, tr) {
    deco = deco.map(tr.changes);
    for (const effect of tr.effects) {
      if (effect.is(setHighlight)) {
        deco =
          effect.value && effect.value.to > effect.value.from
            ? Decoration.set([highlightMark.range(effect.value.from, effect.value.to)])
            : Decoration.none;
      }
    }
    return deco;
  },
  provide: (field) => EditorView.decorations.from(field),
});

const studioTheme = EditorView.theme(
  {
    "&": { height: "380px", backgroundColor: "#0d121c", fontSize: "13px" },
    ".cm-scroller": { fontFamily: '"DM Mono", monospace', lineHeight: "1.75" },
    ".cm-gutters": { backgroundColor: "#0d121c", borderRight: "1px solid #1c2536" },
    ".cm-content": { padding: "12px 0" },
    ".cm-ast-highlight": {
      backgroundColor: "rgba(130, 119, 245, 0.28)",
      outline: "1px solid rgba(166, 160, 255, 0.55)",
      borderRadius: "2px",
    },
  },
  { dark: true },
);

export default function Editor({ value, onChange, onCursor, diagnostics, ref }: Props) {
  const containerRef = useRef<HTMLDivElement>(null);
  const viewRef = useRef<EditorView | null>(null);
  const initialValue = useRef(value);
  const onChangeRef = useRef(onChange);
  const onCursorRef = useRef(onCursor);

  useLayoutEffect(() => {
    onChangeRef.current = onChange;
    onCursorRef.current = onCursor;
  });

  useEffect(() => {
    if (!containerRef.current) return;
    const view = new EditorView({
      parent: containerRef.current,
      state: EditorState.create({
        doc: initialValue.current,
        extensions: [
          basicSetup,
          cpp(),
          oneDark,
          studioTheme,
          lintGutter(),
          highlightField,
          EditorView.updateListener.of((update) => {
            if (update.docChanged) {
              onChangeRef.current(update.state.doc.toString());
            }
            const userMoved = update.transactions.some(
              (tr) => tr.isUserEvent("select") && tr.selection,
            );
            if (userMoved && onCursorRef.current) {
              const head = update.state.selection.main.head;
              const line = update.state.doc.lineAt(head);
              onCursorRef.current(line.number, head - line.from + 1);
            }
          }),
        ],
      }),
    });
    viewRef.current = view;
    return () => {
      view.destroy();
      viewRef.current = null;
    };
  }, []);

  // Push external value changes (presets, resets) into the editor.
  useEffect(() => {
    const view = viewRef.current;
    if (!view) return;
    const current = view.state.doc.toString();
    if (current !== value) {
      view.dispatch({ changes: { from: 0, to: current.length, insert: value } });
    }
  }, [value]);

  useEffect(() => {
    const view = viewRef.current;
    if (!view) return;
    const doc = view.state.doc;
    const mapped: Diagnostic[] = (diagnostics ?? []).map((d) => {
      const from = toPos(doc, d.line, d.column);
      // Underline one character (or the rest of the line if at its end).
      const lineEnd = doc.lineAt(from).to;
      const to = from < lineEnd ? from + 1 : from;
      const start = from === to && from > doc.lineAt(from).from ? from - 1 : from;
      return {
        from: start,
        to: Math.max(to, start),
        severity: d.severity ?? "error",
        message: d.message,
      };
    });
    view.dispatch(setDiagnostics(view.state, mapped));
  }, [diagnostics]);

  useImperativeHandle(
    ref,
    () => ({
      highlight(range, scroll = false) {
        const view = viewRef.current;
        if (!view) return;
        if (!range) {
          view.dispatch({ effects: setHighlight.of(null) });
          return;
        }
        const doc = view.state.doc;
        const from = toPos(doc, range.line, range.column);
        const to = toPos(doc, range.endLine, range.endColumn);
        const effects: StateEffect<unknown>[] = [setHighlight.of({ from, to: Math.max(from, to) })];
        if (scroll) effects.push(EditorView.scrollIntoView(from, { y: "nearest" }));
        view.dispatch({ effects });
      },
      select(line, column, endLine, endColumn) {
        const view = viewRef.current;
        if (!view) return;
        const doc = view.state.doc;
        const anchor = toPos(doc, line, column);
        const head =
          endLine !== undefined && endColumn !== undefined
            ? toPos(doc, endLine, endColumn)
            : anchor;
        view.dispatch({
          selection: { anchor, head },
          effects: EditorView.scrollIntoView(anchor, { y: "center" }),
        });
        view.focus();
      },
    }),
    [],
  );

  return <div className="code-editor" ref={containerRef} />;
}
