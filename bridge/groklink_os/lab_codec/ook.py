"""Simple educational OOK edge encoding for GLK1 lab beacons (host-side).

Timing is didactic (not matching any commercial remote). Used for:
- round-trip encode/decode tests
- offline fixtures
- optional future on-air lab TX of owned beacons only
"""

from __future__ import annotations

from typing import Any

from .beacon import decode_beacon_bytes, encode_beacon, LabBeacon

# Educational timings (microseconds) — NOT a clone of any product
T_SHORT = 250
T_LONG = 500
T_GAP = 250
PREAMBLE_ONES = 16


def bits_to_edges(bits: str) -> list[dict[str, int]]:
    """Convert '0'/'1' string to list of {level: 0|1, us: int}."""
    edges: list[dict[str, int]] = []
    for ch in bits:
        if ch == "1":
            edges.append({"level": 1, "us": T_LONG})
        elif ch == "0":
            edges.append({"level": 1, "us": T_SHORT})
        else:
            continue
        edges.append({"level": 0, "us": T_GAP})
    return edges


def edges_to_bits(edges: list[dict[str, Any]]) -> str:
    bits: list[str] = []
    for e in edges:
        if int(e.get("level", 0)) != 1:
            continue
        us = int(e.get("us") or 0)
        # nearest of short/long
        if abs(us - T_LONG) <= abs(us - T_SHORT):
            bits.append("1")
        else:
            bits.append("0")
    return "".join(bits)


def bytes_to_bits(data: bytes) -> str:
    return "".join(f"{b:08b}" for b in data)


def bits_to_bytes(bits: str) -> bytes:
    # pad to multiple of 8
    pad = (-len(bits)) % 8
    bits = bits + ("0" * pad)
    out = bytearray()
    for i in range(0, len(bits), 8):
        out.append(int(bits[i : i + 8], 2))
    return bytes(out)


def encode_beacon_to_ook_edges(beacon: LabBeacon) -> dict[str, Any]:
    raw = encode_beacon(beacon)
    preamble = "1" * PREAMBLE_ONES
    bits = preamble + bytes_to_bits(raw)
    edges = bits_to_edges(bits)
    return {
        "ok": True,
        "protocol": "groklink.lab_beacon.v1",
        "educational": True,
        "hex": raw.hex(),
        "bit_count": len(bits),
        "preamble_ones": PREAMBLE_ONES,
        "edges": edges,
        "timing_us": {"short": T_SHORT, "long": T_LONG, "gap": T_GAP},
        "beacon": beacon.to_dict(),
        "note": "Didactic OOK timing for owned-lab demos only.",
    }


def decode_ook_edges_to_beacon(edges: list[dict[str, Any]]) -> dict[str, Any]:
    bits = edges_to_bits(edges)
    # strip preamble ones
    i = 0
    while i < len(bits) and bits[i] == "1":
        i += 1
    # require some preamble
    if i < 8:
        # try without strip if magic still possible
        body_bits = bits
    else:
        body_bits = bits[i:]
        # if we stripped only high bits of first data byte, also try sliding window
    # Sliding search for GLK1 magic in bit stream
    magic_bits = bytes_to_bits(b"GLK1")
    idx = body_bits.find(magic_bits)
    if idx < 0:
        idx = bits.find(magic_bits)
        search = bits
    else:
        search = body_bits
    if idx < 0:
        return {
            "ok": False,
            "error": "lab_magic_not_found",
            "lab_beacon": False,
            "third_party_decode": False,
            "note": (
                "No GLK1 lab magic in edge stream. Third-party remote decode is not supported."
            ),
            "bit_len": len(bits),
        }
    payload_bits = search[idx:]
    raw = bits_to_bytes(payload_bits)
    # trim to CRC end by trying decode with increasing length is hard; use fixed min size then CRC
    # Lab frame min 14 bytes, max 14+32=46
    for n in range(14, min(len(raw), 46) + 1):
        result = decode_beacon_bytes(raw[:n])
        if result.get("ok"):
            result["edges_decoded"] = True
            result["bits_used"] = n * 8
            return result
    return {
        "ok": False,
        "error": "lab_frame_not_recovered",
        "lab_beacon": False,
        "note": "Edges present but GLK1 CRC did not validate. Not attempting third-party decode.",
    }
