"""History / research exports. Not for EHR or clinical care."""

from __future__ import annotations

import csv
import json
import time
from pathlib import Path
from typing import Any, Optional

from groklink_os.lab.engagement import load_engagement, stamp_record


def _load_jsonl(path: Path) -> list[dict[str, Any]]:
    rows: list[dict[str, Any]] = []
    if not path.exists():
        return rows
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        s = line.strip()
        if not s.startswith("{"):
            continue
        try:
            row = json.loads(s)
        except json.JSONDecodeError:
            continue
        if isinstance(row, dict):
            rows.append(row)
    return rows


def export_history_json(history_path: Path, out: Path) -> dict[str, Any]:
    rows = _load_jsonl(Path(history_path))
    eng = load_engagement()
    payload = stamp_record(
        {
            "ok": True,
            "count": len(rows),
            "rows": rows,
            "exported_ts": time.time(),
            "source": str(history_path),
        },
        eng,
    )
    out = Path(out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    return {"ok": True, "out": str(out), "count": len(rows)}


def export_history_csv(history_path: Path, out: Path) -> dict[str, Any]:
    rows = _load_jsonl(Path(history_path))
    out = Path(out)
    out.parent.mkdir(parents=True, exist_ok=True)
    fields = ["ts", "kind", "freq_hz", "pulses", "narrative", "engagement_id", "operator_id"]
    eng = load_engagement()
    with out.open("w", encoding="utf-8", newline="") as f:
        w = csv.DictWriter(f, fieldnames=fields, extrasaction="ignore")
        w.writeheader()
        for r in rows:
            w.writerow(
                {
                    "ts": r.get("ts") or r.get("timestamp") or "",
                    "kind": r.get("kind") or "",
                    "freq_hz": r.get("freq_hz") or "",
                    "pulses": r.get("pulses") or r.get("last_pulses") or "",
                    "narrative": (r.get("narrative") or "")[:200],
                    "engagement_id": eng.get("engagement_id", ""),
                    "operator_id": eng.get("operator_id", ""),
                }
            )
    return {"ok": True, "out": str(out), "count": len(rows), "not_medical_device": True}


def export_research_bundle(
    history_path: Path,
    out: Path,
    *,
    title: str = "GrokLink research observation bundle",
) -> dict[str, Any]:
    """Experimental research-shaped bundle. NOT FHIR clinical / not EHR."""
    rows = _load_jsonl(Path(history_path))
    eng = load_engagement()
    bundle = stamp_record(
        {
            "resourceType": "ResearchStudyObservationBundle",
            "status": "experimental-research-only",
            "title": title,
            "ok": True,
            "count": len(rows),
            "observations": rows[:500],
            "clinical_use": False,
            "ehr_integration": False,
            "note": (
                "Experimental research-shaped export only. "
                "Not HL7/FHIR clinical, not for care decisions, not a medical device."
            ),
            "exported_ts": time.time(),
            "source": str(history_path),
        },
        eng,
    )
    out = Path(out)
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text(json.dumps(bundle, indent=2) + "\n", encoding="utf-8")
    return {"ok": True, "out": str(out), "count": len(rows), "not_medical_device": True}
