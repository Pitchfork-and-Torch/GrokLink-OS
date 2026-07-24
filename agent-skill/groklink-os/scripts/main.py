#!/usr/bin/env python3
"""Unified entry for GrokLink OS adaptive skill helpers.

Usage:
  main.py refresh [--offline] [--deep-probe]
  main.py session [--rx] [--freq Hz] [--ms N]
  main.py sync [--offline]
  main.py probe [--deep]
  main.py learn --summary | --capture PATH | --session PATH | --note TEXT
  main.py --dry-run
"""
from __future__ import annotations

import subprocess
import sys
from pathlib import Path


def main() -> int:
    here = Path(__file__).resolve().parent
    argv = sys.argv[1:]
    if not argv or argv[0] in ("-h", "--help"):
        print(__doc__)
        return 0

    if argv[0] == "--dry-run":
        print('{"dry_run": true, "entry": "main.py", "subcommands": ["refresh","session","observe","sync","probe","learn"]}')
        return 0

    dispatch = {
        "refresh": "refresh_all.py",
        "session": "session_check.py",
        "observe": "observe_session.py",
        "sync": "sync_firmware_knowledge.py",
        "probe": "probe_capabilities.py",
        "learn": "learn_from_data.py",
    }
    cmd = argv[0]
    if cmd not in dispatch:
        # backward compatible: treat as session_check args
        target = here / "session_check.py"
        return subprocess.call([sys.executable, str(target), *argv])

    target = here / dispatch[cmd]
    return subprocess.call([sys.executable, str(target), *argv[1:]])


if __name__ == "__main__":
    raise SystemExit(main())
