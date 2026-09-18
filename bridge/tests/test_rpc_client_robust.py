"""PC bridge RPC robustness: skip CDC banners, first JSON object only."""

from __future__ import annotations

import json

import pytest

from groklink_os.rpc.client import GrokLinkClient, extract_json_object


def test_extract_json_object_skips_banner() -> None:
    raw = 'GrokLink OS boot\n{"ok":true,"cmd":"pong","api":7}'
    obj = extract_json_object(raw)
    assert obj is not None
    d = json.loads(obj)
    assert d["ok"] is True
    assert d["cmd"] == "pong"


def test_extract_json_object_nested_and_string_braces() -> None:
    raw = 'note {not json} prefix {"ok":true,"note":"brace } inside","n":1} trailer'
    obj = extract_json_object(raw)
    assert obj is not None
    d = json.loads(obj)
    assert d["ok"] is True
    assert d["note"] == "brace } inside"
    assert d["n"] == 1


def test_extract_json_object_none() -> None:
    assert extract_json_object("") is None
    assert extract_json_object("GrokLink OS ready") is None
    assert extract_json_object("{unterminated") is None


class _FakeTransport:
    def __init__(self, lines: list[str]) -> None:
        self.lines = list(lines)
        self.sent: list[str] = []

    def send_line(self, line: str) -> None:
        self.sent.append(line)

    def recv_line(self, timeout: float) -> str:
        if not self.lines:
            raise TimeoutError(f"serial RPC timeout after {timeout}s")
        return self.lines.pop(0)

    def close(self) -> None:
        return None


def test_client_skips_banners_then_json() -> None:
    c = GrokLinkClient(timeout=1.0)
    c._t = _FakeTransport(  # type: ignore[assignment]
        [
            "GrokLink OS 3.9 Field Card",
            "CDC ready",
            '{"ok":true,"cmd":"pong","api":7,"version":"3.9.0"}',
        ]
    )
    r = c.call("ping")
    assert r["ok"] is True
    assert r["api"] == 7
    assert c._t.sent[0].startswith('{"cmd":"ping"')  # type: ignore[union-attr]


def test_client_timeout_when_only_banners() -> None:
    c = GrokLinkClient(timeout=0.2)
    c._t = _FakeTransport(["boot", "still no json"])  # type: ignore[assignment]
    with pytest.raises(TimeoutError):
        c.call("ping")
