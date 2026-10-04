#!/usr/bin/env python3
"""Scrub emoji mojibake and fancy punctuation from GrokLink-OS text to pure ASCII-safe forms.

Never introduce real emoji. Never leave classic mojibake (UTF-8 misread as latin-1/CP1252).

Usage (repo root):
  py -3 scripts/scrub_emojibake_ascii.py --check
  py -3 scripts/scrub_emojibake_ascii.py
"""
from __future__ import annotations

import argparse
import re
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]

SKIP_SUFFIX = {
    ".png",
    ".jpg",
    ".jpeg",
    ".webp",
    ".gif",
    ".ico",
    ".bin",
    ".dfu",
    ".hex",
    ".woff",
    ".woff2",
    ".pdf",
    ".zip",
    ".7z",
    ".exe",
    ".dll",
    ".a",
    ".o",
    ".obj",
    ".lib",
    ".pdb",
}


def moj(utf8: bytes) -> str:
    """UTF-8 sequence as latin-1 characters (classic mojibake form)."""
    return utf8.decode("latin-1")


def moj_cp1252(utf8: bytes) -> str | None:
    try:
        return utf8.decode("cp1252")
    except UnicodeDecodeError:
        return None


# Real emoji -> ASCII tags (if any remain)
REAL_EMOJI: list[tuple[str, str]] = [
    ("\U0001f680", "[DEPLOY]"),
    ("\U0001f6a8", "[ALERT]"),
    ("\U0001f4e2", "[ALERT]"),
    ("\U0001f4a1", "[IDEA]"),
    ("\U0001f4bb", "[PC]"),
    ("\U0001f4cb", "[BRIEF]"),
    ("\U0001f6d1", "[KILL]"),
    ("\U0001f504", "[RESTART]"),
    ("\U0001f9e0", "[BRAIN]"),
    ("\U0001f4e5", "[IN]"),
    ("\U0001f4e4", "[OUT]"),
    ("\U0001f4e1", "[SYNC]"),
    ("\U0001f527", "[TOOL]"),
    ("\U0001f6e0", "[TOOL]"),
    ("\U0001f6e1", "[SHIELD]"),
    ("\U0001f512", "[LOCK]"),
    ("\U0001f513", "[UNLOCK]"),
    ("\U0001f50d", "[SEARCH]"),
    ("\U0001f4c4", "[DOC]"),
    ("\U0001f4dd", "[NOTE]"),
    ("\U0001f4be", "[SAVE]"),
    ("\U0001f4e6", "[PKG]"),
    ("\U0001f4ca", "[CHART]"),
    ("\U0001f4c8", "[UP]"),
    ("\U0001f4c9", "[DOWN]"),
    ("\U0001f3af", "[TARGET]"),
    ("\U0001f525", "[HOT]"),
    ("\U0001f4a5", "[BOOM]"),
    ("\U0001f44d", "[OK]"),
    ("\U0001f44e", "[NO]"),
    ("\u2705", "OK"),
    ("\u274c", "X"),
    ("\u26a0\ufe0f", "[WARN]"),
    ("\u26a0", "[WARN]"),
    ("\u26d4", "[DENY]"),
    ("\u23f1\ufe0f", "[TIMEOUT]"),
    ("\u23f1", "[TIMEOUT]"),
    ("\u25b6\ufe0f", "[OK]"),
    ("\u25b6", "[OK]"),
    ("\u2139\ufe0f", "[INFO]"),
    ("\u2139", "[INFO]"),
    ("\u2728", "*"),
    ("\u2b50", "*"),
    ("\u2764\ufe0f", ""),
    ("\u2764", ""),
    ("\U0001f31f", "*"),
    ("\U0001f389", "[OK]"),
    ("\U0001f44b", ""),
    ("\U0001f64c", ""),
    ("\U0001f64f", ""),
    ("\U0001f4af", "100"),
    ("\ufe0f", ""),
]

# Real punctuation we ban -> ASCII
REAL_PUNCT: list[tuple[str, str]] = [
    ("\u2014", " - "),
    ("\u2013", "-"),
    ("\u2012", "-"),
    ("\u2011", "-"),
    ("\u2018", "'"),
    ("\u2019", "'"),
    ("\u201c", '"'),
    ("\u201d", '"'),
    ("\u2026", "..."),
    ("\u00b7", " | "),
    ("\u2022", "-"),
    ("\u2192", "->"),
    ("\u2190", "<-"),
    ("\u21d2", "=>"),
    ("\u00a0", " "),
    ("\u200b", ""),
    ("\u200c", ""),
    ("\u200d", ""),
    ("\ufeff", ""),
    ("\ufffd", ""),
]


def build_mojibake_map() -> list[tuple[str, str]]:
    """Map mojibake strings of known emoji/punct to ASCII replacements."""
    out: list[tuple[str, str]] = []
    for ch, rep in REAL_EMOJI + REAL_PUNCT:
        if not ch:
            continue
        raw = ch.encode("utf-8")
        out.append((moj(raw), rep))
        m = moj_cp1252(raw)
        if m and m != ch and m != moj(raw):
            out.append((m, rep))
    # Double-encoded forms: UTF-8 of mojibake (utf-8 -> latin1 -> encode utf-8 again)
    # e.g. em dash mojibake bytes as utf-8 sequence
    for ch, rep in REAL_PUNCT[:6]:  # dashes/quotes mainly
        raw = ch.encode("utf-8")
        try:
            moj1 = raw.decode("cp1252")
        except UnicodeDecodeError:
            moj1 = raw.decode("latin-1")
        moj2 = moj1.encode("utf-8").decode("latin-1")
        if moj2 != moj1:
            out.append((moj2, rep))
    # Common 3-byte emoji lead F0 9F XX as partial cleanup: leave to broader strip
    return out


# Extra hard-coded common mojibake strings operators report
EXTRA_MOJ: list[tuple[str, str]] = [
    # rocket / siren etc often appear as multi-char latin-1; handled via build map
    # zero-width / BOM already above
]

# Regex: any remaining high-bit sequences that look like emoji mojibake
# F0 9F as latin-1 is \u00f0\u009f; after utf-8 roundtrip often \u00f0\u0178
EMOJI_MOJ_LEAD = re.compile(
    r"(?:\u00f0[\u0090-\u00bf\u0178\u0192\u2019\u201c\u201d\u00f0-\u00ff]"
    r"[\u0080-\u00bf]{0,4})"
)
# C2/C3/E2-prefixed garbage blocks of 2-4 chars that are not normal latin letters
# Strip remaining private-use / C1 controls left from mojibake
C1_CONTROLS = re.compile(r"[\u0080-\u009f]+")


def scrub_text(text: str) -> tuple[str, int]:
    n = 0
    out = text
    pairs = REAL_EMOJI + REAL_PUNCT + build_mojibake_map() + EXTRA_MOJ
    # longest first
    pairs = sorted({(a, b) for a, b in pairs if a}, key=lambda kv: len(kv[0]), reverse=True)
    for old, new in pairs:
        if old in out:
            c = out.count(old)
            out = out.replace(old, new)
            n += c
    # strip remaining emoji-like mojibake leads (4-byte UTF-8 as latin-1)
    out2, c = EMOJI_MOJ_LEAD.subn("", out)
    if c:
        out = out2
        n += c
    # strip leftover C1 controls (often mid-sequence junk)
    out2, c = C1_CONTROLS.subn("", out)
    if c:
        out = out2
        n += c
    # Only normalize the ASCII dash we insert (never collapse general whitespace/indent)
    while "  -  " in out:
        out = out.replace("  -  ", " - ")
        n += 1
    return out, n


def list_tracked() -> list[Path]:
    try:
        out = subprocess.check_output(
            ["git", "ls-files", "-z"],
            cwd=ROOT,
            text=False,
        )
        rels = [p.decode("utf-8", "surrogateescape") for p in out.split(b"\x00") if p]
    except Exception:
        rels = []
        for p in ROOT.rglob("*"):
            if p.is_file() and ".git" not in p.parts:
                rels.append(str(p.relative_to(ROOT)).replace("\\", "/"))
    paths = []
    for r in rels:
        p = ROOT / r
        if not p.is_file():
            continue
        if p.suffix.lower() in SKIP_SUFFIX:
            continue
        if any(x in p.parts for x in (".git", "node_modules", "third_party", "build-host", ".next")):
            # still scrub third_party comments? skip third_party to avoid upstream noise unless needed
            if "third_party" in p.parts:
                continue
        paths.append(p)
    return paths


def needs_scrub(text: str) -> bool:
    if any(ch in text for ch, _ in REAL_EMOJI + REAL_PUNCT):
        return True
    if moj(b"\xf0\x9f") in text:
        return True
    if "\u00e2\u20ac" in text:  # â€
        return True
    if "\ufffd" in text:
        return True
    if EMOJI_MOJ_LEAD.search(text):
        return True
    if C1_CONTROLS.search(text):
        return True
    # build map hits
    for old, _ in build_mojibake_map()[:20]:
        if old in text:
            return True
    return False


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--check", action="store_true")
    args = ap.parse_args()
    dirty_files = 0
    total = 0
    for p in list_tracked():
        try:
            raw = p.read_bytes()
        except OSError:
            continue
        if b"\x00" in raw[:1024]:
            continue
        try:
            text = raw.decode("utf-8")
        except UnicodeDecodeError:
            try:
                text = raw.decode("utf-8-sig")
            except UnicodeDecodeError:
                try:
                    text = raw.decode("cp1252")
                except UnicodeDecodeError:
                    continue
        new, n = scrub_text(text)
        if n == 0 or new == text:
            continue
        dirty_files += 1
        total += n
        rel = p.relative_to(ROOT).as_posix()
        print(f"{'NEED' if args.check else 'FIX '} {n:4d}  {rel}")
        if not args.check:
            p.write_bytes(new.encode("utf-8"))
    print(f"files={dirty_files} replacements={total} mode={'check' if args.check else 'write'}")
    return 1 if args.check and dirty_files else 0


if __name__ == "__main__":
    raise SystemExit(main())
