#!/usr/bin/env python3
"""One-shot adaptive refresh: sync firmware knowledge → probe → summarize learnings.

Run at session start and after any firmware update.
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
from pathlib import Path
from typing import Any


def _run(script: str, extra: list[str] | None = None) -> dict[str, Any]:
    here = Path(__file__).resolve().parent
    cmd = [sys.executable, str(here / script), *(extra or [])]
    try:
        proc = subprocess.run(cmd, capture_output=True, text=True, timeout=120, check=False)
    except subprocess.TimeoutExpired:
        return {"ok": False, "error": "timeout", "script": script}
    out = (proc.stdout or "").strip()
    parsed: Any = None
    try:
        parsed = json.loads(out) if out else None
    except json.JSONDecodeError:
        parsed = {"raw": out[:4000]}
    return {
        "ok": proc.returncode == 0,
        "returncode": proc.returncode,
        "script": script,
        "result": parsed,
        "stderr": (proc.stderr or "")[:1000],
    }


def main() -> int:
    ap = argparse.ArgumentParser(description="Refresh GrokLink OS adaptive knowledge")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--offline", action="store_true")
    ap.add_argument("--deep-probe", action="store_true")
    ap.add_argument("--host", default=os.environ.get("GLK_RPC_HOST", "127.0.0.1"))
    ap.add_argument("--port", type=int, default=int(os.environ.get("GLK_RPC_PORT", "7341")))
    args = ap.parse_args()

    if args.dry_run:
        print(
            json.dumps(
                {
                    "dry_run": True,
                    "steps": [
                        "sync_firmware_knowledge.py",
                        "probe_capabilities.py",
                        "learn_from_data.py --summary",
                    ],
                    "offline": args.offline,
                    "host": args.host,
                    "port": args.port,
                },
                indent=2,
            )
        )
        return 0

    sync_args = ["--offline"] if args.offline else []
    probe_args = ["--host", args.host, "--port", str(args.port)]
    if args.deep_probe:
        probe_args.append("--deep")

    report = {
        "ok": True,
        "sync": _run("sync_firmware_knowledge.py", sync_args),
        "probe": _run("probe_capabilities.py", probe_args),
        "learning": _run("learn_from_data.py", ["--summary"]),
    }
    report["ok"] = all(
        report[k].get("ok") for k in ("sync", "learning")
    )  # probe may fail if device offline — still useful

    # Agent-facing advice
    sync_r = (report["sync"].get("result") or {}) if isinstance(report["sync"], dict) else {}
    delta = sync_r.get("delta") or {}
    advice: list[str] = []
    if delta.get("first_sync"):
        advice.append("First knowledge sync complete — prefer live probe over hardcoded CLI maps.")
    for ch in delta.get("changes") or []:
        if ch != "no material changes":
            advice.append(f"Firmware knowledge changed: {ch}")
    probe_r = report["probe"].get("result") or {}
    if isinstance(probe_r, dict):
        if probe_r.get("link") == "down":
            advice.append("Device link down — operating on docs/CLI fallback capabilities.")
        elif probe_r.get("mode") == "live":
            advice.append(
                f"Live link up; supported_cmds={probe_r.get('supported_cmds')}"
            )
    report["agent_advice"] = advice
    print(json.dumps(report, indent=2, default=str))
    return 0 if report["ok"] else 1


if __name__ == "__main__":
    raise SystemExit(main())
