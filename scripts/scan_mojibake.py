#!/usr/bin/env python3
from __future__ import annotations

from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SKIP = {".git", "build", "node_modules", "__pycache__", "dist", ".pio"}


def main() -> None:
    bad = []
    for p in ROOT.rglob("*"):
        if not p.is_file() or any(s in p.parts for s in SKIP):
            continue
        try:
            b = p.read_bytes()
        except OSError:
            continue
        if b"\x00" in b[:300]:
            continue
        rel = p.relative_to(ROOT).as_posix()
        if b"\xc3\xa2\xe2\x82\xac" in b or b"\xc3\xa2\xc2\x80" in b:
            bad.append(("double-em", rel))
        if b"\xc3\xa2\xc2\x94" in b or b"\xc3\xa2\xe2\x94" in b:
            bad.append(("double-box", rel))
        try:
            t = b.decode("utf-8")
        except UnicodeDecodeError:
            bad.append(("undecodable", rel))
            continue
        if "\u00e2\u20ac" in t:
            bad.append(("euro-seq", rel))
        # mojibake vertical bar often appears as a-circumflex + something
        if "\u00e2\u0094\u0082" in t:
            bad.append(("box-latin", rel))
    print("hits", len(bad))
    for x in bad[:80]:
        print(x[0], x[1])

    # sample non-ascii from README and top md
    for name in ["README.md", "docs"]:
        p = ROOT / name
        if p.is_file():
            t = p.read_text(encoding="utf-8")
            print("---", name, "nonascii samples ---")
            n = 0
            for i, line in enumerate(t.splitlines(), 1):
                if any(ord(c) > 127 for c in line):
                    print(i, [hex(ord(c)) for c in line if ord(c) > 127][:12], line[:90])
                    n += 1
                    if n >= 15:
                        break


if __name__ == "__main__":
    main()
