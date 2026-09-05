"""Lightweight OpenAI-tools-compatible local HTTP endpoint (stdlib only).

Endpoints:
  GET  /health
  GET  /v1/tools
  POST /v1/tools/call          {"name": "...", "arguments": {...}}
  POST /v1/tools/call_batch    {"tool_calls":[...]}  OpenAI tool_calls array
  POST /v1/session/run         scripted passive multi-step session
  POST /v1/chat/completions    tool-aware local shim + tool_calls executor
  GET  /v1/observations/recent?limit=20
  GET  /v1/observe/stream      SSE of monitor chunks
  GET  /v1/ports               list serial ports (if pyserial available)

Binds to 127.0.0.1 by default. Passive observation only.
"""

from __future__ import annotations

import json
import threading
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from typing import Any, Optional
from urllib.parse import parse_qs, urlparse

from groklink_os.observe.agent_loop import (
    SYSTEM_PROMPT,
    execute_tool_calls,
    run_scripted_observation_session,
    tools_for_openai,
)
from groklink_os.observe.tools import TOOL_DEFINITIONS, ToolDispatcher, tools_openai_format
from groklink_os.rpc.client import list_serial_ports


class ObserveAPIServer:
    def __init__(
        self,
        dispatcher: ToolDispatcher,
        *,
        host: str = "127.0.0.1",
        port: int = 8741,
    ) -> None:
        self.dispatcher = dispatcher
        self.host = host
        self.port = port
        self._httpd: Optional[ThreadingHTTPServer] = None
        self._thread: Optional[threading.Thread] = None

    def start(self, *, background: bool = True) -> str:
        outer = self

        class Handler(BaseHTTPRequestHandler):
            def log_message(self, fmt: str, *args: Any) -> None:
                return

            def _json(self, code: int, obj: Any) -> None:
                body = json.dumps(obj, default=str).encode("utf-8")
                self.send_response(code)
                self.send_header("Content-Type", "application/json")
                self.send_header("Content-Length", str(len(body)))
                self.send_header("Access-Control-Allow-Origin", "*")
                self.end_headers()
                self.wfile.write(body)

            def _read_json(self) -> dict[str, Any]:
                n = int(self.headers.get("Content-Length") or 0)
                if n <= 0:
                    return {}
                raw = self.rfile.read(n)
                try:
                    data = json.loads(raw.decode("utf-8"))
                except json.JSONDecodeError:
                    return {}
                return data if isinstance(data, dict) else {}

            def do_OPTIONS(self) -> None:  # noqa: N802
                self.send_response(204)
                self.send_header("Access-Control-Allow-Origin", "*")
                self.send_header("Access-Control-Allow-Methods", "GET, POST, OPTIONS")
                self.send_header("Access-Control-Allow-Headers", "Content-Type, Authorization")
                self.end_headers()

            def do_GET(self) -> None:  # noqa: N802
                parsed = urlparse(self.path)
                path = parsed.path.rstrip("/") or "/"
                qs = parse_qs(parsed.query)

                if path in ("/health", "/v1/health"):
                    self._json(
                        200,
                        {
                            "ok": True,
                            "service": "groklink-os-observe",
                            "passive_only": True,
                            "system_prompt_chars": len(SYSTEM_PROMPT),
                        },
                    )
                    return
                if path == "/v1/tools":
                    self._json(200, {"object": "list", "data": tools_openai_format()})
                    return
                if path == "/v1/ports":
                    self._json(200, {"ports": list_serial_ports()})
                    return
                if path == "/v1/system_prompt":
                    self._json(200, {"role": "system", "content": SYSTEM_PROMPT})
                    return
                if path == "/v1/observations/recent":
                    limit = int((qs.get("limit") or ["20"])[0])
                    kind = (qs.get("kind") or [None])[0]
                    r = outer.dispatcher.dispatch(
                        "get_recent_activity", {"limit": limit, "kind": kind}
                    )
                    self._json(200, r)
                    return
                if path == "/v1/observe/stream":
                    self._sse_monitor()
                    return
                self._json(404, {"ok": False, "error": "not_found"})

            def do_POST(self) -> None:  # noqa: N802
                parsed = urlparse(self.path)
                path = parsed.path.rstrip("/") or "/"
                body = self._read_json()

                if path == "/v1/tools/call":
                    name = body.get("name") or body.get("tool") or ""
                    arguments = body.get("arguments") or body.get("params") or {}
                    if not name and "function" in body:
                        r = outer.dispatcher.dispatch_openai_tool_call(body)
                    else:
                        r = outer.dispatcher.dispatch(str(name), dict(arguments))
                    self._json(200, r)
                    return

                if path == "/v1/tools/call_batch":
                    tcs = body.get("tool_calls") or body.get("calls") or []
                    msgs = execute_tool_calls(outer.dispatcher, list(tcs))
                    self._json(200, {"ok": True, "messages": msgs})
                    return

                if path == "/v1/session/run":
                    r = run_scripted_observation_session(
                        outer.dispatcher,
                        freqs_hz=body.get("freqs_hz"),
                        dwell_ms=int(body.get("dwell_ms") or 200),
                        spectrum=bool(body.get("spectrum", True)),
                        monitor_chunks=int(body.get("monitor_chunks") or 0),
                        monitor_interval_ms=int(body.get("monitor_interval_ms") or 600),
                    )
                    self._json(200, r)
                    return

                if path == "/v1/chat/completions":
                    self._json(200, outer._chat_completions(body))
                    return

                self._json(404, {"ok": False, "error": "not_found"})

            def _sse_monitor(self) -> None:
                self.send_response(200)
                self.send_header("Content-Type", "text/event-stream")
                self.send_header("Cache-Control", "no-cache")
                self.send_header("Connection", "keep-alive")
                self.send_header("Access-Control-Allow-Origin", "*")
                self.end_headers()
                for _ in range(32):
                    r = outer.dispatcher.dispatch("get_monitor_chunk", {"wait_ms": 3000})
                    chunk = r.get("result")
                    if chunk is None and not (r.get("status") or {}).get("running"):
                        payload = {"event": "ended", "status": r.get("status")}
                        self.wfile.write(f"data: {json.dumps(payload, default=str)}\n\n".encode())
                        break
                    if chunk is None:
                        self.wfile.write(b": keepalive\n\n")
                    else:
                        self.wfile.write(
                            f"data: {json.dumps(chunk, default=str)}\n\n".encode("utf-8")
                        )
                    try:
                        self.wfile.flush()
                    except OSError:
                        break

        self._httpd = ThreadingHTTPServer((self.host, self.port), Handler)
        url = f"http://{self.host}:{self.port}"
        if background:
            self._thread = threading.Thread(target=self._httpd.serve_forever, daemon=True)
            self._thread.start()
        else:
            self._httpd.serve_forever()
        return url

    def stop(self) -> None:
        if self._httpd:
            self._httpd.shutdown()
            self._httpd.server_close()
            self._httpd = None

    def _chat_completions(self, body: dict[str, Any]) -> dict[str, Any]:
        """OpenAI-compatible shim:

        - If body.tool_calls present: execute and return tool role messages content.
        - If messages end with assistant tool_calls: execute those.
        - Else return tools + system prompt guidance (client still needs a real LLM for reasoning).
        """
        # Explicit tool_calls field
        tool_calls = body.get("tool_calls")
        if not tool_calls:
            msgs = body.get("messages") or []
            for m in reversed(msgs):
                if isinstance(m, dict) and m.get("tool_calls"):
                    tool_calls = m["tool_calls"]
                    break

        if tool_calls:
            results = execute_tool_calls(self.dispatcher, list(tool_calls))
            return {
                "id": "glk-observe-local",
                "object": "chat.completion",
                "model": body.get("model") or "groklink-os-observe",
                "choices": [
                    {
                        "index": 0,
                        "message": {
                            "role": "assistant",
                            "content": json.dumps(
                                {
                                    "note": "Tool results executed locally (passive RF only).",
                                    "tool_messages": results,
                                },
                                default=str,
                            ),
                        },
                        "finish_reason": "stop",
                    }
                ],
                "tool_messages": results,
            }

        return {
            "id": "glk-observe-local",
            "object": "chat.completion",
            "model": body.get("model") or "groklink-os-observe",
            "choices": [
                {
                    "index": 0,
                    "message": {
                        "role": "assistant",
                        "content": (
                            "GrokLink observe API is ready. Use tools via POST /v1/tools/call "
                            "or send assistant tool_calls to this endpoint for local execution. "
                            "Pair with an external LLM for multi-turn reasoning. "
                            "Passive RF only; authorized lab use."
                        ),
                    },
                    "finish_reason": "stop",
                }
            ],
            "tools": TOOL_DEFINITIONS,
            "system_prompt": SYSTEM_PROMPT,
        }


def serve_forever(
    host: str = "127.0.0.1",
    port: int = 8741,
    dispatcher: Optional[ToolDispatcher] = None,
) -> None:
    d = dispatcher or ToolDispatcher()
    server = ObserveAPIServer(d, host=host, port=port)
    print(f"GrokLink OS observe API on http://{host}:{port} (passive only)")
    print("  GET  /v1/tools  POST /v1/tools/call  POST /v1/session/run")
    try:
        server.start(background=False)
    finally:
        d.close()
