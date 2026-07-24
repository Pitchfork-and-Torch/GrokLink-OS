"""Skill craft: capture → analyze → generate manifest (human approve → deploy)."""

from __future__ import annotations

import json
import re
from pathlib import Path
from typing import Any


def _pulse_stats(path: Path) -> dict[str, Any]:
    pulses: list[int] = []
    if not path.exists():
        return {"count": 0, "mean": 0.0, "max": 0}
    for line in path.read_text(encoding="utf-8", errors="replace").splitlines():
        line = line.strip()
        if not line:
            continue
        try:
            obj = json.loads(line)
        except json.JSONDecodeError:
            m = re.search(r"pulses[\"']?\s*[:=]\s*(-?\d+)", line)
            if m:
                pulses.append(int(m.group(1)))
            continue
        if "pulses" in obj:
            pulses.append(int(obj["pulses"]))
    if not pulses:
        return {"count": 0, "mean": 0.0, "max": 0}
    return {
        "count": len(pulses),
        "mean": sum(pulses) / len(pulses),
        "max": max(pulses),
    }


def craft_skill_from_capture(capture_path: str, out_dir: str) -> str:
    cap = Path(capture_path)
    stats = _pulse_stats(cap)
    skill_id = "crafted_passive_watch"
    dest = Path(out_dir) / skill_id
    dest.mkdir(parents=True, exist_ok=True)
    manifest = {
        "id": skill_id,
        "version": "3.0.0",
        "risk_class": "passive_rx",
        "hw": ["subghz"],
        "description": "Auto-crafted passive watcher from capture stats (draft).",
        "source_capture": str(cap),
        "stats": stats,
        "os": "GrokLink-OS",
    }
    rules = {
        "trigger": {"pulses_gt": max(5, int(stats.get("mean", 0)))},
        "actions": [{"op": "log", "msg": "activity"}, {"op": "infer"}],
        "never_auto_tx": True,
    }
    (dest / "manifest.json").write_text(json.dumps(manifest, indent=2), encoding="utf-8")
    (dest / "rules.json").write_text(json.dumps(rules, indent=2), encoding="utf-8")
    (dest / "README.md").write_text(
        f"# {skill_id}\n\nDraft skill. **Human approval required before deploy.**\n"
        f"Risk: passive_rx. Source: `{cap}`.\n",
        encoding="utf-8",
    )
    return str(dest)
