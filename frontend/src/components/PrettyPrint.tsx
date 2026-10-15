import { useMemo } from "react";

const MAX_DIFF_CELLS = 4_000_000;

/**
 * Marks which lines of `a` and `b` are not part of their longest common
 * subsequence (i.e. lines that the pretty printer changed). Comparison
 * ignores trailing whitespace. Returns null for very large inputs.
 */
function lineDiff(a: string[], b: string[]) {
  const n = a.length;
  const m = b.length;
  if (n * m > MAX_DIFF_CELLS) return null;
  const A = a.map((s) => s.trimEnd());
  const B = b.map((s) => s.trimEnd());
  const dp: Uint32Array[] = Array.from({ length: n + 1 }, () => new Uint32Array(m + 1));
  for (let i = n - 1; i >= 0; i--) {
    for (let j = m - 1; j >= 0; j--) {
      dp[i][j] = A[i] === B[j] ? dp[i + 1][j + 1] + 1 : Math.max(dp[i + 1][j], dp[i][j + 1]);
    }
  }
  const changedA = new Array<boolean>(n).fill(true);
  const changedB = new Array<boolean>(m).fill(true);
  let i = 0;
  let j = 0;
  while (i < n && j < m) {
    if (A[i] === B[j]) {
      changedA[i++] = false;
      changedB[j++] = false;
    } else if (dp[i + 1][j] >= dp[i][j + 1]) {
      i++;
    } else {
      j++;
    }
  }
  return { changedA, changedB };
}

function splitLines(text: string): string[] {
  const lines = text.split("\n");
  if (lines.length > 1 && lines[lines.length - 1] === "") lines.pop();
  return lines;
}

function CodeColumn({
  title,
  lines,
  changed,
  kind,
}: {
  title: string;
  lines: string[];
  changed: boolean[] | null;
  kind: "removed" | "added";
}) {
  return (
    <div className="pp-column">
      <div className="pp-title">{title}</div>
      <pre className="pp-code">
        {lines.map((line, index) => (
          <div
            key={index}
            className={`pp-line${changed?.[index] ? ` pp-${kind}` : ""}`}
          >
            <span className="pp-num">{index + 1}</span>
            <span className="pp-text">{line || " "}</span>
          </div>
        ))}
      </pre>
    </div>
  );
}

type Props = { input: string; formatted: string };

export default function PrettyPrint({ input, formatted }: Props) {
  const { left, right, diff } = useMemo(() => {
    const left = splitLines(input);
    const right = splitLines(formatted);
    return { left, right, diff: lineDiff(left, right) };
  }, [input, formatted]);

  const changes = diff ? diff.changedB.filter(Boolean).length : null;

  return (
    <div className="pretty-print">
      {changes !== null && (
        <p className="pp-summary">
          {changes === 0
            ? "Your input is already in canonical form."
            : `${changes} line${changes > 1 ? "s" : ""} changed by the pretty printer (highlighted).`}
        </p>
      )}
      <div className="pp-grid">
        <CodeColumn title="Your input" lines={left} changed={diff?.changedA ?? null} kind="removed" />
        <CodeColumn title="Formatted" lines={right} changed={diff?.changedB ?? null} kind="added" />
      </div>
    </div>
  );
}
