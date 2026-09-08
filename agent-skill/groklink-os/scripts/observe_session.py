#!/usr/bin/env python3
"""Run a passive multi-step signal observation session for LLM / operator use.

Uses the groklink-os bridge observation stack when installed; falls back to
documenting the plan if the package is missing.

Authorized research only. Never transmits.
"""
from __future__ import annotations

import argparse
import json
import os
import sys
from datetime import datetime, timezone
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from paths_util import learn_root  # noqa: E402


def _utc_now() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat()


def main() -> int:
    ap = argparse.ArgumentParser(description="Passive RF observation session (no TX)")
    ap.add_argument("--freqs", default="433920000", help="Comma-separated Hz")
    ap.add_argument("--dwell-ms", type=int, default=200)
    ap.add_argument("--no-spectrum", action="store_true")
    ap.add_argument("--monitor-chunks", type=int, default=0)
    ap.add_argument("--mock", action="store_true", help="Use mock device (no RPC)")
    ap.add_argument("--json", action="store_true", help="Force JSON stdout")
    ap.add_argument("--learn", action="store_true", help="Ingest observation store into learn index")
    args = ap.parse_args()

    freqs = [int(x.strip()) for x in args.freqs.split(",") if x.strip()]
    plan = {
        "started_at": _utc_now(),
        "freqs_hz": freqs,
        "dwell_ms": args.dwell_ms,
        "spectrum": not args.no_spectrum,
        "monitor_chunks": args.monitor_chunks,
        "tx": False,
        "note": "Authorized research only. Passive observation.",
    }

    try:
        from groklink_os.observe.agent_loop import run_scripted_observation_session
        from groklink_os.observe.store import ObservationStore
        from groklink_os.observe.tools import ToolDispatcher
    except ImportError:
        print(
            json.dumps(
                {
                    "ok": False,
                    "error": "groklink_os package not importable",
                    "hint": "cd GrokLink-OS/bridge && pip install -e '.[serial]'",
                    "plan": plan,
                },
                indent=2,
            )
        )
        return 1

    if args.mock:

        class Mock:
            def edu_ack(self, phrase: str = "") -> dict:
                return {"ok": True, "edu": True}

            def status(self) -> dict:
                return {"ok": True, "version": "3.2.0", "api": 3, "edu": True, "radio": 0, "heap_free": 9000}

            def subghz_probe(self) -> dict:
                return {"ok": True, "partnum": 0, "version": 20, "hw": False}

            def subghz_rx(self, freq_hz: int = 433920000, ms: int = 200) -> dict:
                return {
                    "ok": True,
                    "freq_hz": freq_hz,
                    "ms": ms,
                    "pulses": 14,
                    "rssi": -64,
                    "sim": True,
                    "ts_ms": 100,
                }

            def spectrum(self, freqs, ms=200, settle_ms=100) -> dict:
                return {
                    "ok": True,
                    "ms": ms,
                    "settle_ms": settle_ms,
                    "bands": [{"freq_hz": f, "pulses": 8, "rssi": -72} for f in freqs],
                }

            def close(self) -> None:
                return None

        d = ToolDispatcher(client=Mock())  # type: ignore[arg-type]
    else:
        d = ToolDispatcher()

    try:
        report = run_scripted_observation_session(
            d,
            freqs_hz=freqs,
            dwell_ms=args.dwell_ms,
            spectrum=not args.no_spectrum,
            monitor_chunks=args.monitor_chunks,
        )
    finally:
        d.close()

    report["plan"] = plan
    report["finished_at"] = _utc_now()

    # persist session
    dest = learn_root() / "sessions" / f"observe_{_utc_now().replace(':', '')}.json"
    dest.write_text(json.dumps(report, indent=2, default=str), encoding="utf-8")
    report["session_log"] = str(dest)

    if args.learn:
        store = ObservationStore()
        obs_path = store.root / "observations" / "recent.jsonl"
        if obs_path.exists():
            try:
                import subprocess

                subprocess.run(
                    [
                        sys.executable,
                        str(Path(__file__).resolve().parent / "learn_from_data.py"),
                        "--observations",
                        str(obs_path),
                    ],
                    check=False,
                    capture_output=True,
                    text=True,
                    timeout=60,
                )
                report["learned_from"] = str(obs_path)
            except Exception as exc:  # noqa: BLE001
                report["learn_error"] = str(exc)

    print(json.dumps(report, indent=2, default=str))
    return 0 if report.get("ok") else 1


if __name__ == "__main__":
    # silence unused import lint for os in some environments
    _ = os.environ
    raise SystemExit(main())
