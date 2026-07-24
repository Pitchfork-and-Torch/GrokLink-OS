"""Owned-lab codec + education tests (no hardware, no third-party decode)."""

from __future__ import annotations

from groklink_os.lab_codec import (
    LabBeacon,
    analyze_edge_timing,
    decode_beacon_bytes,
    decode_beacon_hex,
    decode_ook_edges_to_beacon,
    encode_beacon,
    encode_beacon_to_ook_edges,
    rolling_code_education,
)
from groklink_os.lab_codec.beacon import demo_replay
from groklink_os.observe.tools import TOOL_DEFINITIONS, ToolDispatcher


def test_round_trip_hex() -> None:
    b = LabBeacon(lab_id=42, counter=3, message="HELLO")
    raw = encode_beacon(b)
    r = decode_beacon_bytes(raw)
    assert r["ok"] is True
    assert r["beacon"]["lab_id"] == 42
    assert r["beacon"]["counter"] == 3
    assert r["beacon"]["message"] == "HELLO"
    assert r["beacon"]["rolling_code"] is False
    assert r["safety"]["third_party_decode"] is False


def test_reject_non_lab() -> None:
    r = decode_beacon_hex("deadbeefcafebabe")
    assert r["ok"] is False
    assert r["lab_beacon"] is False
    assert "third_party" in r["note"].lower() or r.get("third_party_decode") is False


def test_ook_round_trip() -> None:
    b = LabBeacon(lab_id=1, counter=9, message="LAB")
    ook = encode_beacon_to_ook_edges(b)
    assert ook["ok"]
    r = decode_ook_edges_to_beacon(ook["edges"])
    assert r["ok"] is True
    assert r["beacon"]["counter"] == 9


def test_replay_demo_lesson() -> None:
    a = LabBeacon(lab_id=1, counter=5, message="X")
    same = demo_replay(a, LabBeacon(lab_id=1, counter=5, message="X"))
    diff = demo_replay(a, LabBeacon(lab_id=1, counter=6, message="X"))
    assert same["same_bytes"] is True
    assert diff["same_bytes"] is False
    assert "rolling" in same["lesson"].lower()


def test_timing_and_education() -> None:
    edges = [{"level": 1, "us": 500}, {"level": 0, "us": 250}, {"level": 1, "us": 250}]
    t = analyze_edge_timing(edges)
    assert t["ok"] and t["protocol_id"] is None
    edu = rolling_code_education()
    assert edu["ok"]
    assert "rolling_code_prediction" in edu["forbidden"]
    assert edu["safety"]["attack_tooling"] is False


def test_tools_lab_codec() -> None:
    names = {t["function"]["name"] for t in TOOL_DEFINITIONS}
    for n in (
        "lab_beacon_encode",
        "lab_beacon_decode",
        "lab_replay_demo",
        "explain_rolling_codes",
        "analyze_edge_timing",
    ):
        assert n in names
    d = ToolDispatcher()
    try:
        enc = d.dispatch("lab_beacon_encode", {"lab_id": 2, "counter": 4, "message": "OK"})
        assert enc["ok"]
        hx = enc["result"]["hex"]
        dec = d.dispatch("lab_beacon_decode", {"hex": hx})
        assert dec["ok"]
        assert dec["result"]["beacon"]["message"] == "OK"
        bad = d.dispatch("lab_beacon_decode", {"hex": "00112233"})
        assert bad["ok"] is False
        edu = d.dispatch("explain_rolling_codes")
        assert edu["ok"]
        rep = d.dispatch("lab_replay_demo", {"counter": 1})
        assert rep["result"]["same_bytes"] is True
    finally:
        d.close()
