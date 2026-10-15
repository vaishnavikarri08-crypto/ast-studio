import { AlertCircle } from "lucide-react";

import type { AnalysisError, Mode } from "../ast/types";

type Props = {
  mode: Mode;
  errors: AnalysisError[];
  diagnostics: string;
  onLocate(error: AnalysisError): void;
};

export default function ErrorList({ mode, errors, diagnostics, onLocate }: Props) {
  const showDiagnostics = mode === "clang" && diagnostics.trim() !== "";
  if (!errors.length && !showDiagnostics) return null;

  return (
    <div className="error-list">
      {errors.length > 0 && (
        <>
          <div className="error-list-title">
            <AlertCircle size={15} />
            {errors.length} error{errors.length > 1 ? "s" : ""}
          </div>
          <ul>
            {errors.map((error, index) => (
              <li key={index}>
                <button onClick={() => onLocate(error)} title="Go to location">
                  <span className="error-loc">
                    {error.line}:{error.column}
                  </span>
                  <span className="error-sep">—</span>
                  <span className="error-msg">{error.message}</span>
                  <span className="error-stage">({error.stage})</span>
                </button>
              </li>
            ))}
          </ul>
        </>
      )}
      {showDiagnostics && (
        <details className="raw-diagnostics">
          <summary>Raw clang diagnostics</summary>
          <pre>{diagnostics}</pre>
        </details>
      )}
    </div>
  );
}
