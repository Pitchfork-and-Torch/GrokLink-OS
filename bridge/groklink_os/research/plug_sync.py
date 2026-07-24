"""Ingest on-device unplugged field lessons when USB reconnects.

Pulls vault / agent / status, documents findings in the local learning store
(~/.grok/state/groklink-os/), never publishes captures, never TX.
"""

from __future__ import annotations

import json
import os
import re
import time
from dataclasses import dataclass, field, asdict
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Optional


def _utc_now() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def _learn_root() -> Path:
    env = os.environ.get("GROKLINK_OS_LEARN_DIR")
    if env:
        p = Path(env).expanduser()
    else:
        p = Path.home() / ".grok" / "state" / "groklink-os"
    for sub in (
        "sessions",
        "captures",
        "notes",
        "drafts",
        "observations",
        "audit",
        "field_trips",
        "research",
    ):
        (p / sub).mkdir(parents=True, exist_ok=True)
    return p


SECRET_RE = re.compile(
    r"(?i)(api[_-]?key|secret|token|password|authorization)\s*[:=]\s*\S+"
)


def _scrub(text: str) -> str:
    return SECRET_RE.sub(r"\1=[REDACTED]", text)


@dataclass
class PlugSyncResult:
    ok: bool
    trip_id: str = ""
    path: str = ""
    note_path: str = ""
    summary: dict[str, Any] = field(default_factory=dict)
    error: Optional[str] = None
    vault_cleared: bool = False
    narrative: str = ""

    def to_dict(self) -> dict[str, Any]:
        return asdict(self)


def _analyze_vault(events: list[dict[str, Any]]) -> dict[str, Any]:
    by_kind: dict[str, int] = {}
    by_mission: dict[str, int] = {}
    pulse_vals: list[int] = []
    infer_labels: dict[str, int] = {}
    for e in events:
        if not isinstance(e, dict):
            continue
        k = str(e.get("kind") or "unknown")
        by_kind[k] = by_kind.get(k, 0) + 1
        m = str(e.get("mission") or "none")
        by_mission[m] = by_mission.get(m, 0) + 1
        try:
            p = int(e.get("pulses") or 0)
            if k == "rx":
                pulse_vals.append(p)
        except (TypeError, ValueError):
            pass
        if k == "infer":
            lab = str(
                e.get("infer")
                if e.get("infer") is not None
                else e.get("infer_label")
                if e.get("infer_label") is not None
                else e.get("label")
                or "?"
            )
            infer_labels[lab] = infer_labels.get(lab, 0) + 1
    pulse_stats: dict[str, Any] = {"n": len(pulse_vals)}
    if pulse_vals:
        pulse_stats.update(
            {
                "min": min(pulse_vals),
                "max": max(pulse_vals),
                "mean": round(sum(pulse_vals) / len(pulse_vals), 2),
                "sum": sum(pulse_vals),
            }
        )
    lessons: list[str] = []
    if not events:
        lessons.append(
            "Vault empty on reconnect — either no offline activity, power-cycle cleared RAM, "
            "or vault was already drained."
        )
    else:
        lessons.append(
            f"Recovered {len(events)} vault event(s) from unplugged field explore."
        )
        if by_kind.get("rx"):
            lessons.append(
                f"Passive RX ticks: {by_kind['rx']}; "
                f"pulse activity range "
                f"{pulse_stats.get('min', 'n/a')}–{pulse_stats.get('max', 'n/a')} "
                f"(mean {pulse_stats.get('mean', 'n/a')})."
            )
        if by_kind.get("infer"):
            lessons.append(
                f"On-device infer steps: {by_kind['infer']} "
                f"(labels={infer_labels}) — host heuristics only, not protocol decode."
            )
        if by_kind.get("done") or by_kind.get("auto"):
            lessons.append(
                f"Mission cycle markers: done={by_kind.get('done', 0)} auto={by_kind.get('auto', 0)}."
            )
        if by_mission:
            top = sorted(by_mission.items(), key=lambda x: -x[1])[:4]
            lessons.append("Missions seen: " + ", ".join(f"{m}×{n}" for m, n in top) + ".")
        lessons.append(
            "Safety: events are edge/pulse counts and mission IR only — "
            "no third-party payload decode, no rolling-code data."
        )
    return {
        "event_count": len(events),
        "by_kind": by_kind,
        "by_mission": by_mission,
        "pulse_stats": pulse_stats,
        "infer_labels": infer_labels,
        "lessons": lessons,
    }


def _append_research_journal(root: Path, trip_id: str, analysis: dict[str, Any], status: dict[str, Any]) -> Path:
    journal = root / "notes" / "unplugged_research_journal.md"
    stamp = _utc_now()
    fw = status.get("version") or "?"
    lines = [
        f"\n## Plug-in research — {stamp}\n",
        f"- **trip_id:** `{trip_id}`\n",
        f"- **firmware:** {fw}\n",
        f"- **vault events:** {analysis.get('event_count', 0)}\n",
        f"- **by_kind:** `{json.dumps(analysis.get('by_kind') or {})}`\n",
        f"- **by_mission:** `{json.dumps(analysis.get('by_mission') or {})}`\n",
        f"- **pulse_stats:** `{json.dumps(analysis.get('pulse_stats') or {})}`\n",
        "\n### Lessons\n",
    ]
    for lesson in analysis.get("lessons") or []:
        lines.append(f"- {_scrub(str(lesson))}\n")
    lines.append(
        "\n### Research follow-ups\n"
        "- Compare pulse ranges to prior field_trips for the same band.\n"
        "- If vault empty after known unplugged time: check power loss vs sticky offline.\n"
        "- Host Signal Cognition: run observe_noise_floor + observe_rx after ingest.\n"
        "- Do not attempt third-party decode from vault pulse counts.\n"
    )
    if not journal.exists():
        header = (
            "# Unplugged research journal\n\n"
            "Auto-appended when `groklink-os plug-sync` runs after USB reconnect.\n"
            "Local-only learning store — do not commit captures with PII.\n"
        )
        journal.write_text(header, encoding="utf-8")
    with journal.open("a", encoding="utf-8") as f:
        f.writelines(lines)
    return journal


def _update_research_index(root: Path, trip: dict[str, Any]) -> None:
    idx_path = root / "research" / "plug_sync_index.json"
    if idx_path.exists():
        try:
            idx = json.loads(idx_path.read_text(encoding="utf-8"))
        except (OSError, json.JSONDecodeError):
            idx = {"schema": 1, "trips": []}
    else:
        idx = {"schema": 1, "trips": []}
    idx.setdefault("trips", []).append(
        {
            "trip_id": trip.get("trip_id"),
            "utc": trip.get("utc"),
            "firmware": (trip.get("status") or {}).get("version"),
            "event_count": (trip.get("analysis") or {}).get("event_count"),
            "lessons": (trip.get("analysis") or {}).get("lessons"),
            "file": trip.get("relative_path"),
        }
    )
    # keep last 200
    idx["trips"] = idx["trips"][-200:]
    idx["updated_at"] = _utc_now()
    idx["total_trips"] = len(idx["trips"])
    idx_path.write_text(json.dumps(idx, indent=2), encoding="utf-8")

    # Cumulative lesson rollup
    rollup = root / "research" / "lessons_learned_summary.json"
    all_lessons: list[str] = []
    kind_totals: dict[str, int] = {}
    for t in idx["trips"]:
        for L in t.get("lessons") or []:
            all_lessons.append(str(L))
    # re-aggregate kinds from last trip only is weak; store cumulative kinds from trip files if present
    summary = {
        "schema": 1,
        "updated_at": _utc_now(),
        "total_plug_syncs": idx["total_trips"],
        "latest_trip_id": trip.get("trip_id"),
        "latest_lessons": (trip.get("analysis") or {}).get("lessons"),
        "latest_by_kind": (trip.get("analysis") or {}).get("by_kind"),
        "note": "Full history in plug_sync_index.json and field_trips/*.json",
    }
    rollup.write_text(json.dumps(summary, indent=2), encoding="utf-8")


def ingest_unplugged_lessons(
    *,
    clear_vault: bool = False,
    client: Any = None,
    n_vault: int = 16,
) -> PlugSyncResult:
    """
    Connect to device (or use provided client), pull unplugged state, document lessons.
    """
    root = _learn_root()
    owns = False
    c = client
    try:
        if c is None:
            from groklink_os.rpc.client import open_client

            c = open_client(timeout=10.0)
            owns = True
        # Soft edu for elevated passive ops if needed
        try:
            c.edu_ack()
        except Exception:  # noqa: BLE001
            pass

        ping = c.ping()
        status = c.status()
        try:
            agent = c.agent_status()
        except Exception as exc:  # noqa: BLE001
            agent = {"ok": False, "error": str(exc)}
        try:
            vault = c.vault_tail(n=n_vault)
        except Exception as exc:  # noqa: BLE001
            vault = {"ok": False, "error": str(exc), "events": [], "count": 0}

        events = vault.get("events") if isinstance(vault.get("events"), list) else []
        # Some firmwares return events as JSON string
        if isinstance(vault.get("events"), str):
            try:
                events = json.loads(vault["events"])
            except json.JSONDecodeError:
                events = []

        analysis = _analyze_vault(events if isinstance(events, list) else [])
        trip_id = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
        trip = {
            "schema": "groklink.field_trip.v1",
            "trip_id": trip_id,
            "utc": _utc_now(),
            "source": "usb_reconnect_plug_sync",
            "ping": ping,
            "status": status,
            "agent_status": agent,
            "vault": {"count": vault.get("count"), "events": events},
            "analysis": analysis,
            "safety": {
                "tx": False,
                "third_party_decode": False,
                "local_learning_only": True,
            },
        }

        trip_path = root / "field_trips" / f"{trip_id}.json"
        trip["relative_path"] = f"field_trips/{trip_id}.json"
        trip_path.write_text(json.dumps(trip, indent=2, default=str), encoding="utf-8")

        # Session-shaped record for learn_from_data compatibility
        session = {
            "ok": True,
            "kind": "unplugged_ingest",
            "trip_id": trip_id,
            "started_at": trip["utc"],
            "finished_at": _utc_now(),
            "status": status,
            "agent_status": agent,
            "vault_analysis": analysis,
            "tx": False,
        }
        sess_path = root / "sessions" / f"unplugged_{trip_id}.json"
        sess_path.write_text(json.dumps(session, indent=2, default=str), encoding="utf-8")

        note_path = _append_research_journal(root, trip_id, analysis, status if isinstance(status, dict) else {})
        _update_research_index(root, trip)

        # Index entry
        idx_path = root / "learnings_index.json"
        try:
            idx = json.loads(idx_path.read_text(encoding="utf-8")) if idx_path.exists() else {"schema": 1, "entries": []}
        except (OSError, json.JSONDecodeError):
            idx = {"schema": 1, "entries": []}
        idx.setdefault("entries", []).append(
            {
                "type": "unplugged_ingest",
                "trip_id": trip_id,
                "utc": trip["utc"],
                "event_count": analysis.get("event_count"),
                "path": str(trip_path.name),
            }
        )
        idx["entries"] = idx["entries"][-500:]
        idx["updated_at"] = _utc_now()
        idx_path.write_text(json.dumps(idx, indent=2), encoding="utf-8")

        cleared = False
        if clear_vault and events:
            try:
                c.vault_clear()
                cleared = True
            except Exception:  # noqa: BLE001
                cleared = False

        narrative = " ".join(analysis.get("lessons") or ["Ingest complete."])
        return PlugSyncResult(
            ok=True,
            trip_id=trip_id,
            path=str(trip_path),
            note_path=str(note_path),
            summary={
                "firmware": status.get("version") if isinstance(status, dict) else None,
                "analysis": analysis,
                "agent": {
                    "offline": agent.get("offline") if isinstance(agent, dict) else None,
                    "active": agent.get("active") if isinstance(agent, dict) else None,
                    "cycles": agent.get("cycles") if isinstance(agent, dict) else None,
                    "vault": agent.get("vault") if isinstance(agent, dict) else None,
                },
                "learn_root": str(root),
            },
            vault_cleared=cleared,
            narrative=narrative,
        )
    except Exception as exc:  # noqa: BLE001
        return PlugSyncResult(ok=False, error=str(exc), narrative=f"Plug-sync failed: {exc}")
    finally:
        if owns and c is not None:
            try:
                c.close()
            except Exception:  # noqa: BLE001
                pass


def wait_for_device_and_ingest(
    *,
    poll_s: float = 2.0,
    timeout_s: float = 0.0,
    clear_vault: bool = False,
) -> PlugSyncResult:
    """Poll until CDC device appears (or timeout), then ingest. timeout_s=0 means forever."""
    from groklink_os.rpc.client import find_device_serial_port

    start = time.monotonic()
    while True:
        port = find_device_serial_port()
        if port:
            # small settle after enumerate
            time.sleep(0.6)
            return ingest_unplugged_lessons(clear_vault=clear_vault)
        if timeout_s > 0 and (time.monotonic() - start) >= timeout_s:
            return PlugSyncResult(
                ok=False,
                error="timeout_waiting_for_device",
                narrative="No GrokLink CDC device appeared before timeout.",
            )
        time.sleep(poll_s)
