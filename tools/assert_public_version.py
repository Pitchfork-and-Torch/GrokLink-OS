"""Fail if public render templates or landing copy lag VERSION.

Raster pixels can still lie if nobody re-renders. This gate at least
blocks shipping HTML/templates that still advertise an old semver.
Run before landing deploy. Exit 2 on mismatch.
"""
from __future__ import annotations

import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
VERSION = (ROOT / "VERSION").read_text(encoding="utf-8").strip()
if not re.fullmatch(r"\d+\.\d+\.\d+", VERSION):
    print("BAD VERSION file:", VERSION)
    sys.exit(2)

files = [
    ROOT / "tools" / "render_architecture.html",
    ROOT / "tools" / "render_infographic.html",
    ROOT / "tools" / "render_tweet_features.html",
    ROOT / "landing" / "public" / "index.html",
    ROOT / "landing" / "public" / "llms.txt",
    ROOT / "landing" / "public" / "app.js",
]
# Stale major.minor that must not appear as current product chrome
stale = []
for p in files:
    text = p.read_text(encoding="utf-8")
    if VERSION not in text:
        stale.append(f"MISSING {VERSION} in {p.relative_to(ROOT)}")
    # Architecture/tweet/infographic templates must not headline an older x.y
    if p.name.startswith("render_") and f"GrokLink OS {VERSION.rsplit('.', 1)[0]}" not in text:
        # allow "GrokLink OS" without minor on infographic title
        if "render_infographic" not in p.name:
            if f"GrokLink OS {VERSION}" not in text and f"GrokLink OS {VERSION.rsplit('.', 1)[0]}" not in text:
                stale.append(f"NO HEADLINE VERSION in {p.relative_to(ROOT)}")

# Hard ban: live architecture template still saying 3.7 as current
arch = (ROOT / "tools" / "render_architecture.html").read_text(encoding="utf-8")
if "GrokLink OS 3.7" in arch or "v3.7.0 STABLE" in arch:
    stale.append("architecture template still headlines 3.7")

stamp = ROOT / "landing" / "public" / "assets" / "GRAPHICS-VERSION.txt"
if not stamp.exists():
    stale.append("missing landing/public/assets/GRAPHICS-VERSION.txt (re-run render_graphics)")
else:
    stamped = stamp.read_text(encoding="utf-8").strip()
    if stamped != VERSION:
        stale.append(f"GRAPHICS-VERSION.txt is {stamped}, VERSION is {VERSION}")

if stale:
    print("PUBLIC VERSION GATE FAIL")
    for s in stale:
        print(" -", s)
    sys.exit(2)
print("PUBLIC VERSION GATE OK", VERSION)
sys.exit(0)
