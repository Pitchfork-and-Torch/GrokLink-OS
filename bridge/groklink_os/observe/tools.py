"""OpenAI-compatible tool definitions and dispatch for signal observation."""

from __future__ import annotations

import json
from typing import Any, Callable, Optional

from groklink_os.observe.monitor import MonitorSession
from groklink_os.observe.packager import ObservationPackager
from groklink_os.observe.schema import event_taxonomy_description, schema_description
from groklink_os.observe.store import ObservationStore
from groklink_os.rpc.client import GrokLinkClient, open_client


# OpenAI Chat Completions / Assistants function tools
TOOL_DEFINITIONS: list[dict[str, Any]] = [
    {
        "type": "function",
        "function": {
            "name": "get_observation_schema",
            "description": (
                "Return the self-describing GrokLink signal observation schema and tool list. "
                "Call once at session start so you understand observation fields."
            ),
            "parameters": {"type": "object", "properties": {}, "additionalProperties": False},
        },
    },
    {
        "type": "function",
        "function": {
            "name": "get_device_status",
            "description": (
                "Get live device/policy status and optional CC1101 probe. Passive only. "
                "Ensures edu_ack session is open for subsequent RX."
            ),
            "parameters": {
                "type": "object",
                "properties": {
                    "probe_radio": {
                        "type": "boolean",
                        "description": "If true, also run subghz_probe (default true).",
                        "default": True,
                    }
                },
                "additionalProperties": False,
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "observe_rx",
            "description": (
                "Take a single passive SubGHz RX snapshot at freq_hz for ms milliseconds. "
                "Returns a structured signal observation (pulses, RSSI, occupancy, narrative). "
                "Never transmits. Requires authorized lab frequency only."
            ),
            "parameters": {
                "type": "object",
                "properties": {
                    "freq_hz": {
                        "type": "integer",
                        "description": "Center frequency in Hz (e.g. 433920000).",
                        "default": 433_920_000,
                    },
                    "ms": {
                        "type": "integer",
                        "description": "Dwell time milliseconds (device may cap ~100–500 ms).",
                        "default": 400,
                        "minimum": 50,
                        "maximum": 5000,
                    },
                },
                "additionalProperties": False,
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "observe_spectrum",
            "description": (
                "Sequential multi-band passive spectrum scan. Returns per-band activity vector "
                "with occupancy and hottest band. Passive only."
            ),
            "parameters": {
                "type": "object",
                "properties": {
                    "freqs_hz": {
                        "type": "array",
                        "items": {"type": "integer"},
                        "description": "List of frequencies in Hz.",
                        "default": [315_000_000, 433_920_000, 868_350_000],
                    },
                    "ms": {
                        "type": "integer",
                        "description": "Per-band dwell ms.",
                        "default": 400,
                    },
                    "settle_ms": {
                        "type": "integer",
                        "description": "Settle between bands (host/device planner).",
                        "default": 2000,
                    },
                },
                "additionalProperties": False,
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "start_monitor",
            "description": (
                "Start a host-side passive monitor session that periodically samples one or more "
                "frequencies and emits observation chunks. Poll with get_monitor_chunk. Never TX."
            ),
            "parameters": {
                "type": "object",
                "properties": {
                    "freqs_hz": {
                        "type": "array",
                        "items": {"type": "integer"},
                        "default": [433_920_000],
                    },
                    "dwell_ms": {"type": "integer", "default": 200},
                    "interval_ms": {
                        "type": "integer",
                        "description": "Spacing between samples (USB-safe).",
                        "default": 800,
                    },
                    "chunk_size": {
                        "type": "integer",
                        "description": "Samples per monitor_chunk observation.",
                        "default": 4,
                    },
                },
                "additionalProperties": False,
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "stop_monitor",
            "description": "Stop the active passive monitor session and flush remaining samples.",
            "parameters": {"type": "object", "properties": {}, "additionalProperties": False},
        },
    },
    {
        "type": "function",
        "function": {
            "name": "get_monitor_chunk",
            "description": (
                "Fetch the next ready monitor chunk (monitor_chunk observation). "
                "Optionally wait up to wait_ms for a chunk."
            ),
            "parameters": {
                "type": "object",
                "properties": {
                    "wait_ms": {
                        "type": "integer",
                        "description": "Max wait for a chunk (0 = non-blocking).",
                        "default": 0,
                    }
                },
                "additionalProperties": False,
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "get_recent_activity",
            "description": (
                "Return recent stored observations and a rolling activity summary from this host. "
                "Local-only data; never leaves the operator machine by default."
            ),
            "parameters": {
                "type": "object",
                "properties": {
                    "limit": {"type": "integer", "default": 20, "minimum": 1, "maximum": 100},
                    "kind": {
                        "type": "string",
                        "description": "Optional filter: rx_snapshot|spectrum_scan|monitor_chunk|...",
                    },
                },
                "additionalProperties": False,
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "list_missions",
            "description": (
                "List ROM/SD passive lab missions available on the device/agent "
                "(e.g. lab_passive_433). Does not arm or run."
            ),
            "parameters": {"type": "object", "properties": {}, "additionalProperties": False},
        },
    },
    {
        "type": "function",
        "function": {
            "name": "list_skills",
            "description": "List registered skills (ROM catalog and/or SD). Read-only.",
            "parameters": {"type": "object", "properties": {}, "additionalProperties": False},
        },
    },
    {
        "type": "function",
        "function": {
            "name": "run_passive_mission",
            "description": (
                "Arm a passive lab/MedSec mission by id and run up to N IR steps on-device/host. "
                "Only allowlisted passive missions (RX/log/infer). Never TX. "
                "MedSec ids: medsec_lab_passive_ism, fac_rf_snapshot_passive, medsec_passive_watch. "
                "NOT a medical device."
            ),
            "parameters": {
                "type": "object",
                "properties": {
                    "mission_id": {
                        "type": "string",
                        "description": "Mission id, e.g. lab_passive_433 or medsec_lab_passive_ism",
                        "default": "lab_passive_433",
                    },
                    "steps": {
                        "type": "integer",
                        "description": "Max IR steps to execute (default 8, max 32).",
                        "default": 8,
                    },
                },
                "additionalProperties": False,
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "get_mission_status",
            "description": "Get active or named mission status (pc, state, last_pulses, infer).",
            "parameters": {
                "type": "object",
                "properties": {
                    "mission_id": {
                        "type": "string",
                        "description": "Optional mission id; omit for active mission.",
                    }
                },
                "additionalProperties": False,
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "start_offline_agent",
            "description": (
                "Enable USB-safe offline autonomous looping for an allowlisted passive mission "
                "(lab_passive_watch recommended). Device ticks one IR step ~600ms when USB idle. Never TX."
            ),
            "parameters": {
                "type": "object",
                "properties": {
                    "mission_id": {
                        "type": "string",
                        "default": "lab_passive_watch",
                    }
                },
                "additionalProperties": False,
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "stop_offline_agent",
            "description": "Disable offline autonomous agent ticking and disarm active mission.",
            "parameters": {"type": "object", "properties": {}, "additionalProperties": False},
        },
    },
    {
        "type": "function",
        "function": {
            "name": "get_agent_status",
            "description": "Offline flag, active mission, vault size, step counters.",
            "parameters": {"type": "object", "properties": {}, "additionalProperties": False},
        },
    },
    {
        "type": "function",
        "function": {
            "name": "get_vault_tail",
            "description": (
                "Return recent RAM vault events (mission RX/infer/done) from device/host. "
                "Local lab data only; no SD required."
            ),
            "parameters": {
                "type": "object",
                "properties": {"n": {"type": "integer", "default": 8, "minimum": 1, "maximum": 16}},
                "additionalProperties": False,
            },
        },
    },
    # --- v3.5 Signal Cognition (additive; still passive-only) ---
    {
        "type": "function",
        "function": {
            "name": "get_event_taxonomy",
            "description": (
                "Return the non-decode event taxonomy for light RX observations. "
                "Explains quiet_dwell, edge_activity, elevated_vs_noise, band_hotspot, noise_floor_sample. "
                "Never provides protocol decode."
            ),
            "parameters": {"type": "object", "properties": {}, "additionalProperties": False},
        },
    },
    {
        "type": "function",
        "function": {
            "name": "get_calibration_state",
            "description": (
                "Return host-side band calibration baselines (noise floor, baseline pulse rate). "
                "Local-only; passive. Call after observe_noise_floor."
            ),
            "parameters": {"type": "object", "properties": {}, "additionalProperties": False},
        },
    },
    {
        "type": "function",
        "function": {
            "name": "observe_noise_floor",
            "description": (
                "Take a short passive RX sample and update the host noise-floor / pulse-rate baseline "
                "for that band. Prefer a quiet lab moment. Never TX. Never decodes payloads."
            ),
            "parameters": {
                "type": "object",
                "properties": {
                    "freq_hz": {
                        "type": "integer",
                        "default": 433_920_000,
                        "description": "Authorized lab frequency Hz.",
                    },
                    "ms": {
                        "type": "integer",
                        "default": 200,
                        "minimum": 50,
                        "maximum": 500,
                    },
                },
                "additionalProperties": False,
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "observe_compare",
            "description": (
                "Passively sample two frequencies and return a band_compare observation with "
                "which band is hotter by energy_score / calibrated occupancy. Never TX."
            ),
            "parameters": {
                "type": "object",
                "properties": {
                    "freq_a_hz": {"type": "integer", "default": 433_920_000},
                    "freq_b_hz": {"type": "integer", "default": 315_000_000},
                    "ms": {"type": "integer", "default": 250, "minimum": 50, "maximum": 500},
                },
                "additionalProperties": False,
            },
        },
    },
    # --- v3.6 Lab Codec education (owned GLK1 only; no third-party / rolling-code tools) ---
    {
        "type": "function",
        "function": {
            "name": "lab_beacon_encode",
            "description": (
                "Encode a GrokLink educational lab beacon (GLK1) for owned-lab demos. "
                "Fixed format with plain counter (replayable by design). NOT a rolling code. "
                "Does not encode third-party remotes."
            ),
            "parameters": {
                "type": "object",
                "properties": {
                    "lab_id": {"type": "integer", "default": 1},
                    "counter": {"type": "integer", "default": 1},
                    "message": {"type": "string", "default": "LAB"},
                },
                "additionalProperties": False,
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "lab_beacon_decode",
            "description": (
                "Decode ONLY GrokLink GLK1 lab beacon hex or edges. "
                "Rejects non-GLK1 payloads. Never decodes third-party remotes or rolling codes."
            ),
            "parameters": {
                "type": "object",
                "properties": {
                    "hex": {
                        "type": "string",
                        "description": "Hex string of candidate frame (GLK1 only).",
                    },
                    "edges": {
                        "type": "array",
                        "description": "Optional OOK edges [{level, us}] from lab encoder.",
                        "items": {"type": "object"},
                    },
                },
                "additionalProperties": False,
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "lab_replay_demo",
            "description": (
                "Educational demo: show that identical GLK1 frames are byte-identical (replayable). "
                "Teaches why rolling codes exist. Does NOT predict commercial hopping codes."
            ),
            "parameters": {
                "type": "object",
                "properties": {
                    "lab_id": {"type": "integer", "default": 1},
                    "counter": {"type": "integer", "default": 7},
                    "message": {"type": "string", "default": "LAB"},
                    "counter_b": {
                        "type": "integer",
                        "description": "Second counter; if omitted, uses same as counter for replay match.",
                    },
                },
                "additionalProperties": False,
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "explain_rolling_codes",
            "description": (
                "Security-education overview of rolling/hopping codes and the replay problem. "
                "Concepts only — no prediction, cracking, or brand remote libraries."
            ),
            "parameters": {"type": "object", "properties": {}, "additionalProperties": False},
        },
    },
    {
        "type": "function",
        "function": {
            "name": "analyze_edge_timing",
            "description": (
                "Modulation literacy: summarize high/low edge timings. "
                "No protocol identification and no third-party decode."
            ),
            "parameters": {
                "type": "object",
                "properties": {
                    "edges": {
                        "type": "array",
                        "items": {"type": "object"},
                        "description": "List of {level:0|1, us:int}",
                    }
                },
                "required": ["edges"],
                "additionalProperties": False,
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "prepare_unplugged_explore",
            "description": (
                "Arm the device for unplugged passive exploration: edu_ack, mark "
                "lab_passive_watch autonomous, enable offline agent. After this succeeds, "
                "USB may be unplugged; on-device GrokAgent continues passive RX missions. "
                "PC/LLM Grok cannot command the device without a link. Never TX. "
                "On reconnect, call ingest_unplugged_lessons / plug-sync."
            ),
            "parameters": {
                "type": "object",
                "properties": {
                    "mission_id": {
                        "type": "string",
                        "default": "lab_passive_watch",
                        "description": "Allowlisted passive mission only.",
                    }
                },
                "additionalProperties": False,
            },
        },
    },
    {
        "type": "function",
        "function": {
            "name": "ingest_unplugged_lessons",
            "description": (
                "On USB reconnect: pull vault/agent/status from the device, analyze unplugged "
                "field activity, and document lessons in the local PC learning store "
                "(field_trips, research journal, learnings index). Run every time the Flipper "
                "is plugged in after unplugged explore. Never TX. Local research only."
            ),
            "parameters": {
                "type": "object",
                "properties": {
                    "clear_vault": {
                        "type": "boolean",
                        "default": False,
                        "description": "If true, clear RAM vault after successful ingest.",
                    }
                },
                "additionalProperties": False,
            },
        },
    },
]


def tools_openai_format() -> list[dict[str, Any]]:
    return list(TOOL_DEFINITIONS)


def tools_as_json_schema_map() -> dict[str, Any]:
    out: dict[str, Any] = {}
    for t in TOOL_DEFINITIONS:
        fn = t["function"]
        out[fn["name"]] = {
            "description": fn["description"],
            "parameters": fn["parameters"],
        }
    return out


class ToolDispatcher:
    """Execute observation tools against a live or mock GrokLinkClient."""

    def __init__(
        self,
        client: Optional[GrokLinkClient] = None,
        *,
        client_factory: Optional[Callable[[], GrokLinkClient]] = None,
        store: Optional[ObservationStore] = None,
        packager: Optional[ObservationPackager] = None,
        auto_edu: bool = True,
    ) -> None:
        self._client = client
        self._client_factory = client_factory
        self.store = store or ObservationStore()
        self.packager = packager or ObservationPackager()
        self.auto_edu = auto_edu
        self._edu_done = False
        self._monitor: Optional[MonitorSession] = None
        self._owns_client = False

    def close(self) -> None:
        if self._monitor and self._monitor.running:
            self._monitor.stop()
        if self._owns_client and self._client is not None:
            self._client.close()
            self._client = None

    def _get_client(self) -> GrokLinkClient:
        if self._client is not None:
            return self._client
        if self._client_factory:
            self._client = self._client_factory()
            self._owns_client = True
            return self._client
        # Prefer GLK_SERIAL_PORT / COM host for real device; else TCP host-sim
        self._client = open_client()
        self._owns_client = True
        return self._client

    def _ensure_edu(self, c: GrokLinkClient) -> None:
        if not self.auto_edu or self._edu_done:
            return
        r = c.edu_ack()
        self._edu_done = bool(r.get("ok") and r.get("edu"))
        self.store.audit("edu_ack", {"ok": self._edu_done})

    def dispatch(self, name: str, arguments: Optional[dict[str, Any]] = None) -> dict[str, Any]:
        args = dict(arguments or {})
        self.store.audit("tool_call", {"name": name, "arguments": args})
        try:
            result = self._dispatch_inner(name, args)
        except Exception as exc:  # noqa: BLE001
            result = {
                "ok": False,
                "error": str(exc),
                "tool": name,
                "safety": {"tx": False},
            }
            self.store.audit("tool_error", {"name": name, "error": str(exc)})
            return result
        self.store.audit("tool_result", {"name": name, "ok": result.get("ok", True)})
        return result

    def _dispatch_inner(self, name: str, args: dict[str, Any]) -> dict[str, Any]:
        if name == "get_observation_schema":
            return {"ok": True, "result": schema_description(), "tools": tools_as_json_schema_map()}

        if name == "get_event_taxonomy":
            return {
                "ok": True,
                "result": event_taxonomy_description(),
                "safety": {"tx": False, "decode": False},
            }

        # --- Lab codec education (host-only; no third-party decode) ---
        if name == "lab_beacon_encode":
            from groklink_os.lab_codec import LabBeacon, encode_beacon_hex, encode_beacon_to_ook_edges

            b = LabBeacon(
                lab_id=int(args.get("lab_id") or 1),
                counter=int(args.get("counter") or 1),
                message=str(args.get("message") or "LAB"),
            )
            ook = encode_beacon_to_ook_edges(b)
            return {
                "ok": True,
                "result": {
                    "hex": encode_beacon_hex(b),
                    "beacon": b.to_dict(),
                    "ook": ook,
                    "narrative": (
                        f"Encoded GLK1 lab beacon lab_id={b.lab_id} counter={b.counter} "
                        f"message={b.message!r}. Educational fixed format."
                    ),
                },
                "safety": {
                    "tx": False,
                    "third_party_decode": False,
                    "rolling_code_prediction": False,
                    "owned_lab_only": True,
                },
            }

        if name == "lab_beacon_decode":
            from groklink_os.lab_codec import decode_beacon_hex, decode_ook_edges_to_beacon

            if args.get("edges"):
                result = decode_ook_edges_to_beacon(list(args["edges"]))
            elif args.get("hex"):
                result = decode_beacon_hex(str(args["hex"]))
            else:
                result = {
                    "ok": False,
                    "error": "need_hex_or_edges",
                    "note": "Provide GLK1 hex or lab OOK edges only.",
                }
            return {
                "ok": bool(result.get("ok")),
                "result": result,
                "safety": {
                    "tx": False,
                    "third_party_decode": False,
                    "rolling_code_prediction": False,
                },
            }

        if name == "lab_replay_demo":
            from groklink_os.lab_codec import LabBeacon
            from groklink_os.lab_codec.beacon import demo_replay

            c1 = int(args.get("counter") or 7)
            c2 = args.get("counter_b")
            c2i = int(c2) if c2 is not None else c1
            lid = int(args.get("lab_id") or 1)
            msg = str(args.get("message") or "LAB")
            result = demo_replay(
                LabBeacon(lab_id=lid, counter=c1, message=msg),
                LabBeacon(lab_id=lid, counter=c2i, message=msg),
            )
            return {
                "ok": True,
                "result": result,
                "safety": {
                    "tx": False,
                    "rolling_code_prediction": False,
                    "educational_replay_only": True,
                },
            }

        if name == "explain_rolling_codes":
            from groklink_os.lab_codec import rolling_code_education

            return {
                "ok": True,
                "result": rolling_code_education(),
                "safety": {"tx": False, "attack_tooling": False},
            }

        if name == "analyze_edge_timing":
            from groklink_os.lab_codec import analyze_edge_timing

            edges = list(args.get("edges") or [])
            result = analyze_edge_timing(edges, source="tool_input")
            return {"ok": True, "result": result, "safety": result.get("safety")}

        if name == "get_calibration_state":
            obs = self.packager.package_calibration_state()
            self.store.append(obs)
            return {"ok": True, "result": obs, "safety": {"tx": False}}

        if name == "get_recent_activity":
            limit = int(args.get("limit") or 20)
            kind = args.get("kind")
            recent = self.store.recent(limit, kind=kind)
            summary = self.store.summarize_recent(limit)
            obs = self.packager.package_activity_summary(summary)
            self.store.append(obs)
            return {"ok": True, "summary": obs, "observations": recent}

        if name == "stop_monitor":
            if not self._monitor:
                return {"ok": True, "result": {"running": False, "note": "no active monitor"}}
            return {"ok": True, "result": self._monitor.stop()}

        if name == "get_monitor_chunk":
            if not self._monitor:
                return {"ok": False, "error": "no_active_monitor"}
            wait_ms = int(args.get("wait_ms") or 0)
            chunk = self._monitor.get_chunk(wait_ms=wait_ms)
            if chunk is None:
                return {"ok": True, "result": None, "status": self._monitor.status()}
            return {"ok": True, "result": chunk, "status": self._monitor.status()}

        c = self._get_client()
        self._ensure_edu(c)

        if name == "get_device_status":
            st = c.status()
            probe = None
            if args.get("probe_radio", True):
                try:
                    probe = c.subghz_probe()
                except Exception:  # noqa: BLE001
                    probe = None
            obs = self.packager.package_status(st, probe=probe)
            self.store.append(obs)
            return {"ok": True, "result": obs}

        if name == "observe_rx":
            freq = int(args.get("freq_hz") or 433_920_000)
            ms = int(args.get("ms") or 400)
            raw = c.subghz_rx(freq_hz=freq, ms=ms)
            obs = self.packager.package_rx(
                raw, request={"freq_hz": freq, "ms": ms}, update_calibration=False
            )
            self.store.append(obs)
            return {"ok": bool(raw.get("ok", True)), "result": obs}

        if name == "observe_noise_floor":
            freq = int(args.get("freq_hz") or 433_920_000)
            ms = int(args.get("ms") or 200)
            raw = c.subghz_rx(freq_hz=freq, ms=ms)
            obs = self.packager.package_rx(
                raw,
                request={"freq_hz": freq, "ms": ms},
                as_noise_floor=True,
            )
            self.store.append(obs)
            return {"ok": bool(raw.get("ok", True)), "result": obs, "safety": {"tx": False}}

        if name == "observe_compare":
            fa = int(args.get("freq_a_hz") or 433_920_000)
            fb = int(args.get("freq_b_hz") or 315_000_000)
            ms = int(args.get("ms") or 250)
            raw_a = c.subghz_rx(freq_hz=fa, ms=ms)
            raw_b = c.subghz_rx(freq_hz=fb, ms=ms)
            obs = self.packager.package_compare(raw_a, raw_b, freq_a=fa, freq_b=fb, ms=ms)
            self.store.append(obs)
            return {"ok": True, "result": obs, "safety": {"tx": False}}

        if name == "observe_spectrum":
            freqs = args.get("freqs_hz") or [315_000_000, 433_920_000, 868_350_000]
            freqs = [int(x) for x in freqs]
            ms = int(args.get("ms") or 400)
            settle = int(args.get("settle_ms") or 2000)
            raw = c.spectrum(freqs, ms=ms, settle_ms=settle)
            obs = self.packager.package_spectrum(
                raw, request={"freqs": freqs, "ms": ms, "settle_ms": settle}
            )
            self.store.append(obs)
            return {"ok": bool(raw.get("ok", True)), "result": obs}

        if name == "start_monitor":
            if self._monitor and self._monitor.running:
                self._monitor.stop()
            freqs = [int(x) for x in (args.get("freqs_hz") or [433_920_000])]
            dwell = int(args.get("dwell_ms") or 200)
            interval = int(args.get("interval_ms") or 800)
            chunk_size = int(args.get("chunk_size") or 4)

            def rx_fn(freq_hz: int, ms: int) -> dict[str, Any]:
                # each sample reuses same connection path
                self._ensure_edu(c)
                return c.subghz_rx(freq_hz=freq_hz, ms=ms)

            self._monitor = MonitorSession(
                rx_fn,
                self.packager,
                self.store,
                freqs_hz=freqs,
                dwell_ms=dwell,
                interval_ms=interval,
                chunk_size=chunk_size,
            )
            st = self._monitor.start()
            return {"ok": True, "result": st}

        if name == "list_missions":
            raw = c.mission_list()
            self.store.audit("list_missions", {"ok": raw.get("ok")})
            return {"ok": bool(raw.get("ok", True)), "result": raw, "safety": {"tx": False}}

        if name == "list_skills":
            raw = c.skill_list()
            return {"ok": bool(raw.get("ok", True)), "result": raw, "safety": {"tx": False}}

        if name == "get_mission_status":
            mid = args.get("mission_id") or ""
            raw = c.mission_status(str(mid) if mid else "")
            return {"ok": bool(raw.get("ok", True)), "result": raw, "safety": {"tx": False}}

        if name == "run_passive_mission":
            mid = str(args.get("mission_id") or "lab_passive_433")
            # Hard allowlist — never run unknown missions via LLM tools
            allowed = {
                "lab_passive_433",
                "lab_spectrum_planner",
                "lab_passive_watch",
                "lab_noise_baseline",
                "medsec_lab_passive_ism",
                "fac_rf_snapshot_passive",
                "medsec_passive_watch",
            }
            if mid not in allowed:
                return {
                    "ok": False,
                    "error": "mission_not_allowlisted",
                    "mission_id": mid,
                    "allowed": sorted(allowed),
                    "safety": {"tx": False},
                }
            steps = int(args.get("steps") or 8)
            arm = c.mission_arm(mid)
            if not arm.get("ok"):
                return {"ok": False, "error": "arm_failed", "arm": arm, "safety": {"tx": False}}
            run = c.mission_run(steps=steps)
            status = c.mission_status(mid)
            # Package a narrative observation for the learning store
            st = status if isinstance(status, dict) else {}
            narrative = (
                f"Passive mission {mid}: ran={run.get('ran')} "
                f"state={(st.get('state') if st.get('state') else st)} "
                f"last_pulses={st.get('last_pulses')}."
            )
            obs = {
                "schema": "groklink.signal_observation.v1",
                "kind": "activity_summary",
                "narrative": narrative,
                "safety": {"tx": False, "path": "passive_mission"},
                "mission": {"id": mid, "arm": arm, "run": run, "status": status},
            }
            self.store.append(obs)
            self.store.audit("run_passive_mission", {"id": mid, "steps": steps})
            return {
                "ok": bool(run.get("ok", True)),
                "result": {"mission_id": mid, "arm": arm, "run": run, "status": status, "observation": obs},
                "safety": {"tx": False},
            }

        if name == "prepare_unplugged_explore":
            mid = str(args.get("mission_id") or "lab_passive_watch")
            allow = {
                "lab_passive_watch",
                "lab_passive_433",
                "lab_spectrum_planner",
                "lab_noise_baseline",
                "medsec_lab_passive_ism",
                "fac_rf_snapshot_passive",
                "medsec_passive_watch",
            }
            if mid not in allow:
                return {
                    "ok": False,
                    "error": "not_allowlisted",
                    "safety": {"tx": False},
                    "note": "Only passive ROM missions may run unplugged.",
                }
            self._ensure_edu(c)
            auto = c.agent_auto(mid, True)
            off = c.agent_offline(True)
            arm = c.mission_arm(mid)
            snap = c.agent_status()
            return {
                "ok": True,
                "result": {
                    "mission_id": mid,
                    "agent_auto": auto,
                    "agent_offline": off,
                    "mission_arm": arm,
                    "agent_status": snap,
                    "unplugged_ready": True,
                    "narrative": (
                        f"Device armed for unplugged passive explore mission={mid}. "
                        f"You may disconnect USB; on-device agent keeps passive RX ticks. "
                        f"On reconnect: ingest_unplugged_lessons or groklink-os plug-sync. "
                        f"PC LLM cannot drive hardware without a link."
                    ),
                },
                "safety": {"tx": False, "passive_only": True},
            }

        if name == "ingest_unplugged_lessons":
            from groklink_os.research.plug_sync import ingest_unplugged_lessons

            clear = bool(args.get("clear_vault"))
            # Prefer existing client if already connected
            result = ingest_unplugged_lessons(clear_vault=clear, client=c)
            return {
                "ok": result.ok,
                "result": result.to_dict(),
                "safety": {"tx": False, "local_learning_only": True},
            }

        if name == "start_offline_agent":
            mid = str(args.get("mission_id") or "lab_passive_watch")
            allowed = {
                "lab_passive_433",
                "lab_spectrum_planner",
                "lab_passive_watch",
                "lab_noise_baseline",
                "medsec_lab_passive_ism",
                "fac_rf_snapshot_passive",
                "medsec_passive_watch",
            }
            if mid not in allowed:
                return {"ok": False, "error": "mission_not_allowlisted", "safety": {"tx": False}}
            auto = c.agent_auto(mid, True)
            st = c.agent_status()
            self.store.audit("start_offline_agent", {"id": mid})
            return {
                "ok": bool(auto.get("ok", True)),
                "result": {"auto": auto, "status": st},
                "safety": {"tx": False, "path": "offline_passive"},
            }

        if name == "stop_offline_agent":
            off = c.agent_offline(False)
            try:
                c.mission_disarm("")
            except Exception:  # noqa: BLE001
                pass
            st = c.agent_status()
            return {"ok": True, "result": {"offline": off, "status": st}, "safety": {"tx": False}}

        if name == "get_agent_status":
            raw = c.agent_status()
            return {"ok": bool(raw.get("ok", True)), "result": raw, "safety": {"tx": False}}

        if name == "get_vault_tail":
            n = int(args.get("n") or 8)
            raw = c.vault_tail(n)
            return {"ok": bool(raw.get("ok", True)), "result": raw, "safety": {"tx": False}}

        return {"ok": False, "error": f"unknown_tool:{name}", "safety": {"tx": False}}

    def dispatch_openai_tool_call(self, tool_call: dict[str, Any]) -> dict[str, Any]:
        """Accept either Chat Completions tool_call object or {name, arguments}."""
        if "function" in tool_call:
            fn = tool_call["function"]
            name = fn.get("name") or ""
            raw_args = fn.get("arguments") or "{}"
            if isinstance(raw_args, str):
                try:
                    arguments = json.loads(raw_args) if raw_args.strip() else {}
                except json.JSONDecodeError:
                    arguments = {}
            else:
                arguments = dict(raw_args)
        else:
            name = tool_call.get("name") or ""
            arguments = dict(tool_call.get("arguments") or {})
        return self.dispatch(name, arguments)
