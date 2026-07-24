"""Host-side multi-turn observation agent helpers for OpenAI-style tool loops.

Does not call cloud LLMs by itself — packages tools + executes tool_calls so any
model (OpenAI, Claude, Grok, local) can drive the device as a sensory peripheral.
"""

from __future__ import annotations

import json
from typing import Any, Callable, Optional

from groklink_os.observe.tools import TOOL_DEFINITIONS, ToolDispatcher, tools_openai_format


SYSTEM_PROMPT = """You are a lab RF observation assistant for GrokLink OS.

Rules:
- Authorized research and owned equipment only.
- NOT a medical device — never claim clinical, diagnostic, or patient-care use.
- Use only passive observation tools (observe_rx, observe_spectrum, monitors, status,
  run_passive_mission for allowlisted lab/MedSec missions).
- Never attempt TX, GPIO, or system control. Never invent confirm tokens.
- Prefer narrative + occupancy + rssi + pulses when summarizing the signal world.
- Call get_observation_schema once if you are unsure of field meanings.
- Frequencies must be operator-approved lab bands.
- MedSec missions (medsec_lab_passive_ism, fac_rf_snapshot_passive, medsec_passive_watch)
  are passive research only under written RoE.

When tools return observations, explain what the RF environment looks like in plain language.
"""


def tools_for_openai() -> list[dict[str, Any]]:
    return tools_openai_format()


def tools_for_anthropic() -> list[dict[str, Any]]:
    """Convert OpenAI tools to Anthropic-style tool specs."""
    out = []
    for t in TOOL_DEFINITIONS:
        fn = t["function"]
        out.append(
            {
                "name": fn["name"],
                "description": fn["description"],
                "input_schema": fn["parameters"],
            }
        )
    return out


def execute_tool_calls(
    dispatcher: ToolDispatcher,
    tool_calls: list[dict[str, Any]],
) -> list[dict[str, Any]]:
    """Execute a list of OpenAI-format tool_calls; return tool result messages."""
    messages: list[dict[str, Any]] = []
    for tc in tool_calls:
        result = dispatcher.dispatch_openai_tool_call(tc)
        tc_id = tc.get("id") or tc.get("tool_call_id") or "call_0"
        messages.append(
            {
                "role": "tool",
                "tool_call_id": tc_id,
                "name": (tc.get("function") or {}).get("name") or tc.get("name"),
                "content": json.dumps(result, default=str),
            }
        )
    return messages


def run_scripted_observation_session(
    dispatcher: ToolDispatcher,
    *,
    freqs_hz: Optional[list[int]] = None,
    dwell_ms: int = 200,
    spectrum: bool = True,
    monitor_chunks: int = 0,
    monitor_interval_ms: int = 600,
) -> dict[str, Any]:
    """Deterministic multi-step passive session (no cloud LLM required).

    Useful for demos, CI, and seeding the learning store.
    """
    freqs = list(freqs_hz or [433_920_000])
    steps: list[dict[str, Any]] = []

    steps.append({"tool": "get_observation_schema", "result": dispatcher.dispatch("get_observation_schema")})
    steps.append(
        {
            "tool": "get_device_status",
            "result": dispatcher.dispatch("get_device_status", {"probe_radio": True}),
        }
    )
    for f in freqs:
        steps.append(
            {
                "tool": "observe_rx",
                "result": dispatcher.dispatch("observe_rx", {"freq_hz": f, "ms": dwell_ms}),
            }
        )
    if spectrum:
        steps.append(
            {
                "tool": "observe_spectrum",
                "result": dispatcher.dispatch(
                    "observe_spectrum",
                    {"freqs_hz": freqs, "ms": min(dwell_ms, 200), "settle_ms": 100},
                ),
            }
        )
    if monitor_chunks > 0:
        steps.append(
            {
                "tool": "start_monitor",
                "result": dispatcher.dispatch(
                    "start_monitor",
                    {
                        "freqs_hz": freqs,
                        "dwell_ms": min(dwell_ms, 150),
                        "interval_ms": monitor_interval_ms,
                        "chunk_size": 2,
                    },
                ),
            }
        )
        for _ in range(monitor_chunks):
            steps.append(
                {
                    "tool": "get_monitor_chunk",
                    "result": dispatcher.dispatch("get_monitor_chunk", {"wait_ms": 5000}),
                }
            )
        steps.append({"tool": "stop_monitor", "result": dispatcher.dispatch("stop_monitor")})

    steps.append(
        {
            "tool": "get_recent_activity",
            "result": dispatcher.dispatch("get_recent_activity", {"limit": 20}),
        }
    )

    narratives = []
    for s in steps:
        r = s["result"]
        res = r.get("result") if isinstance(r, dict) else None
        if isinstance(res, dict) and res.get("narrative"):
            narratives.append(res["narrative"])
        elif isinstance(r, dict) and isinstance(r.get("summary"), dict):
            narratives.append(r["summary"].get("narrative") or "")

    return {
        "ok": all(bool(s["result"].get("ok", True)) for s in steps if isinstance(s["result"], dict)),
        "steps": steps,
        "narratives": [n for n in narratives if n],
        "safety": {"tx": False, "path": "scripted_passive_session"},
    }


def openai_chat_payload(
    user_message: str,
    *,
    model: str = "gpt-4.1-mini",
    prior_messages: Optional[list[dict[str, Any]]] = None,
) -> dict[str, Any]:
    """Build a chat.completions.create body with GrokLink tools attached."""
    messages = [{"role": "system", "content": SYSTEM_PROMPT}]
    if prior_messages:
        messages.extend(prior_messages)
    messages.append({"role": "user", "content": user_message})
    return {
        "model": model,
        "messages": messages,
        "tools": tools_for_openai(),
        "tool_choice": "auto",
    }


def run_openai_tool_round(
    dispatcher: ToolDispatcher,
    assistant_message: dict[str, Any],
) -> list[dict[str, Any]]:
    """Given an assistant message that may contain tool_calls, execute them."""
    tool_calls = assistant_message.get("tool_calls") or []
    if not tool_calls:
        return []
    return execute_tool_calls(dispatcher, tool_calls)


def with_live_llm(
    complete_fn: Callable[[dict[str, Any]], dict[str, Any]],
    dispatcher: ToolDispatcher,
    user_message: str,
    *,
    model: str = "gpt-4.1-mini",
    max_rounds: int = 4,
) -> dict[str, Any]:
    """Drive a multi-round tool loop using a caller-supplied completions function.

    complete_fn(body) -> OpenAI-style chat completion JSON.
    Does not require the openai package; pass any HTTP wrapper.
    """
    body = openai_chat_payload(user_message, model=model)
    messages = list(body["messages"])
    transcript: list[dict[str, Any]] = []
    final_text = ""

    for _ in range(max_rounds):
        body = {
            "model": model,
            "messages": messages,
            "tools": tools_for_openai(),
            "tool_choice": "auto",
        }
        completion = complete_fn(body)
        transcript.append({"completion": completion})
        choice = (completion.get("choices") or [{}])[0]
        msg = choice.get("message") or {}
        messages.append(msg)
        tool_calls = msg.get("tool_calls") or []
        if not tool_calls:
            final_text = msg.get("content") or ""
            break
        tool_msgs = execute_tool_calls(dispatcher, tool_calls)
        messages.extend(tool_msgs)
        transcript.append({"tool_results": tool_msgs})
    else:
        final_text = "(max tool rounds reached)"

    return {
        "ok": True,
        "final": final_text,
        "messages": messages,
        "transcript": transcript,
        "safety": {"tx": False},
    }
