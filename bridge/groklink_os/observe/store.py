"""Local observation history and request audit (operator machine only)."""

from __future__ import annotations

import json
import os
import threading
from collections import deque
from pathlib import Path
from typing import Any, Deque, Optional

from groklink_os.observe.schema import utc_now_iso


def default_store_root() -> Path:
    env = os.environ.get("GROKLINK_OS_LEARN_DIR") or os.environ.get("GLK_OBSERVE_DIR")
    if env:
        root = Path(env).expanduser()
    else:
        root = Path.home() / ".grok" / "state" / "groklink-os"
    root.mkdir(parents=True, exist_ok=True)
    (root / "observations").mkdir(parents=True, exist_ok=True)
    (root / "audit").mkdir(parents=True, exist_ok=True)
    return root


class ObservationStore:
    """Ring buffer + optional JSONL persistence for observations and audits."""

    def __init__(
        self,
        root: Optional[Path] = None,
        *,
        max_memory: int = 256,
        persist: bool = True,
    ) -> None:
        self.root = root or default_store_root()
        self.max_memory = max_memory
        self.persist = persist
        self._obs: Deque[dict[str, Any]] = deque(maxlen=max_memory)
        self._lock = threading.RLock()
        self._obs_path = self.root / "observations" / "recent.jsonl"
        self._audit_path = self.root / "audit" / "observe_audit.jsonl"

    def append(self, observation: dict[str, Any]) -> None:
        with self._lock:
            self._obs.append(observation)
            if self.persist:
                self._append_jsonl(self._obs_path, observation)

    def audit(self, event: str, detail: Optional[dict[str, Any]] = None) -> None:
        row = {
            "ts": utc_now_iso(),
            "event": event,
            "detail": detail or {},
            "safety": {"tx": False, "path": "observe_audit"},
        }
        with self._lock:
            if self.persist:
                self._append_jsonl(self._audit_path, row)

    def recent(self, limit: int = 20, *, kind: Optional[str] = None) -> list[dict[str, Any]]:
        with self._lock:
            items = list(self._obs)
        if kind:
            items = [x for x in items if x.get("kind") == kind]
        if limit <= 0:
            return items
        return items[-limit:]

    def summarize_recent(self, limit: int = 50) -> dict[str, Any]:
        items = self.recent(limit)
        by_band: dict[str, int] = {}
        occupancy_counts: dict[str, int] = {}
        hottest: list[dict[str, Any]] = []
        for o in items:
            rf = o.get("rf") or {}
            act = o.get("activity") or {}
            label = rf.get("band_label") or str(rf.get("freq_hz") or "unknown")
            by_band[label] = by_band.get(label, 0) + 1
            occ = act.get("occupancy") or "unknown"
            occupancy_counts[occ] = occupancy_counts.get(occ, 0) + 1
            score = act.get("energy_score")
            if score is not None:
                hottest.append(
                    {
                        "freq_hz": rf.get("freq_hz"),
                        "energy_score": score,
                        "occupancy": occ,
                        "utc": (o.get("timestamps") or {}).get("utc"),
                        "observation_id": o.get("observation_id"),
                    }
                )
        hottest.sort(key=lambda x: float(x.get("energy_score") or 0), reverse=True)
        return {
            "count": len(items),
            "by_band": by_band,
            "occupancy_counts": occupancy_counts,
            "hottest": hottest[:10],
            "latest_utc": (items[-1].get("timestamps") or {}).get("utc") if items else None,
        }

    @staticmethod
    def _append_jsonl(path: Path, obj: dict[str, Any]) -> None:
        path.parent.mkdir(parents=True, exist_ok=True)
        with path.open("a", encoding="utf-8") as f:
            f.write(json.dumps(obj, separators=(",", ":"), default=str) + "\n")
