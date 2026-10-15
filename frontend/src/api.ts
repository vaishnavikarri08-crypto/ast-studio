import type { AnalyzeResponse, Mode } from "./ast/types";

// Production builds are served by the backend itself, so the API is same-origin.
export const API_URL: string =
  import.meta.env.VITE_API_URL ||
  (import.meta.env.DEV ? "http://127.0.0.1:8000/api/analyze" : "/api/analyze");

export async function analyze(
  code: string,
  mode: Mode,
  signal?: AbortSignal,
): Promise<AnalyzeResponse> {
  const response = await fetch(API_URL, {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ code, language: "cpp", mode }),
    signal,
  });

  let data: unknown = null;
  try {
    data = await response.json();
  } catch {
    // Non-JSON body; handled below.
  }

  if (!response.ok) {
    const detail = (data as { detail?: unknown } | null)?.detail;
    throw new Error(
      typeof detail === "string"
        ? detail
        : `The analysis request failed (HTTP ${response.status})`,
    );
  }
  if (!data || typeof data !== "object") {
    throw new Error("The backend returned an unexpected response");
  }

  const raw = data as Partial<AnalyzeResponse>;
  return {
    mode: raw.mode ?? mode,
    success: Boolean(raw.success),
    tokens: raw.tokens ?? [],
    comments: raw.comments,
    ast: raw.ast ?? null,
    formatted: raw.formatted ?? null,
    errors: raw.errors ?? [],
    diagnostics: raw.diagnostics ?? "",
  };
}
