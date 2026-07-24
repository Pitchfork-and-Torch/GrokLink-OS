"""PHI / PII hygiene for MedSec labels (best-effort, not a HIPAA control).

Rejects common patterns that must never appear in engagement IDs, site labels,
case titles, or notes. Lab labels only.
"""

from __future__ import annotations

import re
from typing import Iterable

# Best-effort patterns — not exhaustive medical NLP.
_PATTERNS: list[tuple[str, re.Pattern[str]]] = [
    ("ssn", re.compile(r"\b\d{3}-\d{2}-\d{4}\b")),
    ("ssn_compact", re.compile(r"\b\d{9}\b")),
    ("mrn_label", re.compile(r"\b(mrn|medical\s*record|patient\s*id)\b", re.I)),
    ("dob_label", re.compile(r"\b(dob|date\s*of\s*birth)\b", re.I)),
    ("email", re.compile(r"\b[A-Za-z0-9._%+-]+@[A-Za-z0-9.-]+\.[A-Za-z]{2,}\b")),
    ("phone_us", re.compile(r"\b(?:\+?1[-.\s]?)?\(?\d{3}\)?[-.\s]?\d{3}[-.\s]?\d{4}\b")),
    ("patient_name_hint", re.compile(r"\bpatient\s+[A-Z][a-z]+\s+[A-Z][a-z]+\b")),
    ("phi_keyword", re.compile(r"\b(phi|hipaa|ehr\s*chart|diagnosis\s+of)\b", re.I)),
]


class PhiHygieneError(ValueError):
    """Raised when a field looks like it may contain PHI/PII."""


def find_phi_hits(text: str) -> list[str]:
    if not text:
        return []
    hits: list[str] = []
    for name, rx in _PATTERNS:
        if rx.search(text):
            hits.append(name)
    return hits


def assert_no_phi(*fields: str, labels: Iterable[str] | None = None) -> None:
    """Raise PhiHygieneError if any field matches PHI-like patterns."""
    labs = list(labels) if labels is not None else [f"field{i}" for i in range(len(fields))]
    for lab, val in zip(labs, fields):
        hits = find_phi_hits(val or "")
        if hits:
            raise PhiHygieneError(
                f"Possible PHI/PII pattern in '{lab}': {', '.join(hits)}. "
                "Use lab labels only (no patient identifiers)."
            )


def sanitize_or_reject(text: str, *, field: str = "field") -> str:
    assert_no_phi(text, labels=[field])
    return (text or "").strip()
