"""Host-side band calibration store for Signal Cognition (v3.5).

Stores noise-floor and baseline pulse-rate per band_label. Local only.
Never publishes. Never enables TX.
"""

from __future__ import annotations

import json
import time
from pathlib import Path
from typing import Any, Optional

from groklink_os.observe.schema import band_label


class CalibrationStore:
    """Rolling baselines keyed by band_label (e.g. 433-class)."""

    def __init__(self, path: Optional[Path] = None) -> None:
        if path is None:
            root = Path.home() / ".grok" / "state" / "groklink-os"
            root.mkdir(parents=True, exist_ok=True)
            path = root / "calibration.json"
        self.path = path
        self._bands: dict[str, dict[str, Any]] = {}
        self._load()

    def _load(self) -> None:
        if not self.path.exists():
            return
        try:
            data = json.loads(self.path.read_text(encoding="utf-8"))
            if isinstance(data, dict) and isinstance(data.get("bands"), dict):
                self._bands = data["bands"]
        except (OSError, json.JSONDecodeError):
            self._bands = {}

    def save(self) -> None:
        payload = {
            "schema": "groklink.calibration.v1",
            "updated_mono_ms": int(time.monotonic() * 1000),
            "bands": self._bands,
            "safety": {"tx": False, "note": "Host baselines only; not device firmware state."},
        }
        self.path.parent.mkdir(parents=True, exist_ok=True)
        self.path.write_text(json.dumps(payload, indent=2), encoding="utf-8")

    def get(self, freq_hz: int) -> Optional[dict[str, Any]]:
        return self._bands.get(band_label(freq_hz))

    def get_by_label(self, label: str) -> Optional[dict[str, Any]]:
        return self._bands.get(label)

    def update_from_sample(
        self,
        *,
        freq_hz: int,
        rssi_dbm: Optional[int],
        pulse_rate_hz: float,
        method: str = "observe_noise_floor",
        alpha: float = 0.35,
    ) -> dict[str, Any]:
        """EMA update of noise floor and baseline pulse rate."""
        label = band_label(freq_hz)
        prev = dict(self._bands.get(label) or {})
        now = int(time.monotonic() * 1000)
        if rssi_dbm is not None:
            if prev.get("noise_floor_dbm") is None:
                nf = float(rssi_dbm)
            else:
                nf = (1.0 - alpha) * float(prev["noise_floor_dbm"]) + alpha * float(rssi_dbm)
            # noise floor tracks quieter (more negative) with slight bias
            nf = min(nf, float(rssi_dbm) + 1.0)
            prev["noise_floor_dbm"] = round(nf, 2)
        if prev.get("baseline_pulse_rate_hz") is None:
            prev["baseline_pulse_rate_hz"] = round(float(pulse_rate_hz), 3)
        else:
            prev["baseline_pulse_rate_hz"] = round(
                (1.0 - alpha) * float(prev["baseline_pulse_rate_hz"]) + alpha * float(pulse_rate_hz),
                3,
            )
        prev["method"] = method
        prev["freq_hz_last"] = int(freq_hz)
        prev["updated_mono_ms"] = now
        prev["sample_count"] = int(prev.get("sample_count") or 0) + 1
        self._bands[label] = prev
        self.save()
        return {"band_label": label, **prev}

    def as_dict(self) -> dict[str, Any]:
        return {
            "bands": dict(self._bands),
            "count": len(self._bands),
            "path_note": "local host store only",
            "safety": {"tx": False},
        }

    def calibration_block_for(self, freq_hz: int) -> dict[str, Any]:
        row = self.get(freq_hz)
        now = int(time.monotonic() * 1000)
        if not row:
            return {
                "noise_floor_dbm": None,
                "baseline_pulse_rate_hz": None,
                "snr_est_db": None,
                "method": "none",
                "age_ms": None,
                "band_label": band_label(freq_hz),
            }
        age = None
        if row.get("updated_mono_ms") is not None:
            age = max(0, now - int(row["updated_mono_ms"]))
        return {
            "noise_floor_dbm": row.get("noise_floor_dbm"),
            "baseline_pulse_rate_hz": row.get("baseline_pulse_rate_hz"),
            "snr_est_db": None,  # filled by packager with current rssi
            "method": row.get("method") or "host_rolling",
            "age_ms": age,
            "band_label": band_label(freq_hz),
            "sample_count": row.get("sample_count"),
        }
