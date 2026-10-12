from fastapi.middleware.cors import CORSMiddleware
import json
import subprocess
import tempfile
from pathlib import Path

from fastapi import FastAPI, HTTPException
from pydantic import BaseModel

app = FastAPI(title="AST Visualizer API")
app.add_middleware(
    CORSMiddleware,
    allow_origins=["http://localhost:5173", "http://127.0.0.1:5173"],
    allow_methods=["*"],
    allow_headers=["*"],
)

MAX_SOURCE_BYTES = 1_000_000
TIMEOUT_SECONDS = 15


class SourceRequest(BaseModel):
    code: str
    language: str = "cpp"


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


@app.get("/")
def home():
    return {"message": "AST Visualizer API is running"}


@app.post("/api/analyze")
def analyze(request: SourceRequest):
    if request.language not in ("c", "cpp"):
        raise HTTPException(
            status_code=400,
            detail="Language must be 'c' or 'cpp'.",
        )

    if len(request.code.encode("utf-8")) > MAX_SOURCE_BYTES:
        raise HTTPException(
            status_code=413,
            detail="Source file is too large.",
        )

    suffix = ".c" if request.language == "c" else ".cpp"
    standard = "c17" if request.language == "c" else "c++20"
    compiler = "clang" if request.language == "c" else "clang++"
    clang_language = "c" if request.language == "c" else "c++"

    with tempfile.TemporaryDirectory() as temp_dir:
        source_path = Path(temp_dir) / f"input{suffix}"
        source_path.write_text(request.code, encoding="utf-8")

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

        if result.returncode != 0:
            return {
                "success": False,
                "ast": None,
                "formatted_code": None,
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

        # Format the original source code.
        formatted = run_tool(
            [
                "clang-format",
                f"--assume-filename=input{suffix}",
            ],
            request.code,
        )

        if formatted.returncode != 0:
            raise HTTPException(
                status_code=500,
                detail=formatted.stderr,
            )

        return {
            "success": True,
            "ast": ast,
            "formatted_code": formatted.stdout,
            "diagnostics": diagnostics,
        }
