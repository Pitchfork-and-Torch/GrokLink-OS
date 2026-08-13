#!/usr/bin/env python3
"""Exit 1 if CP1252/UTF-8 mojibake markers are present (for pre-commit / CI)."""
from __future__ import annotations

import sys
from pathlib import Path

# Import fixer helpers
sys.path.insert(0, str(Path(__file__).resolve().parent))
from fix_mojibake_utf8 import looks_corrupted, iter_files, fix_text  # noqa: E402


def main() -> int:
    root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else Path(__file__).resolve().parents[1]
    bad = []
    for p in iter_files(root):
        try:
            t = p.read_text(encoding="utf-8")
        except Exception:
            continue
        if looks_corrupted(t) or fix_text(t) != t.lstrip("\ufeff") and t.startswith("\ufeff"):
            # any fix_text change means dirty
            if fix_text(t) != t:
                bad.append(p.relative_to(root).as_posix())
    if bad:
        print("MOJIBAKE DETECTED in:")
        for b in bad:
            print(" ", b)
        print("Run: py scripts/fix_mojibake_utf8.py")
        return 1
    print("mojibake check: clean")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
