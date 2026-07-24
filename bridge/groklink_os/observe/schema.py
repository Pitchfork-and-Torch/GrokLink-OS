"""Self-describing signal observation schema for LLM consumption.

Schema ID: groklink.signal_observation.v2 (compatible with v1 field set)

Observations are host-packaged JSON objects. Device RPC remains compact;
rich narrative, occupancy, calibration, and rolling summaries are produced
on the PC bridge. Observation paths never transmit and never decode payloads.
"""

from __future__ import annotations

import time
import uuid
from datetime import datetime, timezone
from enum import Enum
from typing import Any, Optional


OBSERVATION_SCHEMA_ID = "groklink.signal_observation.v2"
SCHEMA_VERSION = 2
SCHEMA_COMPAT = ["groklink.signal_observation.v1", "groklink.signal_observation.v2"]

# v1 alias for older callers
OBSERVATION_SCHEMA_ID_V1 = "groklink.signal_observation.v1"

EVENT_TAXONOMY: list[dict[str, str]] = [
    {
        "type": "quiet_dwell",
        "meaning": "No edges in dwell; ambient quiet relative to baseline.",
    },
    {
        "type": "edge_activity",
        "meaning": "GDO0 edge counts during dwell (light RX; not a decoded frame).",
    },
    {
        "type": "elevated_vs_noise",
        "meaning": "Activity or RSSI elevated versus host/device noise-floor baseline.",
    },
    {
        "type": "band_hotspot",
        "meaning": "Band ranked hottest in a multi-band scan (relative only).",
    },
    {
        "type": "noise_floor_sample",
        "meaning": "Sample used to update host calibration baseline for a band.",
    },
    {
        "type": "sample_ref",
        "meaning": "Reference to another observation_id (monitor chunks).",
    },
]


class ObservationKind(str, Enum):
    RX_SNAPSHOT = "rx_snapshot"
    SPECTRUM_SCAN = "spectrum_scan"
    MONITOR_CHUNK = "monitor_chunk"
    DEVICE_STATUS = "device_status"
    ACTIVITY_SUMMARY = "activity_summary"
    SCHEMA_DESC = "schema_desc"
    NOISE_FLOOR = "noise_floor"
    BAND_COMPARE = "band_compare"
    CALIBRATION_STATE = "calibration_state"


def utc_now_iso() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def host_mono_ms() -> int:
    return int(time.monotonic() * 1000)


def new_observation_id() -> str:
    return uuid.uuid4().hex


def band_label(freq_hz: int) -> str:
    if freq_hz < 350_000_000:
        return "315-class"
    if freq_hz < 650_000_000:
        return "433-class"
    if freq_hz < 920_000_000:
        return "868-class"
    return "915-class"


def pulse_rate_hz(pulses: Optional[int], dwell_ms: int) -> float:
    p = max(0, int(pulses or 0))
    return round(p * 1000.0 / max(dwell_ms, 1), 3)


def occupancy_from_activity(
    pulses: Optional[int],
    rssi_dbm: Optional[int],
    *,
    dwell_ms: int = 400,
) -> str:
    """Map crude edge/pulse + RSSI into an LLM-friendly occupancy label (v1 heuristic)."""
    p = int(pulses or 0)
    r = rssi_dbm
    rate = p * 100.0 / max(dwell_ms, 1)
    if r is not None and r > -45 and rate >= 5:
        return "high"
    if rate >= 40 or (r is not None and r > -55 and rate >= 10):
        return "high"
    if rate >= 8 or (r is not None and r > -70 and rate >= 3):
        return "medium"
    if rate >= 1 or (r is not None and r > -85 and p > 0):
        return "low"
    if p == 0 and (r is None or r < -90):
        return "quiet"
    if p == 0:
        return "quiet"
    return "unknown"


def energy_score(pulses: Optional[int], rssi_dbm: Optional[int], dwell_ms: int = 400) -> float:
    """0.0–1.0 composite for ranking activity (host-side only)."""
    p = max(0, int(pulses or 0))
    rate = min(1.0, (p * 100.0 / max(dwell_ms, 1)) / 80.0)
    if rssi_dbm is None:
        rssi_part = 0.0
    else:
        rssi_part = max(0.0, min(1.0, (float(rssi_dbm) + 110.0) / 70.0))
    return round(0.65 * rate + 0.35 * rssi_part, 4)


def calibrated_occupancy(
    *,
    pulse_rate: float,
    rssi_dbm: Optional[int],
    baseline_pulse_rate: Optional[float],
    noise_floor_dbm: Optional[float],
) -> tuple[str, float, Optional[float]]:
    """
    Relative occupancy vs baseline.

    Returns (label, confidence, snr_est_db).
    Labels: quiet | ambient | elevated | busy | unknown
    """
    snr: Optional[float] = None
    if rssi_dbm is not None and noise_floor_dbm is not None:
        snr = round(float(rssi_dbm) - float(noise_floor_dbm), 2)

    if baseline_pulse_rate is None and noise_floor_dbm is None:
        # Fall back to absolute heuristic mapped into calibrated vocabulary
        abs_occ = occupancy_from_activity(
            int(pulse_rate * 0.4),  # approximate pulses for 400ms
            rssi_dbm,
            dwell_ms=400,
        )
        mapping = {
            "quiet": "quiet",
            "low": "ambient",
            "medium": "elevated",
            "high": "busy",
            "unknown": "unknown",
        }
        return mapping.get(abs_occ, "unknown"), 0.35, snr

    br = float(baseline_pulse_rate or 0.0)
    # rate elevation factor
    if br <= 0.5:
        elev = pulse_rate
    else:
        elev = pulse_rate / max(br, 0.1)

    conf = 0.55
    if baseline_pulse_rate is not None and noise_floor_dbm is not None:
        conf = 0.85
    elif baseline_pulse_rate is not None or noise_floor_dbm is not None:
        conf = 0.7

    # RSSI elevation helps when rates are noisy
    if snr is not None and snr >= 12:
        conf = min(1.0, conf + 0.1)

    if pulse_rate < 2 and (snr is None or snr < 3):
        return "quiet", conf, snr
    if elev < 1.5 and (snr is None or snr < 4):
        return "ambient", conf, snr
    if elev < 4.0 or (snr is not None and snr < 10):
        return "elevated", conf, snr
    return "busy", conf, snr


def build_observation(
    kind: ObservationKind | str,
    *,
    device: Optional[dict[str, Any]] = None,
    policy_context: Optional[dict[str, Any]] = None,
    rf: Optional[dict[str, Any]] = None,
    activity: Optional[dict[str, Any]] = None,
    events: Optional[list[dict[str, Any]]] = None,
    spectrum: Optional[dict[str, Any]] = None,
    window: Optional[dict[str, Any]] = None,
    stats: Optional[dict[str, Any]] = None,
    calibration: Optional[dict[str, Any]] = None,
    raw_device: Optional[dict[str, Any]] = None,
    narrative: Optional[str] = None,
    extra: Optional[dict[str, Any]] = None,
    device_mono_ms: Optional[int] = None,
    observation_id: Optional[str] = None,
) -> dict[str, Any]:
    """Build a self-describing observation object (v2, v1-compatible fields)."""
    kind_s = kind.value if isinstance(kind, ObservationKind) else str(kind)
    obs: dict[str, Any] = {
        "schema": OBSERVATION_SCHEMA_ID,
        "schema_version": SCHEMA_VERSION,
        "schema_compat": list(SCHEMA_COMPAT),
        "observation_id": observation_id or new_observation_id(),
        "kind": kind_s,
        "timestamps": {
            "utc": utc_now_iso(),
            "device_mono_ms": device_mono_ms,
            "host_mono_ms": host_mono_ms(),
        },
        "device": device or {},
        "policy_context": policy_context
        or {
            "passive_only": True,
            "edu_required": True,
            "tx_available": False,
            "note": "Observations never trigger TX / GPIO / system actions.",
        },
        "safety": {
            "path": "passive_rx",
            "tx": False,
            "gpio": False,
            "system": False,
            "decode": False,
            "audited": True,
            "authorized_use_only": True,
        },
    }
    if rf is not None:
        obs["rf"] = rf
    if activity is not None:
        obs["activity"] = activity
    if events is not None:
        obs["events"] = events
    if spectrum is not None:
        obs["spectrum"] = spectrum
    if window is not None:
        obs["window"] = window
    if stats is not None:
        obs["stats"] = stats
    if calibration is not None:
        obs["calibration"] = calibration
    if raw_device is not None:
        obs["raw_device"] = raw_device
    if narrative is not None:
        obs["narrative"] = narrative
    if extra:
        obs.update(extra)
    return obs


def schema_description() -> dict[str, Any]:
    """Machine-readable description for tools that call get_observation_schema."""
    return build_observation(
        ObservationKind.SCHEMA_DESC,
        narrative=(
            "GrokLink OS signal observations v2: structured JSON for passive SubGHz activity. "
            "Includes v1 fields plus stats (pulse_rate), calibration (noise floor / SNR est), "
            "calibrated_occupancy, and a non-decode event taxonomy. Never TX, never payload decode."
        ),
        extra={
            "fields": {
                "schema": OBSERVATION_SCHEMA_ID,
                "schema_version": SCHEMA_VERSION,
                "schema_compat": SCHEMA_COMPAT,
                "kind": [k.value for k in ObservationKind],
                "timestamps.utc": "ISO-8601 UTC",
                "rf.freq_hz": "Center frequency in Hz",
                "stats.pulse_rate_hz": "Edges per second over dwell",
                "activity.pulses": "GDO0 edge / pulse count during dwell",
                "activity.rssi_dbm": "Estimated RSSI when available",
                "activity.occupancy": "quiet|low|medium|high|unknown (absolute heuristic)",
                "activity.calibrated_occupancy": "quiet|ambient|elevated|busy|unknown (vs baseline)",
                "calibration.noise_floor_dbm": "Host/device baseline RSSI for band",
                "calibration.snr_est_db": "rssi - noise_floor when both known",
                "spectrum.bands": "Per-frequency activity vector",
                "events": "Non-decode taxonomy only; payload_hex always null",
                "safety": "Always passive; tx=false; decode=false",
            },
            "event_taxonomy": EVENT_TAXONOMY,
            "tools": [
                "get_observation_schema",
                "get_event_taxonomy",
                "get_device_status",
                "get_calibration_state",
                "observe_noise_floor",
                "observe_rx",
                "observe_spectrum",
                "observe_compare",
                "start_monitor",
                "stop_monitor",
                "get_monitor_chunk",
                "get_recent_activity",
                "list_missions",
                "list_skills",
                "run_passive_mission",
                "get_mission_status",
                "start_offline_agent",
                "stop_offline_agent",
                "get_agent_status",
                "get_vault_tail",
            ],
            "legal": "Authorized research / owned equipment only. edu_ack required for RX.",
            "migration_v1_to_v2": {
                "breaking": False,
                "notes": [
                    "schema id changed to v2; schema_compat lists v1",
                    "all v1 fields still present",
                    "new optional blocks: stats, calibration, calibrated_occupancy, confidence",
                    "event types expanded; payload_hex remains null",
                ],
            },
        },
    )


def event_taxonomy_description() -> dict[str, Any]:
    return build_observation(
        ObservationKind.SCHEMA_DESC,
        narrative=(
            "Event taxonomy for light RX: edge counts and relative elevation only. "
            "No protocol decode, no payload_hex, no transmitter identification."
        ),
        extra={
            "event_taxonomy": EVENT_TAXONOMY,
            "forbidden": [
                "decoded_frame",
                "payload_hex_non_null",
                "protocol_id",
                "rolling_code",
                "device_identity_from_rf",
            ],
            "safety": {"tx": False, "decode": False},
        },
    )
