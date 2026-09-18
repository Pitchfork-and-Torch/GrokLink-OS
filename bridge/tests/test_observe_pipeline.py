"""Host-side signal observability unit tests (no device required)."""

from __future__ import annotations

from pathlib import Path
from typing import Any

from groklink_os.observe.packager import ObservationPackager
from groklink_os.observe.schema import (
    OBSERVATION_SCHEMA_ID,
    OBSERVATION_SCHEMA_ID_V1,
    ObservationKind,
    occupancy_from_activity,
    schema_description,
)
from groklink_os.observe.store import ObservationStore
from groklink_os.observe.tools import TOOL_DEFINITIONS, ToolDispatcher, tools_openai_format


class MockClient:
    def __init__(self) -> None:
        self.edu_calls = 0

    def connect(self) -> None:
        return None

    def close(self) -> None:
        return None

    def edu_ack(self, phrase: str = "") -> dict[str, Any]:
        self.edu_calls += 1
        return {"ok": True, "edu": True}

    def status(self) -> dict[str, Any]:
        return {
            "ok": True,
            "version": "3.2.0",
            "api": 3,
            "edu": True,
            "radio": 0,
            "heap_free": 12000,
        }

    def subghz_probe(self) -> dict[str, Any]:
        return {"ok": True, "partnum": 0, "version": 20, "hw": False}

    def subghz_rx(self, freq_hz: int = 433_920_000, ms: int = 400) -> dict[str, Any]:
        return {
            "ok": True,
            "err": 0,
            "freq_hz": freq_hz,
            "ms": ms,
            "pulses": 18,
            "rssi": -62,
            "sim": True,
            "ts_ms": 12345,
            "kind": "rx",
        }

    def spectrum(
        self, freqs: list[int], ms: int = 400, settle_ms: int = 2000
    ) -> dict[str, Any]:
        bands = [{"freq_hz": f, "pulses": 5 + (i * 3), "rssi": -70 + i} for i, f in enumerate(freqs)]
        return {
            "ok": True,
            "kind": "spectrum",
            "ms": ms,
            "settle_ms": settle_ms,
            "ts_ms": 20000,
            "bands": bands,
        }

    def mission_list(self) -> dict[str, Any]:
        return {"ok": True, "missions": "lab_passive_433,lab_spectrum_planner", "count": 2}

    def skill_list(self) -> dict[str, Any]:
        return {"ok": True, "skills": "lab_passive_listen", "count": 1}

    def mission_arm(self, mission_id: str) -> dict[str, Any]:
        return {"ok": True, "id": mission_id}

    def mission_run(self, steps: int = 8) -> dict[str, Any]:
        return {
            "ok": True,
            "ran": steps,
            "status": {"ok": True, "id": "lab_passive_433", "state": "done", "last_pulses": 3},
        }

    def mission_status(self, mission_id: str = "") -> dict[str, Any]:
        return {
            "ok": True,
            "id": mission_id or "lab_passive_433",
            "state": "done",
            "pc": 4,
            "steps": 4,
            "last_pulses": 3,
        }

    def mission_disarm(self, mission_id: str = "") -> dict[str, Any]:
        return {"ok": True}

    def agent_status(self) -> dict[str, Any]:
        return {"ok": True, "offline": True, "active": "lab_passive_watch", "vault": 2}

    def agent_auto(self, mission_id: str, on: bool = True) -> dict[str, Any]:
        return {"ok": True, "id": mission_id, "autonomous": on, "offline": on}

    def agent_offline(self, on: bool = True) -> dict[str, Any]:
        return {"ok": True, "offline": on}

    def vault_tail(self, n: int = 8) -> dict[str, Any]:
        return {
            "ok": True,
            "count": 1,
            "events": [{"mission": "lab_passive_watch", "kind": "rx", "pulses": 4}],
        }


def test_occupancy_labels() -> None:
    assert occupancy_from_activity(0, -100, dwell_ms=400) == "quiet"
    assert occupancy_from_activity(50, -50, dwell_ms=100) in ("medium", "high")


def test_package_rx() -> None:
    p = ObservationPackager(device_ctx={"version": "3.2.0"})
    obs = p.package_rx(
        {"ok": True, "freq_hz": 433920000, "ms": 200, "pulses": 12, "rssi": -65, "sim": True, "ts_ms": 9}
    )
    assert obs["schema"] == OBSERVATION_SCHEMA_ID
    assert obs["kind"] == ObservationKind.RX_SNAPSHOT.value
    assert obs["activity"]["pulses"] == 12
    assert obs["activity"]["occupancy"] in ("quiet", "low", "medium", "high", "unknown")
    assert obs["safety"]["tx"] is False
    assert "narrative" in obs
    assert obs["timestamps"]["device_mono_ms"] == 9


def test_package_spectrum() -> None:
    p = ObservationPackager()
    obs = p.package_spectrum(
        {
            "ok": True,
            "bands": [
                {"freq_hz": 315000000, "pulses": 2, "rssi": -80},
                {"freq_hz": 433920000, "pulses": 40, "rssi": -55},
            ],
        },
        request={"ms": 400, "settle_ms": 2000},
    )
    assert obs["kind"] == ObservationKind.SPECTRUM_SCAN.value
    assert len(obs["spectrum"]["bands"]) == 2
    assert obs["spectrum"]["hottest"]["freq_hz"] == 433920000


def test_store_recent(tmp_path: Path) -> None:
    store = ObservationStore(root=tmp_path, persist=True)
    p = ObservationPackager()
    o = p.package_rx({"ok": True, "freq_hz": 433920000, "ms": 100, "pulses": 1, "rssi": -90})
    store.append(o)
    store.audit("unit_test", {"x": 1})
    assert len(store.recent(5)) == 1
    s = store.summarize_recent(5)
    assert s["count"] == 1
    assert (tmp_path / "observations" / "recent.jsonl").exists()


def test_tool_dispatcher_mock(tmp_path: Path) -> None:
    mock = MockClient()
    store = ObservationStore(root=tmp_path, persist=False)
    d = ToolDispatcher(client=mock, store=store, auto_edu=True)  # type: ignore[arg-type]
    schema = d.dispatch("get_observation_schema")
    assert schema["ok"] is True
    st = d.dispatch("get_device_status", {"probe_radio": True})
    assert st["ok"] is True
    assert st["result"]["kind"] == "device_status"
    rx = d.dispatch("observe_rx", {"freq_hz": 433920000, "ms": 200})
    assert rx["ok"] is True
    assert rx["result"]["kind"] == "rx_snapshot"
    assert mock.edu_calls >= 1
    sp = d.dispatch("observe_spectrum", {"freqs_hz": [315000000, 433920000], "ms": 100, "settle_ms": 50})
    assert sp["ok"] is True
    recent = d.dispatch("get_recent_activity", {"limit": 10})
    assert recent["ok"] is True
    assert recent["summary"]["kind"] == "activity_summary"
    d.close()


def test_openai_tool_definitions() -> None:
    tools = tools_openai_format()
    names = {t["function"]["name"] for t in tools}
    assert "observe_rx" in names
    assert "observe_spectrum" in names
    assert "start_monitor" in names
    assert "get_recent_activity" in names
    assert "run_passive_mission" in names
    assert "list_missions" in names
    assert len(TOOL_DEFINITIONS) >= 10
    desc = schema_description()
    assert desc["schema"] == OBSERVATION_SCHEMA_ID


def test_run_passive_mission_tool(tmp_path: Path) -> None:
    mock = MockClient()
    store = ObservationStore(root=tmp_path, persist=False)
    d = ToolDispatcher(client=mock, store=store)  # type: ignore[arg-type]
    r = d.dispatch("run_passive_mission", {"mission_id": "lab_passive_433", "steps": 4})
    assert r["ok"] is True
    assert r["safety"]["tx"] is False
    bad = d.dispatch("run_passive_mission", {"mission_id": "evil_tx", "steps": 1})
    assert bad["ok"] is False
    off = d.dispatch("start_offline_agent", {"mission_id": "lab_passive_watch"})
    assert off["ok"] is True
    vt = d.dispatch("get_vault_tail", {"n": 4})
    assert vt["ok"] is True
    d.dispatch("stop_offline_agent")
    d.close()


def test_monitor_chunk_with_mock(tmp_path: Path) -> None:
    mock = MockClient()
    store = ObservationStore(root=tmp_path, persist=False)
    d = ToolDispatcher(client=mock, store=store)  # type: ignore[arg-type]
    start = d.dispatch(
        "start_monitor",
        {"freqs_hz": [433920000], "dwell_ms": 50, "interval_ms": 100, "chunk_size": 2},
    )
    assert start["ok"] is True
    chunk = d.dispatch("get_monitor_chunk", {"wait_ms": 3000})
    assert chunk["ok"] is True
    # may need a moment; retry once
    if chunk.get("result") is None:
        chunk = d.dispatch("get_monitor_chunk", {"wait_ms": 3000})
    assert chunk.get("result") is not None
    assert chunk["result"]["kind"] == "monitor_chunk"
    stop = d.dispatch("stop_monitor")
    assert stop["ok"] is True
    d.close()
