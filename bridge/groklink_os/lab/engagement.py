"""Engagement context for MedSec / lab evidence (PC-side only).

NOT a medical device. No PHI. Operator and engagement IDs are lab labels only.
"""

from __future__ import annotations

import json
import os
import time
from pathlib import Path
from typing import Any, Optional

DEFAULT_PATH = Path.home() / ".groklink" / "engagement.json"

DISCLAIMER = (
    "Educational authorized research only. Not a medical device. "
    "Not for diagnosis, treatment, or patient-connected care."
)


def engagement_path() -> Path:
    env = os.environ.get("GROKLINK_ENGAGEMENT_FILE") or os.environ.get("GLK_ENGAGEMENT_FILE")
    return Path(env) if env else DEFAULT_PATH


def load_engagement(path: Optional[Path] = None) -> dict[str, Any]:
    p = Path(path) if path else engagement_path()
    if not p.exists():
        return {
            "operator_id": "",
            "engagement_id": "",
            "site_label": "",
            "roe_ack": False,
            "profile": "default",
            "not_medical_device": True,
            "disclaimer": DISCLAIMER,
        }
    try:
        data = json.loads(p.read_text(encoding="utf-8"))
    except (json.JSONDecodeError, OSError):
        data = {}
    data.setdefault("not_medical_device", True)
    data.setdefault("disclaimer", DISCLAIMER)
    data.setdefault("profile", "default")
    return data


def save_engagement(
    *,
    operator_id: str,
    engagement_id: str,
    site_label: str = "",
    roe_ack: bool = False,
    profile: str = "medsec-strict",
    path: Optional[Path] = None,
    extra: Optional[dict[str, Any]] = None,
) -> Path:
    from groklink_os.lab.phi import assert_no_phi

    assert_no_phi(
        operator_id,
        engagement_id,
        site_label,
        profile,
        labels=["operator_id", "engagement_id", "site_label", "profile"],
    )
    p = Path(path) if path else engagement_path()
    p.parent.mkdir(parents=True, exist_ok=True)
    payload = {
        "operator_id": operator_id.strip(),
        "engagement_id": engagement_id.strip(),
        "site_label": site_label.strip(),
        "roe_ack": bool(roe_ack),
        "profile": profile.strip() or "default",
        "updated_ts": time.time(),
        "not_medical_device": True,
        "disclaimer": DISCLAIMER,
    }
    if extra:
        payload.update(extra)
    p.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    return p


def stamp_record(record: dict[str, Any], engagement: Optional[dict[str, Any]] = None) -> dict[str, Any]:
    eng = engagement if engagement is not None else load_engagement()
    out = dict(record)
    out["operator_id"] = eng.get("operator_id") or out.get("operator_id") or ""
    out["engagement_id"] = eng.get("engagement_id") or out.get("engagement_id") or ""
    out["site_label"] = eng.get("site_label") or out.get("site_label") or ""
    out["roe_ack"] = bool(eng.get("roe_ack", False))
    out["profile"] = eng.get("profile") or out.get("profile") or "default"
    out["not_medical_device"] = True
    out["disclaimer"] = eng.get("disclaimer") or DISCLAIMER
    out["stamped_ts"] = time.time()
    return out


def stamp_audit_jsonl(
    inp: Path,
    out: Path,
    *,
    engagement: Optional[dict[str, Any]] = None,
) -> dict[str, Any]:
    eng = engagement if engagement is not None else load_engagement()
    inp = Path(inp)
    out = Path(out)
    out.parent.mkdir(parents=True, exist_ok=True)
    n_in = 0
    n_out = 0
    lines_out: list[str] = []
    text = inp.read_text(encoding="utf-8", errors="replace") if inp.exists() else ""
    for line in text.splitlines():
        s = line.strip()
        if not s:
            continue
        n_in += 1
        if not (s.startswith("{") and s.endswith("}")):
            continue
        try:
            row = json.loads(s)
        except json.JSONDecodeError:
            continue
        if not isinstance(row, dict):
            continue
        lines_out.append(json.dumps(stamp_record(row, eng), ensure_ascii=True))
        n_out += 1
    out.write_text("\n".join(lines_out) + ("\n" if lines_out else ""), encoding="utf-8")
    return {
        "ok": True,
        "input_lines": n_in,
        "stamped_rows": n_out,
        "out": str(out),
        "engagement_id": eng.get("engagement_id", ""),
        "operator_id": eng.get("operator_id", ""),
        "not_medical_device": True,
    }
