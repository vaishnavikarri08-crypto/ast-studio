import json
import os
import re
import shutil
import subprocess
import tempfile
from pathlib import Path
from typing import Literal

from fastapi import FastAPI, HTTPException
from fastapi.middleware.cors import CORSMiddleware
from fastapi.staticfiles import StaticFiles
from pydantic import BaseModel

app = FastAPI(title="AST Visualizer API")
app.add_middleware(
    CORSMiddleware,
    # Extra origins (comma-separated) for a frontend hosted elsewhere.
    allow_origins=["http://localhost:5173", "http://127.0.0.1:5173"]
    + [o for o in os.environ.get("CORS_ORIGINS", "").split(",") if o],
    allow_methods=["*"],
    allow_headers=["*"],
)

MAX_SOURCE_BYTES = 1_000_000
TIMEOUT_SECONDS = 15

# Our C++ engine (see docs/contract.md §5). Build it with CMake in ../engine.
ENGINE_PATH = os.environ.get(
    "ASTC_PATH",
    str(Path(__file__).resolve().parent.parent / "engine" / "build" / "astc"),
)

# The built frontend (npm run build). When present it is served at /, so one
# process hosts the whole app and the API is same-origin. Override with FRONTEND_DIST.
FRONTEND_DIST = Path(os.environ.get(
    "FRONTEND_DIST",
    str(Path(__file__).resolve().parent.parent / "frontend" / "dist"),
))

# clang diagnostic lines look like: /tmp/x/input.cpp:3:12: error: expected ';'
CLANG_ERROR_RE = re.compile(r"^[^:\n]+:(\d+):(\d+): (?:fatal )?error: (.*)$", re.MULTILINE)


class SourceRequest(BaseModel):
    code: str
    language: Literal["c", "cpp"] = "cpp"
    mode: Literal["engine", "clang"] = "engine"


def run_tool(command, source=""):
    try:
        return subprocess.run(
            command,
            input=source,
            text=True,
            capture_output=True,
            timeout=TIMEOUT_SECONDS,
            check=False,
        )
    except subprocess.TimeoutExpired:
        raise HTTPException(
            status_code=408,
            detail="Analysis timed out.",
        )
    except FileNotFoundError:
        raise HTTPException(
            status_code=500,
            detail=f"Required tool not installed: {command[0]}",
        )


@app.get("/api/health")
def health():
    return {"message": "AST Visualizer API is running"}


@app.post("/api/analyze")
def analyze(request: SourceRequest):
    if len(request.code.encode("utf-8")) > MAX_SOURCE_BYTES:
        raise HTTPException(
            status_code=413,
            detail="Source file is too large.",
        )

    if request.mode == "engine":
        return analyze_with_engine(request.code)
    return analyze_with_clang(request.code, request.language)


def analyze_with_engine(code):
    if not Path(ENGINE_PATH).is_file():
        raise HTTPException(
            status_code=500,
            detail=f"Engine binary not found at {ENGINE_PATH}. Build it with CMake in ast-studio/engine.",
        )

    result = run_tool([ENGINE_PATH], code)
    if result.returncode != 0:
        raise HTTPException(
            status_code=500,
            detail=f"Engine failed: {result.stderr.strip()}",
        )

    try:
        response = json.loads(result.stdout)
    except json.JSONDecodeError:
        raise HTTPException(
            status_code=500,
            detail="Could not decode the engine output.",
        )

    response["mode"] = "engine"
    response["diagnostics"] = ""
    return response


def analyze_with_clang(code, language):
    suffix = ".c" if language == "c" else ".cpp"
    standard = "c17" if language == "c" else "c++20"
    compiler = "clang" if language == "c" else "clang++"
    clang_language = "c" if language == "c" else "c++"

    with tempfile.TemporaryDirectory() as temp_dir:
        source_path = Path(temp_dir) / f"input{suffix}"
        source_path.write_text(code, encoding="utf-8")

        # Generate the Abstract Syntax Tree as JSON.
        result = run_tool(
            [
                compiler,
                "-x", clang_language,
                f"-std={standard}",
                "-fsyntax-only",
                "-fno-color-diagnostics",
                "-Xclang", "-ast-dump=json",
                str(source_path),
            ]
        )

    diagnostics = result.stderr
    errors = [
        {"stage": "clang", "message": message, "line": int(line), "column": int(column)}
        for line, column, message in CLANG_ERROR_RE.findall(diagnostics)
    ]

    if result.returncode != 0:
        return {
            "mode": "clang",
            "success": False,
            "tokens": [],
            "ast": None,
            "formatted": None,
            "errors": errors or [{"stage": "clang", "message": diagnostics.strip(), "line": 0, "column": 0}],
            "diagnostics": diagnostics,
        }

    try:
        ast = json.loads(result.stdout)
    except json.JSONDecodeError:
        raise HTTPException(
            status_code=500,
            detail="Could not decode the compiler AST.",
        )

    # Remove compiler-provided declarations from the displayed AST.
    if isinstance(ast, dict) and ast.get("kind") == "TranslationUnitDecl":
        ast["inner"] = [
            node for node in ast.get("inner", [])
            if not (
                node.get("isImplicit", False)
                or node.get("name", "").startswith("__")
            )
        ]

    # Format the original source code; clang-format is optional.
    formatted = None
    if shutil.which("clang-format"):
        formatted_result = run_tool(
            ["clang-format", f"--assume-filename=input{suffix}"],
            code,
        )
        if formatted_result.returncode == 0:
            formatted = formatted_result.stdout

    return {
        "mode": "clang",
        "success": True,
        "tokens": [],
        "ast": ast,
        "formatted": formatted,
        "errors": [],
        "diagnostics": diagnostics,
    }


# Mounted last so the API routes above take precedence.
if (FRONTEND_DIST / "index.html").is_file():
    app.mount("/", StaticFiles(directory=FRONTEND_DIST, html=True), name="frontend")
else:
    @app.get("/")
    def home():
        return {"message": "AST Visualizer API is running"}
