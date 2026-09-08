"""Agent loop + transport selection tests (no hardware required)."""

from __future__ import annotations

from pathlib import Path
from typing import Any

from groklink_os.observe.agent_loop import (
    SYSTEM_PROMPT,
    execute_tool_calls,
    openai_chat_payload,
    run_scripted_observation_session,
    tools_for_anthropic,
    tools_for_openai,
)
from groklink_os.observe.store import ObservationStore
from groklink_os.observe.tools import ToolDispatcher
from groklink_os.rpc.client import GrokLinkClient, _looks_like_serial


class MockClient:
    def edu_ack(self, phrase: str = "") -> dict[str, Any]:
        return {"ok": True, "edu": True}

    def status(self) -> dict[str, Any]:
        return {"ok": True, "version": "3.2.0", "api": 3, "edu": True, "radio": 0, "heap_free": 1}

    def subghz_probe(self) -> dict[str, Any]:
        return {"ok": True, "partnum": 0, "version": 20, "hw": False}

    def subghz_rx(self, freq_hz: int = 433_920_000, ms: int = 200) -> dict[str, Any]:
        return {
            "ok": True,
            "freq_hz": freq_hz,
            "ms": ms,
            "pulses": 9,
            "rssi": -70,
            "sim": True,
            "ts_ms": 1,
        }

    def spectrum(self, freqs: list[int], ms: int = 200, settle_ms: int = 50) -> dict[str, Any]:
        return {
            "ok": True,
            "bands": [{"freq_hz": f, "pulses": 3, "rssi": -75} for f in freqs],
            "ms": ms,
            "settle_ms": settle_ms,
        }

    def close(self) -> None:
        return None


def test_looks_like_serial() -> None:
    assert _looks_like_serial("COM12") is True
    assert _looks_like_serial("com3") is True
    assert _looks_like_serial("/dev/ttyUSB0") is True
    assert _looks_like_serial("127.0.0.1") is False
    assert _looks_like_serial("7341") is False


def test_client_serial_mode_flag() -> None:
    c = GrokLinkClient(serial_port="COM9", timeout=1.0)
    assert c.serial_port == "COM9"
    assert c.baud == 230400


def test_tools_for_openai_and_anthropic() -> None:
    oai = tools_for_openai()
    anth = tools_for_anthropic()
    assert len(oai) == len(anth)
    assert anth[0]["name"] == oai[0]["function"]["name"]
    assert "input_schema" in anth[0]
    assert "passive" in SYSTEM_PROMPT.lower() or "Authorized" in SYSTEM_PROMPT


def test_openai_chat_payload() -> None:
    body = openai_chat_payload("What is on 433 MHz?")
    assert body["tools"]
    assert body["messages"][0]["role"] == "system"
    assert body["messages"][-1]["content"].startswith("What is on")


def test_execute_tool_calls(tmp_path: Path) -> None:
    store = ObservationStore(root=tmp_path, persist=False)
    d = ToolDispatcher(client=MockClient(), store=store)  # type: ignore[arg-type]
    tcs = [
        {
            "id": "call_1",
            "type": "function",
            "function": {
                "name": "observe_rx",
                "arguments": '{"freq_hz":433920000,"ms":100}',
            },
        }
    ]
    msgs = execute_tool_calls(d, tcs)
    assert len(msgs) == 1
    assert msgs[0]["role"] == "tool"
    assert msgs[0]["tool_call_id"] == "call_1"
    d.close()


def test_scripted_session(tmp_path: Path) -> None:
    store = ObservationStore(root=tmp_path, persist=False)
    d = ToolDispatcher(client=MockClient(), store=store)  # type: ignore[arg-type]
    report = run_scripted_observation_session(
        d,
        freqs_hz=[433920000],
        dwell_ms=50,
        spectrum=True,
        monitor_chunks=0,
    )
    assert report["ok"] is True
    assert report["narratives"]
    assert report["safety"]["tx"] is False
    d.close()
