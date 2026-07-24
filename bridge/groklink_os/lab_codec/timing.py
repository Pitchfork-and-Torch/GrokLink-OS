"""Modulation literacy: edge timing stats without protocol identification."""

from __future__ import annotations

from typing import Any, Optional


def analyze_edge_timing(
    edges: list[dict[str, Any]],
    *,
    source: str = "lab_or_synthetic",
) -> dict[str, Any]:
    """
    Summarize high/low pulse timing for education.
    Does NOT identify brands, protocols, or rolling codes.
    """
    highs: list[int] = []
    lows: list[int] = []
    for e in edges:
        us = int(e.get("us") or 0)
        if us <= 0:
            continue
        if int(e.get("level", 0)) == 1:
            highs.append(us)
        else:
            lows.append(us)

    def stats(xs: list[int]) -> dict[str, Optional[float]]:
        if not xs:
            return {"count": 0, "min_us": None, "max_us": None, "mean_us": None}
        return {
            "count": len(xs),
            "min_us": min(xs),
            "max_us": max(xs),
            "mean_us": round(sum(xs) / len(xs), 2),
        }

    total_us = sum(int(e.get("us") or 0) for e in edges)
    narrative = (
        f"Edge timing summary ({source}): {len(highs)} high / {len(lows)} low segments, "
        f"total≈{total_us} µs. Stats only — no protocol identification."
    )
    return {
        "ok": True,
        "kind": "edge_timing_summary",
        "source": source,
        "high": stats(highs),
        "low": stats(lows),
        "total_us": total_us,
        "segment_count": len(edges),
        "protocol_id": None,
        "rolling_code": False,
        "safety": {
            "tx": False,
            "third_party_decode": False,
            "protocol_identification": False,
        },
        "narrative": narrative,
    }
