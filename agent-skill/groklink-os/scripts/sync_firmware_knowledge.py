#!/usr/bin/env python3
"""Pull latest public GrokLink OS knowledge into the local learning store.

Sources (in priority order for each field):
1. Local clone (GROKLINK_OS_ROOT or auto-detect)
2. GitHub API + raw.githubusercontent.com (main branch + latest release)

Never stores secrets. Authorized research documentation only.
"""
from __future__ import annotations

import argparse
import json
import re
import ssl
import sys
import urllib.error
import urllib.request
from datetime import datetime, timezone
from pathlib import Path
from typing import Any, Optional

from paths_util import (
    API_BASE,
    RAW_BASE,
    REPO_DEFAULT,
    REPO_URL,
    firmware_snapshot_path,
    learn_root,
    local_repo_root,
)


def _utc_now() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat()


def _http_get(url: str, timeout: float = 20.0) -> tuple[int, str]:
    req = urllib.request.Request(
        url,
        headers={
            "User-Agent": "groklink-os-skill/adaptive",
            "Accept": "application/vnd.github+json, text/plain, */*",
        },
        method="GET",
    )
    ctx = ssl.create_default_context()
    try:
        with urllib.request.urlopen(req, timeout=timeout, context=ctx) as resp:
            body = resp.read().decode("utf-8", errors="replace")
            return int(resp.status), body
    except urllib.error.HTTPError as e:
        return int(e.code), e.read().decode("utf-8", errors="replace")
    except Exception as exc:  # noqa: BLE001
        return 0, str(exc)


def _read_local(path: Path) -> Optional[str]:
    try:
        return path.read_text(encoding="utf-8")
    except OSError:
        return None


def _extract_rpc_cmds(rpc_md: str) -> list[str]:
    cmds: list[str] = []
    # ### cmd_name or {"cmd":"name"
    for m in re.finditer(r'^###\s+([a-z][a-z0-9_]*)', rpc_md, re.M):
        cmds.append(m.group(1))
    for m in re.finditer(r'"cmd"\s*:\s*"([a-z][a-z0-9_]*)"', rpc_md):
        cmds.append(m.group(1))
    # Responses / noise — not client commands
    skip = {"pong", "ok", "error", "true", "false"}
    # dedupe preserve order
    seen: set[str] = set()
    out: list[str] = []
    for c in cmds:
        if c in skip:
            continue
        if c not in seen:
            seen.add(c)
            out.append(c)
    return out


def _extract_cli_commands(help_text: str) -> list[str]:
    cmds: list[str] = []
    for line in help_text.splitlines():
        # typer style: "  ping  " or "  edu-ack  "
        m = re.match(r'^\s{2}([a-z][a-z0-9-]*)\s{2,}', line)
        if m:
            cmds.append(m.group(1))
    seen: set[str] = set()
    out: list[str] = []
    for c in cmds:
        if c not in seen and c not in ("options", "commands"):
            seen.add(c)
            out.append(c)
    return out


def _parse_cli_help() -> dict[str, Any]:
    import shutil
    import subprocess

    if not shutil.which("groklink-os"):
        return {"available": False, "commands": []}
    try:
        proc = subprocess.run(
            ["groklink-os", "--help"],
            capture_output=True,
            text=True,
            timeout=15,
            check=False,
        )
        text = (proc.stdout or "") + "\n" + (proc.stderr or "")
        return {
            "available": proc.returncode == 0 or bool(proc.stdout),
            "commands": _extract_cli_commands(text),
            "help_excerpt": text[:2000],
        }
    except Exception as exc:  # noqa: BLE001
        return {"available": False, "error": str(exc), "commands": []}


def build_snapshot(*, use_network: bool = True) -> dict[str, Any]:
    root = local_repo_root()
    snap: dict[str, Any] = {
        "schema": 1,
        "synced_at": _utc_now(),
        "repo": REPO_DEFAULT,
        "repo_url": REPO_URL,
        "sources": [],
        "version": None,
        "latest_release": None,
        "rpc_commands": [],
        "cli": {},
        "docs": {},
        "highlights": [],
        "safety": {
            "edu_phrase": "I_WILL_USE_ONLY_AUTHORIZED_TARGETS",
            "default_deny": ["active_tx", "gpio", "contact", "system"],
            "risk_classes": [
                "info",
                "passive_rx",
                "active_tx",
                "gpio",
                "contact",
                "system",
            ],
        },
    }

    # --- local clone ---
    if root:
        snap["sources"].append(f"local:{root}")
        ver = _read_local(root / "VERSION")
        if ver:
            snap["version"] = ver.strip()
        for rel, key in (
            ("docs/api/RPC.md", "rpc_md"),
            ("docs/SAFETY.md", "safety_md"),
            ("docs/SKILLS.md", "skills_md"),
            ("docs/GUI.md", "gui_md"),
            ("README.md", "readme"),
            ("CHANGELOG.md", "changelog"),
            ("bridge/README.md", "bridge_readme"),
        ):
            text = _read_local(root / rel)
            if text:
                snap["docs"][key] = {
                    "path": rel,
                    "chars": len(text),
                    "sha16": _short_hash(text),
                    "excerpt": text[:1500],
                }
                if key == "rpc_md":
                    snap["rpc_commands"] = _extract_rpc_cmds(text)
        # README highlights table rows (best-effort)
        readme = _read_local(root / "README.md") or ""
        for m in re.finditer(r'^\|\s*([^|]+?)\s*\|\s*([^|]+?)\s*\|', readme, re.M):
            a, b = m.group(1).strip(), m.group(2).strip()
            if a.lower() in ("area", "------") or set(a) <= {"-"}:
                continue
            if len(snap["highlights"]) < 20:
                snap["highlights"].append({"area": a, "status": b})

    # --- network ---
    if use_network:
        code, body = _http_get(f"{API_BASE}/releases/latest")
        if code == 200:
            try:
                rel = json.loads(body)
                snap["latest_release"] = {
                    "tag": rel.get("tag_name"),
                    "name": rel.get("name"),
                    "published_at": rel.get("published_at"),
                    "body_excerpt": (rel.get("body") or "")[:2000],
                    "assets": [
                        {"name": a.get("name"), "size": a.get("size")}
                        for a in (rel.get("assets") or [])
                    ],
                }
                snap["sources"].append("github:releases/latest")
                if not snap["version"] and rel.get("tag_name"):
                    snap["version"] = str(rel["tag_name"]).lstrip("v")
            except json.JSONDecodeError:
                snap["latest_release_error"] = "invalid json"

        for rel_path, key in (
            ("VERSION", "version_file"),
            ("docs/api/RPC.md", "rpc_md"),
            ("README.md", "readme"),
        ):
            if key in ("rpc_md", "readme") and key in snap["docs"]:
                continue  # already have local
            code, body = _http_get(f"{RAW_BASE}/{rel_path}")
            if code == 200 and body and not body.startswith("404"):
                snap["sources"].append(f"raw:{rel_path}")
                if rel_path == "VERSION" and not snap["version"]:
                    snap["version"] = body.strip().splitlines()[0].strip()
                if rel_path.endswith(".md"):
                    snap["docs"][key] = {
                        "path": rel_path,
                        "chars": len(body),
                        "sha16": _short_hash(body),
                        "excerpt": body[:1500],
                    }
                    if "RPC" in rel_path:
                        remote_cmds = _extract_rpc_cmds(body)
                        if len(remote_cmds) >= len(snap["rpc_commands"]):
                            snap["rpc_commands"] = remote_cmds

    snap["cli"] = _parse_cli_help()
    snap["learn_dir"] = str(learn_root())
    return snap


def _short_hash(text: str) -> str:
    import hashlib

    return hashlib.sha256(text.encode("utf-8")).hexdigest()[:16]


def load_previous() -> Optional[dict[str, Any]]:
    p = firmware_snapshot_path()
    if not p.exists():
        return None
    try:
        return json.loads(p.read_text(encoding="utf-8"))
    except (OSError, json.JSONDecodeError):
        return None


def diff_snapshots(old: Optional[dict[str, Any]], new: dict[str, Any]) -> dict[str, Any]:
    if not old:
        return {"first_sync": True, "changes": ["initial snapshot"]}
    changes: list[str] = []
    if old.get("version") != new.get("version"):
        changes.append(f"version {old.get('version')} → {new.get('version')}")
    old_tag = (old.get("latest_release") or {}).get("tag")
    new_tag = (new.get("latest_release") or {}).get("tag")
    if old_tag != new_tag:
        changes.append(f"release {old_tag} → {new_tag}")
    old_cmds = set(old.get("rpc_commands") or [])
    new_cmds = set(new.get("rpc_commands") or [])
    added = sorted(new_cmds - old_cmds)
    removed = sorted(old_cmds - new_cmds)
    if added:
        changes.append(f"rpc added: {', '.join(added)}")
    if removed:
        changes.append(f"rpc removed: {', '.join(removed)}")
    old_cli = set((old.get("cli") or {}).get("commands") or [])
    new_cli = set((new.get("cli") or {}).get("commands") or [])
    cli_add = sorted(new_cli - old_cli)
    if cli_add:
        changes.append(f"cli added: {', '.join(cli_add)}")
    for key in ("rpc_md", "readme", "safety_md"):
        o = (old.get("docs") or {}).get(key, {}).get("sha16")
        n = (new.get("docs") or {}).get(key, {}).get("sha16")
        if o and n and o != n:
            changes.append(f"doc changed: {key}")
    return {"first_sync": False, "changes": changes or ["no material changes"]}


def main() -> int:
    ap = argparse.ArgumentParser(description="Sync GrokLink OS firmware knowledge")
    ap.add_argument("--dry-run", action="store_true")
    ap.add_argument("--offline", action="store_true", help="Local clone + CLI only")
    ap.add_argument("--json", action="store_true", help="Print full snapshot")
    args = ap.parse_args()

    prev = load_previous()
    snap = build_snapshot(use_network=not args.offline)
    delta = diff_snapshots(prev, snap)
    snap["delta_from_previous"] = delta

    out_path = firmware_snapshot_path()
    if args.dry_run:
        print(
            json.dumps(
                {
                    "dry_run": True,
                    "would_write": str(out_path),
                    "version": snap.get("version"),
                    "latest_release": snap.get("latest_release"),
                    "rpc_commands": snap.get("rpc_commands"),
                    "cli_commands": (snap.get("cli") or {}).get("commands"),
                    "delta": delta,
                    "sources": snap.get("sources"),
                },
                indent=2,
            )
        )
        return 0

    out_path.write_text(json.dumps(snap, indent=2), encoding="utf-8")
    summary = {
        "ok": True,
        "path": str(out_path),
        "version": snap.get("version"),
        "latest_release_tag": (snap.get("latest_release") or {}).get("tag"),
        "rpc_commands": snap.get("rpc_commands"),
        "cli_commands": (snap.get("cli") or {}).get("commands"),
        "delta": delta,
        "sources": snap.get("sources"),
        "learn_dir": snap.get("learn_dir"),
    }
    if args.json:
        print(json.dumps(snap, indent=2))
    else:
        print(json.dumps(summary, indent=2))
    return 0


if __name__ == "__main__":
    # Allow running as script from any cwd
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    raise SystemExit(main())
