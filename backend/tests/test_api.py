import json
import shutil
import stat
import sys
from pathlib import Path

import pytest
from fastapi.testclient import TestClient

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
import app as app_module  # noqa: E402

client = TestClient(app_module.app)
REAL_ENGINE = Path(app_module.ENGINE_PATH)


def fake_engine(tmp_path, payload, exit_code=0):
    """A stand-in astc that ignores stdin and prints payload."""
    script = tmp_path / "astc"
    script.write_text(
        f"#!/bin/sh\ncat >/dev/null\nprintf '%s' '{json.dumps(payload)}'\nexit {exit_code}\n"
    )
    script.chmod(script.stat().st_mode | stat.S_IEXEC)
    return str(script)


def test_home():
    assert client.get("/").status_code == 200
    assert client.get("/api/health").status_code == 200


def test_rejects_unknown_mode():
    response = client.post("/api/analyze", json={"code": "int x;", "mode": "gcc"})
    assert response.status_code == 422


def test_rejects_large_source():
    code = "x" * (app_module.MAX_SOURCE_BYTES + 1)
    response = client.post("/api/analyze", json={"code": code})
    assert response.status_code == 413


def test_engine_mode_passes_through_engine_json(tmp_path, monkeypatch):
    payload = {"success": True, "tokens": [], "comments": [], "ast": {"kind": "Program", "children": []},
               "formatted": "", "errors": []}
    monkeypatch.setattr(app_module, "ENGINE_PATH", fake_engine(tmp_path, payload))
    data = client.post("/api/analyze", json={"code": "int x;"}).json()
    assert data["mode"] == "engine"
    assert data["diagnostics"] == ""
    assert data["ast"]["kind"] == "Program"


def test_engine_missing_binary(monkeypatch):
    monkeypatch.setattr(app_module, "ENGINE_PATH", "/nonexistent/astc")
    response = client.post("/api/analyze", json={"code": "int x;"})
    assert response.status_code == 500
    assert "Engine binary not found" in response.json()["detail"]


def test_engine_internal_failure(tmp_path, monkeypatch):
    monkeypatch.setattr(app_module, "ENGINE_PATH", fake_engine(tmp_path, {}, exit_code=2))
    assert client.post("/api/analyze", json={"code": "int x;"}).status_code == 500


# --- Integration with the real engine (skipped until it is built) ---

needs_engine = pytest.mark.skipif(not REAL_ENGINE.is_file(), reason="engine not built")


@needs_engine
def test_real_engine_precedence():
    data = client.post("/api/analyze", json={"code": "int result=a+b*c;"}).json()
    assert data["success"] is True
    decl = data["ast"]["children"][0]
    assert decl["kind"] == "Declaration"
    plus = decl["children"][0]
    assert plus["op"] == "+"
    assert plus["children"][1]["op"] == "*"
    assert data["formatted"] == "int result = a + b * c;\n"
    assert [t["type"] for t in data["tokens"]][:3] == ["KEYWORD", "IDENTIFIER", "ASSIGNMENT_OPERATOR"]


@needs_engine
def test_real_engine_syntax_error():
    data = client.post("/api/analyze", json={"code": "int x = 1\nint y;"}).json()
    assert data["success"] is False
    assert data["ast"] is None and data["formatted"] is None
    assert data["errors"][0]["stage"] == "parser"
    assert data["errors"][0]["line"] == 2


# --- clang reference mode ---

needs_clang = pytest.mark.skipif(shutil.which("clang++") is None, reason="clang++ not installed")


@needs_clang
def test_clang_mode_success():
    data = client.post("/api/analyze", json={"code": "int main() { return 0; }", "mode": "clang"}).json()
    assert data["mode"] == "clang"
    assert data["success"] is True
    assert data["ast"]["kind"] == "TranslationUnitDecl"
    assert data["tokens"] == []


@needs_clang
def test_clang_mode_parses_errors():
    data = client.post("/api/analyze", json={"code": "int main() { return 0 }", "mode": "clang"}).json()
    assert data["success"] is False
    assert data["errors"][0]["stage"] == "clang"
    assert data["errors"][0]["line"] == 1
    assert "expected ';'" in data["errors"][0]["message"]
