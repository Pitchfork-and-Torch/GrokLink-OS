#!/usr/bin/env python3
"""
Fix CP1252/UTF-8 mojibake in text files.

Classic path: real UTF-8 multi-byte chars (—, │, └, …) were decoded as
Windows-1252 and re-saved as Unicode, producing sequences like:
  —  (was —)
  │  (was │)
  ─  (was ─)

This script rebuilds those sequences from first principles and replaces them.
Also strips UTF-8 BOM.
"""
from __future__ import annotations

import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SKIP_DIRS = {
    ".git",
    "build",
    "dist",
    "node_modules",
    "__pycache__",
    ".pio",
    ".west",
    "target",
    ".cargo",
    "vendor",
    ".cache",
}
TEXT_EXT = {
    ".md",
    ".txt",
    ".c",
    ".h",
    ".cpp",
    ".hpp",
    ".cc",
    ".py",
    ".js",
    ".ts",
    ".tsx",
    ".jsx",
    ".json",
    ".yml",
    ".yaml",
    ".toml",
    ".cmake",
    ".ld",
    ".S",
    ".s",
    ".rst",
    ".html",
    ".css",
    ".sh",
    ".ps1",
    ".cmd",
    ".bat",
    ".ini",
    ".cfg",
    ".conf",
    ".mk",
    ".csv",
    ".svg",
    ".xml",
    ".properties",
}

# Characters we restore (box drawing, dashes, arrows, etc.)
RESTORE_CHARS = (
    # dashes / quotes / ellipsis / bullets
    "\u2014\u2013\u2012\u2018\u2019\u201c\u201d\u2026\u2022\u00b7\u00d7\u00f7"
    # arrows
    "\u2190\u2191\u2192\u2193\u21d2\u21d0"
    # box light
    "\u2500\u2502\u250c\u2510\u2514\u2518\u251c\u2524\u252c\u2534\u253c"
    # box heavy / double (common)
    "\u2550\u2551\u2554\u2557\u255a\u255d\u2560\u2563\u2566\u2569\u256c"
    # blocks / triangles sometimes used in diagrams
    "\u25bc\u25b2\u25b6\u25c0\u25a0\u25a1\u25cf\u25cb"
    # check / cross
    "\u2713\u2717\u00a9\u00ae\u00b0"
)


def build_replacements() -> list[tuple[str, str]]:
    """For each target char, compute its CP1252-mojibake form from UTF-8 bytes."""
    out: list[tuple[str, str]] = []
    for ch in RESTORE_CHARS:
        raw = ch.encode("utf-8")
        try:
            moj = raw.decode("cp1252")
        except UnicodeDecodeError:
            # rare: some utf-8 bytes not valid cp1252 alone — try latin-1
            moj = raw.decode("latin-1")
        if moj != ch:
            out.append((moj, ch))
    # longest first so multi-byte sequences win
    out.sort(key=lambda x: -len(x[0]))
    return out


REPLACEMENTS = build_replacements()


def looks_corrupted(text: str) -> bool:
    # common prefixes of mojibake: a-circumflex followed by smart quotes / box debris
    if "\u00e2\u201d" in text or "\u00e2\u20ac" in text:
        return True
    if "\u00e2\u201c" in text or "\u00e2\u201a" in text:
        return True
    if "\ufeff" in text and text.index("\ufeff") == 0:
        return True
    # any of our known mojibake keys
    return any(a in text for a, _ in REPLACEMENTS[:20])


def fix_text(text: str) -> str:
    import re

    if text.startswith("\ufeff"):
        text = text.lstrip("\ufeff")
    for a, b in REPLACEMENTS:
        if a in text:
            text = text.replace(a, b)
    # Residual after partial fixes: â + ” + C1 control (e.g. U+0090 for ┐)
    extras = {
        "\u00e2\u201d\u0090": "\u2510",  # ┐
        "\u00e2\u201d\u009d": "\u2518",  # ┘ sometimes
        "\u00e2\u201d\u0094": "\u2514",  # └
        "\u00e2\u201d\u008c": "\u250c",  # ┌
        "\u00e2\u201d\u009c": "\u251c",  # ├
        "\u00e2\u201d\u00a4": "\u2524",  # ┤
        "\u00e2\u201d\u20ac": "\u2500",  # ─ residual
    }
    for a, b in extras.items():
        text = text.replace(a, b)
    text = re.sub("\u00e2\u201d[\u0080-\u009f]", "", text)
    text = re.sub("\u00e2\u20ac.", "-", text)
    return text


def iter_files(root: Path):
    for p in root.rglob("*"):
        if not p.is_file():
            continue
        if any(part in SKIP_DIRS for part in p.parts):
            continue
        if p.name in {"CMakeLists.txt", "Makefile", "LICENSE", "Dockerfile"} or p.name.upper().startswith(
            "README"
        ):
            yield p
            continue
        if p.suffix.lower() in TEXT_EXT:
            yield p


def main() -> int:
    root = Path(sys.argv[1]).resolve() if len(sys.argv) > 1 else ROOT
    changed: list[str] = []
    scanned = 0
    for p in iter_files(root):
        try:
            raw = p.read_bytes()
        except OSError:
            continue
        if b"\x00" in raw[:400]:
            continue
        try:
            text = raw.decode("utf-8")
        except UnicodeDecodeError:
            continue
        scanned += 1
        if not looks_corrupted(text) and not any(a in text for a, _ in REPLACEMENTS):
            continue
        new = fix_text(text)
        if new != text:
            # write UTF-8 without BOM, preserve LF
            data = new.encode("utf-8")
            p.write_bytes(data)
            changed.append(p.relative_to(root).as_posix())

    print(f"scanned={scanned} fixed={len(changed)} root={root}")
    for c in changed:
        print(" fixed", c)

    residual = []
    for p in iter_files(root):
        try:
            t = p.read_text(encoding="utf-8")
        except Exception:
            continue
        if looks_corrupted(t):
            residual.append(p.relative_to(root).as_posix())
    print(f"residual={len(residual)}")
    for r in residual[:40]:
        print(" residual", r)
        # show a snippet
        try:
            t = (root / r).read_text(encoding="utf-8")
            for line in t.splitlines():
                if "\u00e2" in line:
                    print("   ", repr(line[:100]))
                    break
        except Exception:
            pass
    return 0 if not residual else 1


if __name__ == "__main__":
    raise SystemExit(main())
