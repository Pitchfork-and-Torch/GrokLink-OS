#!/usr/bin/env python3
"""Probe live GrokLink OS capabilities and merge into the learning store.

Combines:
- firmware_snapshot.json (docs/release/CLI)
- Live RPC ping/status/skill_list/mission_list (passive only)
- Optional command probing that never issues TX

Does not transmit. Does not invent confirm tokens.
"""
from __future__ import annotations

import argparse
import json
import os
import sys
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Optional

from paths_util import (
    EDU_PHRASE,
    capabilities_path,
    firmware_snapshot_path,
    learn_root,
)


def _utc_now() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat()


def _load_json(path: Path) -> dict[str, Any]:
    if not path.exists():
        return {}
    try:
        return json.loads(path.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return {}


def _client(host: str, port: int):
    try:
        from groklink_os.rpc.client import open_client  # type: ignore

        # Auto-picks STM32 CDC (0483:5740) / GLK_SERIAL_PORT when present
        return open_client(host=host, port=port, timeout=10.0)
    except ImportError:
        return None
    except Exception:  # noqa: BLE001
        return None


def probe(
    host: str,
    port: int,
    *,
    deep: bool = False,
) -> dict[str, Any]:
    snap = _load_json(firmware_snapshot_path())
    result: dict[str, Any] = {
        "schema": 1,
        "probed_at": _utc_now(),
        "host": host,
        "port": port,
        "link": "down",
        "firmware_snapshot_version": snap.get("version"),
        "known_rpc_from_docs": list(snap.get("rpc_commands") or []),
        "known_cli": list((snap.get("cli") or {}).get("commands") or []),
        "live": {},
        "supported_cmds": [],
        "denied_cmds": [],
        "unknown_cmds": [],
        "status": None,
        "skills": None,
        "missions": None,
        "safety": {
            "tx": False,
            "note": "probe is passive-only; TX never attempted",
        },
    }

    # Always treat these as known baseline
    baseline = [
        "ping",
        "edu_ack",
        "status",
        "subghz_rx",
        "subghz_tx",
        "confirm_issue",
        "spectrum",
        "skill_list",
        "mission_list",
    ]
    known = list(dict.fromkeys(result["known_rpc_from_docs"] + baseline))

    client = _client(host, port)
    if client is None:
        result["error"] = "groklink_os package not importable; install bridge"
        result["supported_cmds"] = known  # docs-only fallback
        return result

    try:
        with client:
            ping = client.ping()
            result["live"]["ping"] = ping
            result["link"] = "up" if ping.get("ok") is not False else "degraded"
            try:
                result["live"]["edu_ack"] = client.edu_ack(EDU_PHRASE)
            except Exception as exc:  # noqa: BLE001
                result["live"]["edu_ack_error"] = str(exc)
            try:
                st = client.status()
                result["status"] = st
                result["live"]["status"] = st
            except Exception as exc:  # noqa: BLE001
                result["live"]["status_error"] = str(exc)
            for name, meth in (
                ("skill_list", client.skill_list),
                ("mission_list", client.mission_list),
            ):
                try:
                    r = meth()
                    result[name.replace("_list", "s") if name.endswith("_list") else name] = r
                    result["live"][name] = r
                    result["supported_cmds"].append(name if name != "skill_list" else "skill_list")
                except Exception as exc:  # noqa: BLE001
                    result["live"][f"{name}_error"] = str(exc)
                    result["denied_cmds"].append({"cmd": name, "error": str(exc)})

            # Record known API methods from client class
            for attr in dir(client):
                if attr.startswith("_"):
                    continue
                if attr in ("connect", "close", "call"):
                    continue
                if callable(getattr(client, attr, None)):
                    if attr not in result["supported_cmds"]:
                        result["supported_cmds"].append(attr)

            if deep:
                # Safe probes only — short passive ops already covered by session_check
                for cmd in known:
                    if cmd in ("subghz_tx",):
                        result["denied_cmds"].append(
                            {"cmd": cmd, "error": "skipped_tx_by_policy"}
                        )
                        continue
                    if cmd in result["supported_cmds"] or cmd in (
                        "ping",
                        "edu_ack",
                        "status",
                    ):
                        continue
                    # raw call for discovery — may fail closed
                    try:
                        if cmd == "subghz_probe":
                            r = client.call("subghz_probe")
                        elif cmd == "audit_tail":
                            r = client.call("audit_tail", limit=5)
                        else:
                            continue  # don't spam unknown shapes
                        result["live"][cmd] = r
                        if r.get("ok") is False and "deny" in json.dumps(r).lower():
                            result["denied_cmds"].append({"cmd": cmd, "resp": r})
                        else:
                            result["supported_cmds"].append(cmd)
                    except Exception as exc:  # noqa: BLE001
                        result["unknown_cmds"].append({"cmd": cmd, "error": str(exc)})
    except Exception as exc:  # noqa: BLE001
        result["error"] = str(exc)
        result["link"] = "down"

    # Merge docs-known into supported when link down (agent can still plan)
    if result["link"] == "down":
        result["supported_cmds"] = list(dict.fromkeys(known))
        result["mode"] = "docs_fallback"
    else:
        result["mode"] = "live"
        result["supported_cmds"] = list(dict.fromkeys(result["supported_cmds"] + ["ping", "edu_ack", "status"]))

    result["learn_dir"] = str(learn_root())
    return result


def main() -> int:
    ap = argparse.ArgumentParser(description="Probe GrokLink OS capabilities (no TX)")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--deep", action="store_true", help="Extra safe RPC discovery")
    ap.add_argument("--host", default=os.environ.get("GLK_RPC_HOST", "127.0.0.1"))
    ap.add_argument("--port", type=int, default=int(os.environ.get("GLK_RPC_PORT", "7341")))
    args = ap.parse_args()

    if args.dry_run:
        print(
            json.dumps(
                {
                    "dry_run": True,
                    "would_write": str(capabilities_path()),
                    "host": args.host,
                    "port": args.port,
                    "deep": args.deep,
                },
                indent=2,
            )
        )
        return 0

    result = probe(args.host, args.port, deep=args.deep)
    capabilities_path().write_text(json.dumps(result, indent=2, default=str), encoding="utf-8")
    print(json.dumps(result, indent=2, default=str))
    return 0 if result.get("link") in ("up", "degraded") or result.get("mode") == "docs_fallback" else 1


if __name__ == "__main__":
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    raise SystemExit(main())
