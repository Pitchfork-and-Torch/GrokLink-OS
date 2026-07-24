"""v3.5 Signal Cognition — host unit tests (no hardware)."""

from __future__ import annotations

from pathlib import Path

from groklink_os.observe.calibration import CalibrationStore
from groklink_os.observe.packager import ObservationPackager
from groklink_os.observe.schema import (
    EVENT_TAXONOMY,
    OBSERVATION_SCHEMA_ID,
    SCHEMA_VERSION,
    calibrated_occupancy,
    event_taxonomy_description,
    pulse_rate_hz,
    schema_description,
)
from groklink_os.observe.tools import TOOL_DEFINITIONS, ToolDispatcher


def test_schema_v2_identity() -> None:
    assert OBSERVATION_SCHEMA_ID == "groklink.signal_observation.v2"
    assert SCHEMA_VERSION == 2
    desc = schema_description()
    assert desc["schema"] == OBSERVATION_SCHEMA_ID
    assert desc["schema_version"] == 2
    assert "groklink.signal_observation.v1" in desc["schema_compat"]
    assert desc["safety"]["tx"] is False
    assert desc["safety"]["decode"] is False
    assert desc["policy_context"]["passive_only"] is True


def test_pulse_rate_and_calibrated_occupancy() -> None:
    assert pulse_rate_hz(300, 100) == 3000.0
    label, conf, snr = calibrated_occupancy(
        pulse_rate=10.0,
        rssi_dbm=-100,
        baseline_pulse_rate=8.0,
        noise_floor_dbm=-110,
    )
    assert label in ("ambient", "elevated", "busy", "quiet")
    assert snr == 10.0
    assert conf >= 0.5


def test_packager_noise_floor_and_rx(tmp_path: Path) -> None:
    cal = CalibrationStore(path=tmp_path / "cal.json")
    pack = ObservationPackager(calibration=cal)
    quiet = {
        "ok": True,
        "freq_hz": 433920000,
        "ms": 200,
        "pulses": 0,
        "rssi": -118,
        "sim": True,
        "ts_ms": 1,
    }
    nf = pack.package_rx(quiet, as_noise_floor=True)
    assert nf["kind"] == "noise_floor"
    assert nf["safety"]["tx"] is False
    assert any(e["type"] == "noise_floor_sample" for e in nf["events"])
    assert nf["calibration"]["noise_floor_dbm"] is not None

    busy = {
        "ok": True,
        "freq_hz": 433920000,
        "ms": 300,
        "pulses": 6000,
        "rssi": -90,
        "pulse_rate_hz": 20000,
        "rssi_min": -95,
        "rssi_max": -88,
        "sim": False,
        "ts_ms": 2,
    }
    rx = pack.package_rx(busy)
    assert rx["schema"] == OBSERVATION_SCHEMA_ID
    assert "stats" in rx
    assert rx["stats"]["pulse_rate_hz"] == 20000
    assert rx["activity"]["calibrated_occupancy"] in ("elevated", "busy", "ambient")
    assert rx["safety"]["tx"] is False
    assert all(e.get("payload_hex") is None for e in rx["events"])


def test_compare_and_taxonomy(tmp_path: Path) -> None:
    cal = CalibrationStore(path=tmp_path / "cal2.json")
    pack = ObservationPackager(calibration=cal)
    a = {"ok": True, "freq_hz": 433920000, "ms": 200, "pulses": 100, "rssi": -70, "sim": True}
    b = {"ok": True, "freq_hz": 315000000, "ms": 200, "pulses": 5, "rssi": -100, "sim": True}
    cmp_ = pack.package_compare(a, b, freq_a=433920000, freq_b=315000000, ms=200)
    assert cmp_["kind"] == "band_compare"
    assert cmp_["window"]["hotter"] in ("a", "b")
    tax = event_taxonomy_description()
    assert len(EVENT_TAXONOMY) >= 5
    assert "event_taxonomy" in tax


def test_tools_include_v35_and_dispatch_mock(tmp_path: Path) -> None:
    names = [t["function"]["name"] for t in TOOL_DEFINITIONS]
    for n in (
        "get_event_taxonomy",
        "get_calibration_state",
        "observe_noise_floor",
        "observe_compare",
        "observe_rx",
    ):
        assert n in names
    # original 16 still present
    for n in ("observe_rx", "observe_spectrum", "get_vault_tail", "start_offline_agent"):
        assert n in names
    assert len(names) >= 20

    class Mock:
        def edu_ack(self, phrase: str = "") -> dict:
            return {"ok": True, "edu": True}

        def status(self) -> dict:
            return {"ok": True, "version": "3.5.0", "api": 5, "edu": True, "heap_free": 4096}

        def subghz_probe(self) -> dict:
            return {"ok": True, "hw": True, "version": 20}

        def subghz_rx(self, freq_hz: int = 433920000, ms: int = 400) -> dict:
            return {
                "ok": True,
                "freq_hz": freq_hz,
                "ms": ms,
                "pulses": 12,
                "rssi": -95,
                "pulse_rate_hz": 30,
                "rssi_min": -98,
                "rssi_max": -93,
                "sim": True,
                "ts_ms": 9,
            }

        def close(self) -> None:
            return None

    d = ToolDispatcher(client=Mock())
    try:
        r = d.dispatch("get_event_taxonomy")
        assert r["ok"] and r["safety"]["tx"] is False
        r2 = d.dispatch("observe_noise_floor", {"freq_hz": 433920000, "ms": 150})
        assert r2["ok"]
        assert r2["result"]["kind"] == "noise_floor"
        r3 = d.dispatch("observe_compare", {"freq_a_hz": 433920000, "freq_b_hz": 315000000})
        assert r3["ok"]
        assert r3["result"]["kind"] == "band_compare"
        r4 = d.dispatch("get_calibration_state")
        assert r4["ok"]
    finally:
        d.close()
