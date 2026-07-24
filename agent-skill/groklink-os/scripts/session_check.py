#!/usr/bin/env python3
"""GrokLink OS lab session helper — ping, edu-ack, status, optional passive RX.

Logs each run into the adaptive learning store (sessions/).
Authorized research only. Never transmits.
"""
from __future__ import annotations

import argparse
import json
import os
import shutil
import subprocess
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Optional

# local imports
sys.path.insert(0, str(Path(__file__).resolve().parent))
from paths_util import EDU_PHRASE, learn_root  # noqa: E402


def _utc_now() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat()


def _which_bridge() -> Optional[str]:
    return shutil.which("groklink-os")


def _run_cli(args: list[str], timeout: float = 20.0) -> dict[str, Any]:
    cmd = ["groklink-os", *args]
    try:
        proc = subprocess.run(
            cmd,
            capture_output=True,
            text=True,
            timeout=timeout,
            check=False,
        )
    except FileNotFoundError:
        return {"ok": False, "error": "groklink-os not on PATH"}
    except subprocess.TimeoutExpired:
        return {"ok": False, "error": "timeout", "cmd": cmd}

    out = (proc.stdout or "").strip()
    err = (proc.stderr or "").strip()
    parsed: Any = None
    for candidate in (out, *(reversed(out.splitlines()) if out else [])):
        c = candidate.strip()
        if c.startswith("{") and c.endswith("}"):
            try:
                parsed = json.loads(c)
                break
            except json.JSONDecodeError:
                continue
    return {
        "ok": proc.returncode == 0,
        "returncode": proc.returncode,
        "stdout": out,
        "stderr": err,
        "json": parsed,
        "cmd": cmd,
    }


def _try_python_client(
    host: str,
    port: int,
    do_rx: bool,
    freq: int,
    ms: int,
) -> dict[str, Any]:
    try:
        from groklink_os.rpc.client import open_client  # type: ignore
    except ImportError:
        return {"ok": False, "error": "groklink_os package not importable"}

    result: dict[str, Any] = {"ok": True, "via": "python-client"}
    try:
        # open_client auto-picks STM32 CDC (0483:5740) / GLK_SERIAL_PORT when present
        with open_client(host=host, port=port, timeout=10.0) as c:
            result["transport"] = getattr(c, "transport_name", None)
            result["serial_port"] = getattr(c, "serial_port", None)
            result["ping"] = c.ping()
            result["edu_ack"] = c.edu_ack(EDU_PHRASE)
            result["status"] = c.status()
            if do_rx:
                result["rx"] = c.subghz_rx(freq_hz=freq, ms=ms)
    except Exception as exc:  # noqa: BLE001
        return {"ok": False, "error": str(exc), "via": "python-client"}
    return result


def _persist_session(report: dict[str, Any]) -> str:
    dest = learn_root() / "sessions" / f"session_{_utc_now().replace(':', '')}.json"
    dest.write_text(json.dumps(report, indent=2, default=str), encoding="utf-8")
    # best-effort index update via learn_from_data
    try:
        subprocess.run(
            [
                sys.executable,
                str(Path(__file__).resolve().parent / "learn_from_data.py"),
                "--session",
                str(dest),
            ],
            capture_output=True,
            text=True,
            timeout=30,
            check=False,
        )
    except Exception:  # noqa: BLE001
        pass
    return str(dest)


def main() -> int:
    ap = argparse.ArgumentParser(
        description="GrokLink OS session check (authorized research only; no TX)."
    )
    ap.add_argument("--dry-run", action="store_true", help="Print plan only")
    ap.add_argument("--rx", action="store_true", help="Optional passive RX after edu-ack")
    ap.add_argument("--freq", type=int, default=433_920_000, help="RX frequency Hz")
    ap.add_argument("--ms", type=int, default=400, help="RX dwell ms")
    ap.add_argument("--host", default=os.environ.get("GLK_RPC_HOST", "127.0.0.1"))
    ap.add_argument("--port", type=int, default=int(os.environ.get("GLK_RPC_PORT", "7341")))
    ap.add_argument("--no-learn", action="store_true", help="Do not write learning store")
    ap.add_argument("--json", action="store_true", help="Force JSON summary on stdout")
    args = ap.parse_args()

    plan = {
        "steps": ["ping", "edu-ack", "status"] + (["rx"] if args.rx else []),
        "edu_phrase": EDU_PHRASE,
        "host": args.host,
        "port": args.port,
        "rx": {"freq_hz": args.freq, "ms": args.ms} if args.rx else None,
        "tx": False,
        "note": "Authorized research only. This helper never transmits.",
    }

    if args.dry_run:
        print(json.dumps({"dry_run": True, **plan}, indent=2))
        return 0

    bridge = _which_bridge()
    report: dict[str, Any] = {
        "bridge_path": bridge,
        "host": args.host,
        "port": args.port,
        "tx": False,
        "edu_phrase": EDU_PHRASE,
        "started_at": _utc_now(),
    }

    if bridge:
        report["via"] = "cli"
        report["ping"] = _run_cli(["ping"])
        report["edu_ack"] = _run_cli(["edu-ack"])
        report["status"] = _run_cli(["status"])
        if args.rx:
            report["rx"] = _run_cli(
                ["rx", "--freq", str(args.freq), "--ms", str(args.ms)],
                timeout=30.0,
            )
        ok = all(
            report[k].get("ok")
            for k in ("ping", "edu_ack", "status")
            if isinstance(report.get(k), dict)
        )
        if args.rx and isinstance(report.get("rx"), dict):
            ok = ok and bool(report["rx"].get("ok"))
        report["ok"] = ok
    else:
        report = {
            **report,
            **_try_python_client(args.host, args.port, args.rx, args.freq, args.ms),
        }
        if not report.get("ok") and report.get("error") == "groklink_os package not importable":
            report["hint"] = (
                "Install: git clone https://github.com/Pitchfork-and-Torch/GrokLink-OS "
                "&& cd GrokLink-OS/bridge && pip install -e ."
            )

    report["finished_at"] = _utc_now()
    if not args.no_learn:
        report["session_log"] = _persist_session(report)

    print(json.dumps(report, indent=2, default=str))
    return 0 if report.get("ok") else 1


if __name__ == "__main__":
    sys.exit(main())
