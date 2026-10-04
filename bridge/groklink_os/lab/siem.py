"""SIEM-friendly NDJSON export for MedSec engagement evidence."""

from __future__ import annotations

import json
import time
from pathlib import Path
from typing import Any, Optional

from groklink_os.lab.engagement import load_engagement, stamp_record


def observation_to_siem_event(row: dict[str, Any], eng: Optional[dict[str, Any]] = None) -> dict[str, Any]:
    eng = eng if eng is not None else load_engagement()
    return stamp_record(
        {
            "event_type": "groklink.medsec.observation",
            "vendor": "Pitchfork-and-Torch",
            "product": "GrokLink OS",
            "severity": "research",
            "kind": row.get("kind") or "observation",
            "freq_hz": row.get("freq_hz"),
            "pulses": row.get("pulses") or row.get("last_pulses"),
            "narrative": (row.get("narrative") or "")[:500],
            "raw_ts": row.get("ts") or row.get("timestamp") or row.get("ts_ms"),
            "ingest_ts": time.time(),
            "never_auto_tx": True,
            "clinical_use": False,
        },
        eng,
    )


def export_siem_ndjson(history_path: Path, out: Path, *, limit: int = 10000) -> dict[str, Any]:
    history_path = Path(history_path)
    out = Path(out)
    out.parent.mkdir(parents=True, exist_ok=True)
    eng = load_engagement()
    n = 0
    lines: list[str] = []
    if history_path.exists():
        for line in history_path.read_text(encoding="utf-8", errors="replace").splitlines():
            s = line.strip()
            if not s.startswith("{"):
                continue
            try:
                row = json.loads(s)
            except json.JSONDecodeError:
                continue
            if not isinstance(row, dict):
                continue
            lines.append(json.dumps(observation_to_siem_event(row, eng), ensure_ascii=True))
            n += 1
            if n >= limit:
                break
    out.write_text("\n".join(lines) + ("\n" if lines else ""), encoding="utf-8")
    return {
        "ok": True,
        "out": str(out),
        "count": n,
        "format": "ndjson",
        "not_medical_device": True,
        "clinical_use": False,
    }
