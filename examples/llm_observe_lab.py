#!/usr/bin/env python3
"""Example: multi-turn passive RF observation for an LLM / operator.

Uses host-sim RPC (default 127.0.0.1:7341) or a mock if --mock.
Authorized research only — never transmits.

Usage:
  # with host OS sim running:
  py -3 examples/llm_observe_lab.py

  # offline demo (no device):
  py -3 examples/llm_observe_lab.py --mock
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

# allow running from repo root without install
ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "bridge"))

from groklink_os.observe.tools import ToolDispatcher, tools_openai_format  # noqa: E402


class MockClient:
    def edu_ack(self, phrase: str = "") -> dict:
        return {"ok": True, "edu": True}

    def status(self) -> dict:
        return {"ok": True, "version": "3.2.0", "api": 3, "edu": True, "radio": 0, "heap_free": 9999}

    def subghz_probe(self) -> dict:
        return {"ok": True, "partnum": 0, "version": 20, "hw": False}

    def subghz_rx(self, freq_hz: int = 433_920_000, ms: int = 400) -> dict:
        return {
            "ok": True,
            "freq_hz": freq_hz,
            "ms": ms,
            "pulses": 22,
            "rssi": -58,
            "sim": True,
            "ts_ms": 1000,
        }

    def spectrum(self, freqs, ms=400, settle_ms=2000) -> dict:
        return {
            "ok": True,
            "ms": ms,
            "settle_ms": settle_ms,
            "bands": [{"freq_hz": f, "pulses": 10, "rssi": -70} for f in freqs],
        }

    def close(self) -> None:
        return None


def main() -> int:
    ap = argparse.ArgumentParser(description="LLM-oriented passive observation demo")
    ap.add_argument("--mock", action="store_true", help="Use mock device (no RPC)")
    ap.add_argument("--freq", type=int, default=433_920_000)
    args = ap.parse_args()

    print("=== OpenAI tool names ===")
    print([t["function"]["name"] for t in tools_openai_format()])

    if args.mock:
        d = ToolDispatcher(client=MockClient())  # type: ignore[arg-type]
    else:
        d = ToolDispatcher()

    try:
        print("\n=== get_device_status ===")
        print(json.dumps(d.dispatch("get_device_status"), indent=2, default=str)[:1200])

        print("\n=== observe_rx ===")
        rx = d.dispatch("observe_rx", {"freq_hz": args.freq, "ms": 200})
        print(rx["result"]["narrative"])
        print("occupancy:", rx["result"]["activity"]["occupancy"])

        print("\n=== observe_spectrum ===")
        sp = d.dispatch(
            "observe_spectrum",
            {"freqs_hz": [315000000, args.freq], "ms": 100, "settle_ms": 50},
        )
        print(sp["result"]["narrative"])

        print("\n=== get_recent_activity ===")
        act = d.dispatch("get_recent_activity", {"limit": 5})
        print(act["summary"]["narrative"])
    finally:
        d.close()

    print("\nDone. Observations stored under ~/.grok/state/groklink-os/ (unless overridden).")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
