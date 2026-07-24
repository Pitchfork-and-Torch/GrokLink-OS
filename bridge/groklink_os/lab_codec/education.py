"""Security education content — concepts only, no attack tooling."""

from __future__ import annotations

from typing import Any


def rolling_code_education() -> dict[str, Any]:
    """
    Explain rolling / hopping codes for students.
    Explicitly does NOT provide prediction, cracking, or brand-specific decode.
    """
    return {
        "ok": True,
        "kind": "security_education",
        "topic": "rolling_codes",
        "title": "Why rolling codes exist (educational overview)",
        "sections": [
            {
                "heading": "The replay problem",
                "body": (
                    "If a remote always sends the same RF bitstream, an attacker who records "
                    "one transmission can replay that recording later to open the same lock. "
                    "GrokLink's lab beacon intentionally uses a plain counter so students can "
                    "replay an identical hex capture and see this failure mode safely on "
                    "owned lab gear."
                ),
            },
            {
                "heading": "What rolling codes change",
                "body": (
                    "Rolling (hopping) codes change the authenticating value each button press, "
                    "typically with a cryptographic construction shared between transmitter and "
                    "receiver. A pure RF replay of an old capture should no longer be accepted."
                ),
            },
            {
                "heading": "What this project will not do",
                "body": (
                    "GrokLink OS does not implement rolling-code prediction, KeeLoq recovery, "
                    "seed extraction, brand remote libraries, or third-party access-control "
                    "decode. Those capabilities enable unauthorized access and are out of scope."
                ),
            },
            {
                "heading": "What you may practice here",
                "body": (
                    "1) Encode/decode GrokLink lab beacons (GLK1) on owned equipment. "
                    "2) Demonstrate replay of a fixed educational counter. "
                    "3) Study edge timing statistics without claiming protocol identity. "
                    "4) Read threat-model docs for authorized research design."
                ),
            },
        ],
        "allowed_lab_activities": [
            "encode_glk1_beacon",
            "decode_glk1_beacon",
            "replay_demo_same_counter",
            "edge_timing_stats",
            "human_gated_lab_tx_when_device_supports",
        ],
        "forbidden": [
            "rolling_code_prediction",
            "third_party_remote_decode",
            "access_control_cloning",
            "vehicle_immobilizer_attacks",
            "seed_bruteforce",
        ],
        "narrative": (
            "Educational summary: fixed codes are replayable; rolling codes mitigate replay. "
            "GrokLink teaches the problem with owned-lab GLK1 beacons and does not predict "
            "or crack commercial rolling codes."
        ),
        "safety": {
            "tx": False,
            "attack_tooling": False,
            "authorized_education_only": True,
        },
    }
