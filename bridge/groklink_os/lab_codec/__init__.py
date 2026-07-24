"""Owned-lab educational codec — GrokLink lab beacons only.

Never decodes third-party remotes, access-control, or rolling codes.
Never predicts hopping codes.
"""

from .beacon import (
    LAB_MAGIC,
    LAB_PROTOCOL_ID,
    LabBeacon,
    decode_beacon_bytes,
    decode_beacon_hex,
    encode_beacon,
    encode_beacon_hex,
)
from .ook import edges_to_bits, bits_to_edges, decode_ook_edges_to_beacon, encode_beacon_to_ook_edges
from .education import rolling_code_education
from .timing import analyze_edge_timing

__all__ = [
    "LAB_MAGIC",
    "LAB_PROTOCOL_ID",
    "LabBeacon",
    "decode_beacon_bytes",
    "decode_beacon_hex",
    "encode_beacon",
    "encode_beacon_hex",
    "edges_to_bits",
    "bits_to_edges",
    "decode_ook_edges_to_beacon",
    "encode_beacon_to_ook_edges",
    "rolling_code_education",
    "analyze_edge_timing",
]
