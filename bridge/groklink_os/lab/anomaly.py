"""PC-side RF anomaly scoring from observation history.

Never triggers TX. Research/lab use only. Not a medical device.
Scores are heuristic pulse-edge ratios, not protocol or threat IDs.
"""

from __future__ import annotations

import json
import time
from collections import defaultdict
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


def _pulse_by_freq(rows: list[dict[str, Any]]) -> dict[int, list[float]]:
    by: dict[int, list[float]] = defaultdict(list)
    for r in rows:
        # observation pack shapes vary — be defensive
        freq = r.get("freq_hz") or r.get("frequency_hz")
        pulses = r.get("pulses") or r.get("last_pulses")
        if freq is None:
            act = r.get("activity") or {}
            if isinstance(act, dict):
                freq = act.get("freq_hz") or act.get("hottest_freq_hz")
                pulses = pulses if pulses is not None else act.get("pulses") or act.get("pulse_count")
        stats = r.get("stats") or {}
        if isinstance(stats, dict) and pulses is None:
            pulses = stats.get("pulse_count") or stats.get("pulses")
        if freq is None:
            mission = r.get("mission") or {}
            if isinstance(mission, dict):
                st = mission.get("status") or {}
                if isinstance(st, dict):
                    pulses = pulses if pulses is not None else st.get("last_pulses")
        try:
            f = int(freq) if freq is not None else 0
            p = float(pulses) if pulses is not None else 0.0
        except (TypeError, ValueError):
            continue
        if f > 0:
            by[f].append(p)
    return by


def score_observations_anomaly(
    history_path: Path,
    *,
    hot_ratio: float = 2.5,
    quiet_ratio: float = 0.35,
    engagement: Optional[dict[str, Any]] = None,
) -> dict[str, Any]:
    """Compare recent observations to earlier baseline; score 0-100 (lab only)."""
    rows = _load_jsonl(Path(history_path))
    if len(rows) < 2:
        return stamp_record(
            {
                "ok": False,
                "error": "need_at_least_2_observation_rows",
                "score": 0,
                "flags": [],
                "never_auto_tx": True,
            },
            engagement,
        )

    split = max(1, len(rows) // 2)
    base = _pulse_by_freq(rows[:split])
    recent = _pulse_by_freq(rows[split:])
    if not recent:
        # fallback: last row vs rest
        base = _pulse_by_freq(rows[:-1])
        recent = _pulse_by_freq(rows[-1:])

    flags: list[dict[str, Any]] = []
    for freq, vals in recent.items():
        cur = sum(vals) / max(1, len(vals))
        bvals = base.get(freq) or [0.0]
        bas = sum(bvals) / max(1, len(bvals))
        if bas <= 0:
            ratio = 99.0 if cur > 0 else 1.0
        else:
            ratio = cur / bas
        flag = "normal"
        if ratio >= hot_ratio:
            flag = "hot"
        elif ratio <= quiet_ratio and bas > 0:
            flag = "quiet"
        if flag != "normal":
            flags.append(
                {
                    "freq_hz": freq,
                    "flag": flag,
                    "baseline": round(bas, 3),
                    "current": round(cur, 3),
                    "ratio": round(ratio, 3),
                }
            )

    hot = [f for f in flags if f["flag"] == "hot"]
    quiet = [f for f in flags if f["flag"] == "quiet"]
    score = min(100, len(hot) * 20 + len(quiet) * 5)

    return stamp_record(
        {
            "ok": True,
            "score": score,
            "summary": f"hot={len(hot)} quiet={len(quiet)} rows={len(rows)}",
            "flags": flags,
            "hot_count": len(hot),
            "quiet_count": len(quiet),
            "never_auto_tx": True,
            "not_threat_intel": True,
            "disclaimer": (
                "Heuristic pulse-edge anomaly for authorized lab use only. "
                "Not protocol ID, not attack detection, not a medical device."
            ),
            "scored_ts": time.time(),
            "source": str(history_path),
        },
        engagement if engagement is not None else load_engagement(),
    )


def write_anomaly_report(history_path: Path, out_json: Path, **kwargs: Any) -> Path:
    result = score_observations_anomaly(history_path, **kwargs)
    out_json = Path(out_json)
    out_json.parent.mkdir(parents=True, exist_ok=True)
    out_json.write_text(json.dumps(result, indent=2) + "\n", encoding="utf-8")
    return out_json
