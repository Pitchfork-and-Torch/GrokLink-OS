#!/usr/bin/env python3
"""Shared paths for GrokLink OS adaptive skill (portable; no machine PII)."""
from __future__ import annotations

import os
from pathlib import Path


REPO_DEFAULT = "Pitchfork-and-Torch/GrokLink-OS"
REPO_URL = f"https://github.com/{REPO_DEFAULT}"
RAW_BASE = f"https://raw.githubusercontent.com/{REPO_DEFAULT}/main"
API_BASE = f"https://api.github.com/repos/{REPO_DEFAULT}"

EDU_PHRASE = "I_WILL_USE_ONLY_AUTHORIZED_TARGETS"


def skill_root() -> Path:
    return Path(__file__).resolve().parent.parent


def learn_root() -> Path:
    """Mutable learning store — outside the shareable skill tree."""
    env = os.environ.get("GROKLINK_OS_LEARN_DIR")
    if env:
        p = Path(env).expanduser()
    else:
        p = Path.home() / ".grok" / "state" / "groklink-os"
    p.mkdir(parents=True, exist_ok=True)
    for sub in ("sessions", "captures", "notes", "drafts", "observations", "audit"):
        (p / sub).mkdir(parents=True, exist_ok=True)
    return p


def firmware_snapshot_path() -> Path:
    return learn_root() / "firmware_snapshot.json"


def capabilities_path() -> Path:
    return learn_root() / "capabilities.json"


def learnings_index_path() -> Path:
    return learn_root() / "learnings_index.json"


def band_notes_path() -> Path:
    return learn_root() / "band_notes.json"


def local_repo_root() -> Path | None:
    env = os.environ.get("GROKLINK_OS_ROOT")
    if env:
        p = Path(env).expanduser()
        if (p / "VERSION").exists() or (p / "bridge").exists():
            return p
    # Common relative: skill packaged inside repo agent-skill/groklink-os
    cand = skill_root().parent.parent
    if (cand / "VERSION").exists() and (cand / "bridge").exists():
        return cand
    home_cand = Path.home() / "GrokLink-OS"
    if (home_cand / "VERSION").exists():
        return home_cand
    return None
