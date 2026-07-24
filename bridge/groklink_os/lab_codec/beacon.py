"""GrokLink Lab Beacon (GLK1) — educational fixed format for owned equipment only.

Frame layout (little-endian):
  magic[4]     = b'GLK1'
  version[u8]  = 1
  flags[u8]    = bit0 educational demo
  lab_id[u16]  = operator-chosen lab id (0..65535)
  counter[u32] = monotonic educational counter (teaches replay; NOT a rolling code)
  msg_len[u8]  = 0..32
  message[N]   = UTF-8 lab text
  crc16[u16]   = CRC-16/CCITT-FALSE over all prior bytes

This is intentionally NOT KeeLoq / HCS / AES hopping. Counter is plain and
replayable so students can observe why real systems use cryptographic rolling codes.
"""

from __future__ import annotations

import struct
from dataclasses import dataclass
from typing import Any, Optional

LAB_MAGIC = b"GLK1"
LAB_PROTOCOL_ID = "groklink.lab_beacon.v1"
LAB_VERSION = 1
MAX_MSG = 32
FLAG_EDU_DEMO = 0x01


def crc16_ccitt(data: bytes, init: int = 0xFFFF) -> int:
    crc = init & 0xFFFF
    for b in data:
        crc ^= b << 8
        for _ in range(8):
            if crc & 0x8000:
                crc = ((crc << 1) ^ 0x1021) & 0xFFFF
            else:
                crc = (crc << 1) & 0xFFFF
    return crc


@dataclass
class LabBeacon:
    lab_id: int = 1
    counter: int = 0
    message: str = "LAB"
    version: int = LAB_VERSION
    flags: int = FLAG_EDU_DEMO

    def to_dict(self) -> dict[str, Any]:
        return {
            "protocol": LAB_PROTOCOL_ID,
            "magic": LAB_MAGIC.decode("ascii"),
            "version": self.version,
            "flags": self.flags,
            "lab_id": self.lab_id,
            "counter": self.counter,
            "message": self.message,
            "educational": True,
            "rolling_code": False,
            "note": "Fixed educational format for owned-lab demos only. Not a rolling code.",
        }


def encode_beacon(beacon: LabBeacon) -> bytes:
    msg = beacon.message.encode("utf-8")
    if len(msg) > MAX_MSG:
        raise ValueError(f"message longer than {MAX_MSG} bytes")
    if not (0 <= int(beacon.lab_id) <= 0xFFFF):
        raise ValueError("lab_id out of range")
    if not (0 <= int(beacon.counter) <= 0xFFFFFFFF):
        raise ValueError("counter out of range")
    body = (
        LAB_MAGIC
        + bytes([LAB_VERSION, int(beacon.flags) & 0xFF])
        + struct.pack("<H", int(beacon.lab_id) & 0xFFFF)
        + struct.pack("<I", int(beacon.counter) & 0xFFFFFFFF)
        + bytes([len(msg)])
        + msg
    )
    c = crc16_ccitt(body)
    return body + struct.pack("<H", c)


def encode_beacon_hex(beacon: LabBeacon) -> str:
    return encode_beacon(beacon).hex()


def decode_beacon_bytes(data: bytes) -> dict[str, Any]:
    """Decode only GLK1 lab beacons. Rejects all other payloads."""
    if not data:
        return {
            "ok": False,
            "error": "empty",
            "lab_beacon": False,
            "third_party_decode": False,
            "note": "No data. Third-party RF payload decode is not supported.",
        }
    if len(data) < 4 or data[:4] != LAB_MAGIC:
        return {
            "ok": False,
            "error": "not_lab_beacon",
            "lab_beacon": False,
            "third_party_decode": False,
            "magic_seen": data[:4].hex() if len(data) >= 4 else data.hex(),
            "note": (
                "Payload is not a GrokLink lab beacon (GLK1). "
                "GrokLink refuses third-party / remote / access-control decode "
                "and does not implement rolling-code prediction."
            ),
        }
    if len(data) < 14:
        return {"ok": False, "error": "truncated", "lab_beacon": True}
    version = data[4]
    flags = data[5]
    lab_id = struct.unpack_from("<H", data, 6)[0]
    counter = struct.unpack_from("<I", data, 8)[0]
    msg_len = data[12]
    if msg_len > MAX_MSG:
        return {"ok": False, "error": "msg_len_invalid", "lab_beacon": True}
    need = 13 + msg_len + 2
    if len(data) < need:
        return {"ok": False, "error": "truncated", "lab_beacon": True}
    msg = data[13 : 13 + msg_len]
    crc_got = struct.unpack_from("<H", data, 13 + msg_len)[0]
    crc_exp = crc16_ccitt(data[: 13 + msg_len])
    if crc_got != crc_exp:
        return {
            "ok": False,
            "error": "crc_mismatch",
            "lab_beacon": True,
            "crc_got": crc_got,
            "crc_expected": crc_exp,
        }
    try:
        text = msg.decode("utf-8")
    except UnicodeDecodeError:
        text = msg.hex()
    beacon = LabBeacon(
        lab_id=lab_id,
        counter=counter,
        message=text,
        version=version,
        flags=flags,
    )
    return {
        "ok": True,
        "lab_beacon": True,
        "protocol": LAB_PROTOCOL_ID,
        "beacon": beacon.to_dict(),
        "hex": data[:need].hex(),
        "safety": {
            "tx": False,
            "third_party_decode": False,
            "rolling_code_prediction": False,
            "owned_lab_only": True,
        },
        "narrative": (
            f"Decoded GrokLink lab beacon GLK1: lab_id={lab_id}, counter={counter}, "
            f"message={text!r}. Educational fixed format (replayable). Not a rolling code."
        ),
    }


def decode_beacon_hex(hex_str: str) -> dict[str, Any]:
    h = hex_str.strip().lower().replace("0x", "").replace(" ", "")
    try:
        raw = bytes.fromhex(h)
    except ValueError:
        return {"ok": False, "error": "invalid_hex", "lab_beacon": False}
    return decode_beacon_bytes(raw)


def demo_replay(first: LabBeacon, second: LabBeacon) -> dict[str, Any]:
    """Educational demo: identical frames replay; different counters are distinct frames."""
    a = encode_beacon(first)
    b = encode_beacon(second)
    return {
        "ok": True,
        "educational": True,
        "same_bytes": a == b,
        "first_hex": a.hex(),
        "second_hex": b.hex(),
        "lesson": (
            "If two transmissions use the same fixed counter/message, a replay of the first "
            "capture is indistinguishable from the second. Real rolling-code systems change "
            "an authenticating code each press so plain replay fails. GrokLink demonstrates "
            "the problem with a plain counter; it does not implement or predict cryptographic "
            "hopping codes."
        ),
    }
