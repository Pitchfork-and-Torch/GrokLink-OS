#!/usr/bin/env python3
"""Ingest lab captures, session JSON, and operator notes into the learning store.

Adaptive loop:
- Sessions → sessions/*.json + learnings_index
- Captures → pulse/freq stats into band_notes + capture index
- Freeform notes → notes/*.md + index

Never auto-TX. Never stores secrets (strips common key patterns).
Authorized research data only.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import re
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Optional

from paths_util import (
    band_notes_path,
    learn_root,
    learnings_index_path,
)


SECRET_RE = re.compile(
    r"(?i)(api[_-]?key|secret|token|password|authorization)\s*[:=]\s*\S+"
)


def _utc_now() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat()


def _scrub(text: str) -> str:
    return SECRET_RE.sub(r"\1=[REDACTED]", text)


def _load_index() -> dict[str, Any]:
    p = learnings_index_path()
    if not p.exists():
        return {"schema": 1, "entries": [], "updated_at": None}
    try:
        return json.loads(p.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {"schema": 1, "entries": [], "updated_at": None}


def _save_index(idx: dict[str, Any]) -> None:
    idx["updated_at"] = _utc_now()
    learnings_index_path().write_text(json.dumps(idx, indent=2), encoding="utf-8")


def _load_bands() -> dict[str, Any]:
    p = band_notes_path()
    if not p.exists():
        return {"schema": 1, "bands": {}, "updated_at": None}
    try:
        return json.loads(p.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {"schema": 1, "bands": {}, "updated_at": None}


def _save_bands(bands: dict[str, Any]) -> None:
    bands["updated_at"] = _utc_now()
    band_notes_path().write_text(json.dumps(bands, indent=2), encoding="utf-8")


def _sha16(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()[:16]


def _freq_bucket_hz(freq: int) -> str:
    # 100 kHz buckets for lab notes
    return str(int(round(freq / 100_000) * 100_000))


def ingest_session(path: Path, idx: dict[str, Any]) -> dict[str, Any]:
    raw = path.read_text(encoding="utf-8", errors="replace")
    raw = _scrub(raw)
    try:
        data = json.loads(raw)
    except json.JSONDecodeError:
        data = {"raw_text": raw[:5000]}
    dest = learn_root() / "sessions" / f"{path.stem}_{_sha16(raw.encode())}.json"
    payload = {
        "ingested_at": _utc_now(),
        "source": str(path),
        "data": data,
    }
    dest.write_text(json.dumps(payload, indent=2, default=str), encoding="utf-8")
    entry = {
        "type": "session",
        "source": str(path),
        "stored": str(dest),
        "at": _utc_now(),
        "keys": list(data.keys()) if isinstance(data, dict) else [],
    }
    idx["entries"].append(entry)
    return entry


def ingest_capture(path: Path, idx: dict[str, Any], bands: dict[str, Any]) -> dict[str, Any]:
    text = _scrub(path.read_text(encoding="utf-8", errors="replace"))
    lines = [ln for ln in text.splitlines() if ln.strip()]
    records: list[dict[str, Any]] = []
    for ln in lines:
        try:
            obj = json.loads(ln)
            if isinstance(obj, dict):
                records.append(obj)
        except json.JSONDecodeError:
            continue
    # Also accept single JSON object files
    if not records and text.strip().startswith("{"):
        try:
            obj = json.loads(text)
            if isinstance(obj, dict):
                records = [obj]
            elif isinstance(obj, list):
                records = [x for x in obj if isinstance(x, dict)]
        except json.JSONDecodeError:
            pass

    freqs: list[int] = []
    rssi_vals: list[float] = []
    pulse_count = 0
    for r in records:
        for k in ("freq_hz", "frequency", "freq", "frequency_hz"):
            if k in r and r[k] is not None:
                try:
                    freqs.append(int(r[k]))
                except (TypeError, ValueError):
                    pass
        for k in ("rssi", "rssi_dbm", "power_dbm"):
            if k in r and r[k] is not None:
                try:
                    rssi_vals.append(float(r[k]))
                except (TypeError, ValueError):
                    pass
        if "pulses" in r and isinstance(r["pulses"], list):
            pulse_count += len(r["pulses"])
        elif "pulse_count" in r:
            try:
                pulse_count += int(r["pulse_count"])
            except (TypeError, ValueError):
                pass

    stats = {
        "records": len(records),
        "lines": len(lines),
        "freqs_hz": sorted(set(freqs)),
        "pulse_count": pulse_count,
        "rssi_min": min(rssi_vals) if rssi_vals else None,
        "rssi_max": max(rssi_vals) if rssi_vals else None,
        "rssi_mean": (sum(rssi_vals) / len(rssi_vals)) if rssi_vals else None,
    }

    dest = learn_root() / "captures" / f"{path.stem}_{_sha16(text.encode())}.json"
    payload = {
        "ingested_at": _utc_now(),
        "source": str(path),
        "stats": stats,
        "sample_records": records[:5],
        "authorized_lab_only": True,
    }
    dest.write_text(json.dumps(payload, indent=2, default=str), encoding="utf-8")

    # Update band notes
    for f in stats["freqs_hz"]:
        key = _freq_bucket_hz(f)
        b = bands["bands"].setdefault(
            key,
            {
                "freq_bucket_hz": int(key),
                "hits": 0,
                "last_seen": None,
                "notes": [],
                "rssi_samples": [],
            },
        )
        b["hits"] += 1
        b["last_seen"] = _utc_now()
        if stats["rssi_mean"] is not None:
            samples = b.setdefault("rssi_samples", [])
            samples.append(stats["rssi_mean"])
            b["rssi_samples"] = samples[-50:]  # cap

    entry = {
        "type": "capture",
        "source": str(path),
        "stored": str(dest),
        "at": _utc_now(),
        "stats": stats,
    }
    idx["entries"].append(entry)
    return entry


def ingest_note(text: str, idx: dict[str, Any], title: Optional[str] = None) -> dict[str, Any]:
    text = _scrub(text.strip())
    ts = _utc_now().replace(":", "")
    name = title or f"note_{ts}"
    safe = re.sub(r"[^a-zA-Z0-9._-]+", "_", name)[:80]
    dest = learn_root() / "notes" / f"{safe}.md"
    dest.write_text(
        f"# {name}\n\n_ingested: {_utc_now()}_\n\n{text}\n",
        encoding="utf-8",
    )
    entry = {
        "type": "note",
        "title": name,
        "stored": str(dest),
        "at": _utc_now(),
        "chars": len(text),
    }
    idx["entries"].append(entry)
    return entry


def ingest_observations(path: Path, idx: dict[str, Any], bands: dict[str, Any]) -> dict[str, Any]:
    """Ingest observation JSONL (from bridge ObservationStore) into band notes."""
    text = _scrub(path.read_text(encoding="utf-8", errors="replace"))
    records: list[dict[str, Any]] = []
    for ln in text.splitlines():
        ln = ln.strip()
        if not ln:
            continue
        try:
            obj = json.loads(ln)
            if isinstance(obj, dict):
                records.append(obj)
        except json.JSONDecodeError:
            continue
    if not records and text.strip().startswith("{"):
        try:
            obj = json.loads(text)
            if isinstance(obj, dict):
                records = [obj]
        except json.JSONDecodeError:
            pass

    occ_counts: dict[str, int] = {}
    freqs: list[int] = []
    for r in records:
        rf = r.get("rf") or {}
        act = r.get("activity") or {}
        freq = rf.get("freq_hz")
        if freq is None and r.get("spectrum"):
            hottest = (r.get("spectrum") or {}).get("hottest") or {}
            freq = hottest.get("freq_hz")
        if freq is not None:
            try:
                fi = int(freq)
                freqs.append(fi)
                key = _freq_bucket_hz(fi)
                b = bands["bands"].setdefault(
                    key,
                    {
                        "freq_bucket_hz": int(key),
                        "hits": 0,
                        "last_seen": None,
                        "notes": [],
                        "rssi_samples": [],
                        "occupancy_samples": [],
                    },
                )
                b["hits"] += 1
                b["last_seen"] = _utc_now()
                occ = act.get("occupancy")
                if occ:
                    occ_counts[str(occ)] = occ_counts.get(str(occ), 0) + 1
                    samples = b.setdefault("occupancy_samples", [])
                    samples.append(str(occ))
                    b["occupancy_samples"] = samples[-50:]
                rssi = act.get("rssi_dbm")
                if rssi is not None:
                    try:
                        rs = b.setdefault("rssi_samples", [])
                        rs.append(float(rssi))
                        b["rssi_samples"] = rs[-50:]
                    except (TypeError, ValueError):
                        pass
            except (TypeError, ValueError):
                pass

    dest = learn_root() / "captures" / f"obs_{path.stem}_{_sha16(text.encode())}.json"
    payload = {
        "ingested_at": _utc_now(),
        "source": str(path),
        "type": "observations",
        "count": len(records),
        "freqs_hz": sorted(set(freqs)),
        "occupancy_counts": occ_counts,
        "sample_narratives": [
            r.get("narrative") for r in records[:5] if isinstance(r.get("narrative"), str)
        ],
        "authorized_lab_only": True,
    }
    dest.write_text(json.dumps(payload, indent=2, default=str), encoding="utf-8")
    entry = {
        "type": "observations",
        "source": str(path),
        "stored": str(dest),
        "at": _utc_now(),
        "count": len(records),
        "occupancy_counts": occ_counts,
    }
    idx["entries"].append(entry)
    return entry


def summarize() -> dict[str, Any]:
    idx = _load_index()
    bands = _load_bands()
    entries = idx.get("entries") or []
    by_type: dict[str, int] = {}
    for e in entries:
        t = e.get("type", "unknown")
        by_type[t] = by_type.get(t, 0) + 1
    hot = sorted(
        (bands.get("bands") or {}).values(),
        key=lambda b: b.get("hits", 0),
        reverse=True,
    )[:10]
    return {
        "learn_dir": str(learn_root()),
        "entry_count": len(entries),
        "by_type": by_type,
        "band_buckets": len(bands.get("bands") or {}),
        "hottest_bands": [
            {
                "freq_bucket_hz": h.get("freq_bucket_hz"),
                "hits": h.get("hits"),
                "last_seen": h.get("last_seen"),
            }
            for h in hot
        ],
        "updated_at": idx.get("updated_at"),
    }


def main() -> int:
    ap = argparse.ArgumentParser(description="Learn from GrokLink lab data (no TX)")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--session", type=Path, help="Session JSON to ingest")
    ap.add_argument("--capture", type=Path, help="Capture jsonl/json to ingest")
    ap.add_argument(
        "--observations",
        type=Path,
        help="Observation JSONL (bridge ObservationStore recent.jsonl)",
    )
    ap.add_argument("--note", type=str, help="Freeform operator note")
    ap.add_argument("--title", type=str, default=None, help="Note title")
    ap.add_argument("--summary", action="store_true", help="Print learning summary")
    args = ap.parse_args()

    if args.dry_run:
        print(
            json.dumps(
                {
                    "dry_run": True,
                    "learn_dir": str(learn_root()),
                    "actions": {
                        "session": str(args.session) if args.session else None,
                        "capture": str(args.capture) if args.capture else None,
                        "observations": str(args.observations) if args.observations else None,
                        "note": bool(args.note),
                        "summary": args.summary,
                    },
                },
                indent=2,
            )
        )
        return 0

    if not any([args.session, args.capture, args.observations, args.note, args.summary]):
        ap.print_help()
        return 2

    idx = _load_index()
    bands = _load_bands()
    results: list[dict[str, Any]] = []

    if args.session:
        if not args.session.exists():
            print(json.dumps({"ok": False, "error": f"missing session {args.session}"}))
            return 1
        results.append(ingest_session(args.session, idx))
    if args.capture:
        if not args.capture.exists():
            print(json.dumps({"ok": False, "error": f"missing capture {args.capture}"}))
            return 1
        results.append(ingest_capture(args.capture, idx, bands))
        _save_bands(bands)
    if args.observations:
        if not args.observations.exists():
            print(json.dumps({"ok": False, "error": f"missing observations {args.observations}"}))
            return 1
        results.append(ingest_observations(args.observations, idx, bands))
        _save_bands(bands)
    if args.note:
        results.append(ingest_note(args.note, idx, title=args.title))

    # Cap index growth
    if len(idx["entries"]) > 500:
        idx["entries"] = idx["entries"][-500:]
    if results:
        _save_index(idx)

    out: dict[str, Any] = {"ok": True, "ingested": results}
    if args.summary or results:
        out["summary"] = summarize()
    print(json.dumps(out, indent=2, default=str))
    return 0


if __name__ == "__main__":
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    raise SystemExit(main())
