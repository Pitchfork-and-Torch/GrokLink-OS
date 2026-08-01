#!/usr/bin/env python3
"""Example: wire GrokLink observation tools into an OpenAI-style tool loop.

This script does NOT require the openai package. It:
1. Prints tools JSON you can paste into Assistants / chat.completions
2. Demonstrates local tool execution for assistant tool_calls
3. Optionally runs a scripted passive session (--session)

Usage:
  py -3 examples/openai_tools_example.py --mock
  py -3 examples/openai_tools_example.py --session --mock
  $env:GLK_SERIAL_PORT='COM5'; py -3 examples/openai_tools_example.py --session
"""

from __future__ import annotations

import argparse
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "bridge"))

from groklink_os.observe.agent_loop import (  # noqa: E402
    SYSTEM_PROMPT,
    execute_tool_calls,
    openai_chat_payload,
    run_scripted_observation_session,
    tools_for_anthropic,
    tools_for_openai,
)
from groklink_os.observe.tools import ToolDispatcher  # noqa: E402


class Mock:
    def edu_ack(self, phrase: str = "") -> dict:
        return {"ok": True, "edu": True}

    def status(self) -> dict:
        return {"ok": True, "version": "3.2.0", "api": 3, "edu": True, "radio": 0, "heap_free": 8000}

    def subghz_probe(self) -> dict:
        return {"ok": True, "partnum": 0, "version": 20, "hw": False}

    def subghz_rx(self, freq_hz: int = 433920000, ms: int = 200) -> dict:
        return {"ok": True, "freq_hz": freq_hz, "ms": ms, "pulses": 11, "rssi": -61, "sim": True}

    def spectrum(self, freqs, ms=200, settle_ms=50) -> dict:
        return {"ok": True, "bands": [{"freq_hz": f, "pulses": 4, "rssi": -70} for f in freqs]}

    def close(self) -> None:
        return None


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--mock", action="store_true")
    ap.add_argument("--session", action="store_true", help="Run scripted multi-step session")
    ap.add_argument("--print-tools", action="store_true", help="Print OpenAI tools JSON")
    ap.add_argument("--anthropic", action="store_true", help="Print Anthropic tool specs")
    args = ap.parse_args()

    if args.print_tools:
        print(json.dumps(tools_for_openai(), indent=2))
        return 0
    if args.anthropic:
        print(json.dumps(tools_for_anthropic(), indent=2))
        return 0

    print("=== system prompt (truncated) ===")
    print(SYSTEM_PROMPT[:400], "...\n")

    print("=== chat.completions request skeleton ===")
    print(json.dumps(openai_chat_payload("Scan my lab 433 band and summarize activity."), indent=2)[:800])
    print("...\n")

    d = ToolDispatcher(client=Mock() if args.mock else None)  # type: ignore[arg-type]
    try:
        if args.session:
            report = run_scripted_observation_session(
                d, freqs_hz=[433_920_000], dwell_ms=150, spectrum=True, monitor_chunks=0
            )
            print("=== scripted session narratives ===")
            for n in report.get("narratives") or []:
                print("-", n)
            return 0 if report.get("ok") else 1

        # Simulate an assistant tool_call the model would emit
        fake_assistant_tool_calls = [
            {
                "id": "call_rx1",
                "type": "function",
                "function": {
                    "name": "observe_rx",
                    "arguments": json.dumps({"freq_hz": 433920000, "ms": 200}),
                },
            }
        ]
        tool_msgs = execute_tool_calls(d, fake_assistant_tool_calls)
        print("=== executed tool messages (feed back to model) ===")
        print(json.dumps(tool_msgs, indent=2, default=str)[:1500])
    finally:
        d.close()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
