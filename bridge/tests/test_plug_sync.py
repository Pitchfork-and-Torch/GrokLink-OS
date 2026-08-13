"""Unit tests for plug-sync research ingest (no hardware)."""

from __future__ import annotations

import json
from pathlib import Path

from groklink_os.research.plug_sync import _analyze_vault, ingest_unplugged_lessons


def test_analyze_vault_lessons() -> None:
    events = [
        {"ts_ms": 1, "mission": "lab_passive_watch", "kind": "rx", "pulses": 10, "infer": 0, "score": 0.1},
        {"ts_ms": 2, "mission": "lab_passive_watch", "kind": "rx", "pulses": 50, "infer": 0, "score": 0.2},
        {"ts_ms": 3, "mission": "lab_passive_watch", "kind": "infer", "pulses": 50, "infer": 1, "score": 0.5},
        {"ts_ms": 4, "mission": "lab_passive_watch", "kind": "done", "pulses": 50, "infer": 1, "score": 0.5},
    ]
    a = _analyze_vault(events)
    assert a["event_count"] == 4
    assert a["by_kind"]["rx"] == 2
    assert a["pulse_stats"]["max"] == 50
    assert any("Recovered 4" in L for L in a["lessons"])


def test_analyze_empty() -> None:
    a = _analyze_vault([])
    assert a["event_count"] == 0
    assert any("empty" in L.lower() for L in a["lessons"])


class _Mock:
    def edu_ack(self, phrase: str = "") -> dict:
        return {"ok": True, "edu": True}

    def ping(self) -> dict:
        return {"ok": True, "version": "3.6.1", "api": 5}

    def status(self) -> dict:
        return {"ok": True, "version": "3.6.1", "missions": 4, "edu": True}

    def agent_status(self) -> dict:
        return {"ok": True, "offline": True, "active": "lab_passive_watch", "cycles": 3, "vault": 4}

    def vault_tail(self, n: int = 16) -> dict:
        return {
            "ok": True,
            "count": 2,
            "events": [
                {"ts_ms": 10, "mission": "lab_passive_watch", "kind": "rx", "pulses": 12, "infer": 0, "score": 0.0},
                {"ts_ms": 20, "mission": "lab_passive_watch", "kind": "done", "pulses": 12, "infer": 0, "score": 0.1},
            ],
        }

    def vault_clear(self) -> dict:
        return {"ok": True, "cleared": True}

    def close(self) -> None:
        return None


def test_ingest_with_mock(tmp_path: Path, monkeypatch) -> None:
    monkeypatch.setenv("GROKLINK_OS_LEARN_DIR", str(tmp_path))
    r = ingest_unplugged_lessons(client=_Mock(), clear_vault=True)
    assert r.ok
    assert r.vault_cleared
    assert Path(r.path).exists()
    assert Path(r.note_path).exists()
    data = json.loads(Path(r.path).read_text(encoding="utf-8"))
    assert data["analysis"]["event_count"] == 2
    assert (tmp_path / "research" / "plug_sync_index.json").exists()
    assert (tmp_path / "research" / "lessons_learned_summary.json").exists()
