#!/usr/bin/env python3
"""On USB reconnect: pull unplugged vault lessons into PC learning store.

Usage:
  py -3 ingest_unplugged.py
  py -3 ingest_unplugged.py --clear-vault
  py -3 ingest_unplugged.py --wait --timeout 120
"""
from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

# Allow running from skill tree without install
_ROOT = Path(__file__).resolve().parents[3]
_BRIDGE = _ROOT / "bridge"
if _BRIDGE.is_dir():
    sys.path.insert(0, str(_BRIDGE))


def main() -> int:
    ap = argparse.ArgumentParser(description="Ingest unplugged field lessons on plug-in")
    ap.add_argument("--clear-vault", action="store_true")
    ap.add_argument("--wait", action="store_true", help="Wait for device CDC")
    ap.add_argument("--timeout", type=float, default=0, help="Wait timeout seconds (0=forever)")
    args = ap.parse_args()

    from groklink_os.research.plug_sync import (
        ingest_unplugged_lessons,
        wait_for_device_and_ingest,
    )

    if args.wait:
        r = wait_for_device_and_ingest(
            timeout_s=float(args.timeout),
            clear_vault=bool(args.clear_vault),
        )
    else:
        r = ingest_unplugged_lessons(clear_vault=bool(args.clear_vault))
    print(json.dumps(r.to_dict(), indent=2, default=str))
    return 0 if r.ok else 1


if __name__ == "__main__":
    raise SystemExit(main())
