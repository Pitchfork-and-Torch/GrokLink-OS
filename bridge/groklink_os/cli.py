"""GrokLink OS CLI — RPC + multi-LLM signal observability."""

from __future__ import annotations

import json
import sys
from pathlib import Path
from typing import Optional

import typer
from rich import print as rprint

from groklink_os import EDU_ACK, __version__
from groklink_os.observe.agent_loop import SYSTEM_PROMPT, run_scripted_observation_session
from groklink_os.observe.openai_server import ObserveAPIServer
from groklink_os.observe.store import ObservationStore
from groklink_os.observe.tools import ToolDispatcher, tools_openai_format
from groklink_os.rpc.client import GrokLinkClient, list_serial_ports, open_client
from groklink_os.skills.craft import craft_skill_from_capture

app = typer.Typer(
    help="GrokLink OS PC bridge — authorized research only. Passive observation tools for LLMs.",
    no_args_is_help=True,
)


@app.command()
def version() -> None:
    """Print bridge version."""
    rprint(f"groklink-os bridge {__version__}")


def _client() -> GrokLinkClient:
    """TCP host-sim or USB CDC (GLK_SERIAL_PORT / COM host)."""
    return open_client()


@app.command()
def ping() -> None:
    c = _client()
    try:
        rprint({"transport": c.transport_name, **c.ping()})
    finally:
        c.close()


@app.command("edu-ack")
def edu_ack() -> None:
    c = _client()
    try:
        rprint(c.edu_ack(EDU_ACK))
    finally:
        c.close()


@app.command()
def status() -> None:
    c = _client()
    try:
        rprint(c.status())
    finally:
        c.close()


@app.command()
def rx(
    freq: int = typer.Option(433_920_000, help="Frequency Hz"),
    ms: int = typer.Option(500, help="Dwell ms"),
) -> None:
    c = _client()
    try:
        c.edu_ack(EDU_ACK)
        rprint(c.subghz_rx(freq, ms))
    finally:
        c.close()


@app.command()
def spectrum(
    freqs: str = typer.Option("433920000,315000000", help="Comma-separated Hz"),
    ms: int = 400,
    settle: int = 2000,
) -> None:
    fl = [int(x.strip()) for x in freqs.split(",") if x.strip()]
    c = _client()
    try:
        c.edu_ack(EDU_ACK)
        rprint(c.spectrum(fl, ms=ms, settle_ms=settle))
    finally:
        c.close()


@app.command("confirm-issue")
def confirm_issue(
    action: str = "subghz_tx",
    freq: int = 0,
) -> None:
    c = _client()
    try:
        c.edu_ack(EDU_ACK)
        rprint(c.confirm_issue(action, freq_hz=freq))
    finally:
        c.close()


@app.command()
def missions() -> None:
    c = _client()
    try:
        rprint(c.mission_list())
    finally:
        c.close()


@app.command("mission-arm")
def mission_arm(mission_id: str = typer.Argument("lab_passive_433")) -> None:
    c = _client()
    try:
        c.edu_ack(EDU_ACK)
        rprint(c.mission_arm(mission_id))
    finally:
        c.close()


@app.command("mission-run")
def mission_run(
    mission_id: str = typer.Option("lab_passive_433", "--id"),
    steps: int = typer.Option(8, help="Max IR steps"),
) -> None:
    """Arm and run a passive lab mission (no TX)."""
    d = ToolDispatcher()
    try:
        rprint(d.dispatch("run_passive_mission", {"mission_id": mission_id, "steps": steps}))
    finally:
        d.close()


@app.command("mission-status")
def mission_status(mission_id: str = typer.Argument("")) -> None:
    c = _client()
    try:
        rprint(c.mission_status(mission_id))
    finally:
        c.close()


@app.command("agent-status")
def agent_status() -> None:
    c = _client()
    try:
        rprint(c.agent_status())
    finally:
        c.close()


@app.command("agent-offline")
def agent_offline_cmd(
    mission_id: str = typer.Option("lab_passive_watch", "--id"),
    stop: bool = typer.Option(False, "--stop", help="Disable offline agent"),
) -> None:
    """Enable USB-safe autonomous passive mission loop on device."""
    d = ToolDispatcher()
    try:
        if stop:
            rprint(d.dispatch("stop_offline_agent"))
        else:
            rprint(d.dispatch("start_offline_agent", {"mission_id": mission_id}))
    finally:
        d.close()


@app.command("prepare-unplugged")
def prepare_unplugged_cmd(
    mission_id: str = typer.Option("lab_passive_watch", "--id"),
) -> None:
    """Arm passive field explorer, then you may unplug USB."""
    d = ToolDispatcher()
    try:
        rprint(d.dispatch("prepare_unplugged_explore", {"mission_id": mission_id}))
        rprint(
            "[yellow]USB may now be unplugged. On-device agent continues passive RX. "
            "PC/LLM cannot command without a link. On reconnect run: groklink-os plug-sync[/yellow]"
        )
    finally:
        d.close()


@app.command("plug-sync")
def plug_sync_cmd(
    clear_vault: bool = typer.Option(
        False,
        "--clear-vault",
        help="Clear device RAM vault after successful ingest (frees ring buffer).",
    ),
    wait: bool = typer.Option(
        False,
        "--wait",
        help="Wait until device CDC appears, then ingest (for plug-in automation).",
    ),
    timeout: int = typer.Option(0, help="Seconds to wait with --wait (0=forever)"),
) -> None:
    """Ingest unplugged vault/agent lessons into local PC research store."""
    from groklink_os.research.plug_sync import ingest_unplugged_lessons, wait_for_device_and_ingest

    if wait:
        r = wait_for_device_and_ingest(timeout_s=float(timeout), clear_vault=clear_vault)
    else:
        r = ingest_unplugged_lessons(clear_vault=clear_vault)
    rprint(r.to_dict())
    if r.ok:
        rprint(f"[green]Research journal:[/green] {r.note_path}")
        rprint(f"[green]Field trip:[/green] {r.path}")
    else:
        rprint(f"[red]plug-sync failed:[/red] {r.error}")
        raise typer.Exit(code=1)


@app.command("vault-tail")
def vault_tail(n: int = typer.Option(8, help="Max events")) -> None:
    c = _client()
    try:
        rprint(c.vault_tail(n))
    finally:
        c.close()


@app.command()
def skills() -> None:
    c = _client()
    try:
        rprint(c.skill_list())
    finally:
        c.close()


@app.command("list-ports")
def list_ports() -> None:
    """List USB serial ports (requires pyserial)."""
    ports = list_serial_ports()
    if not ports:
        rprint("[yellow]No ports found (install pyserial: pip install 'groklink-os[serial]').[/yellow]")
    rprint(ports)


@app.command("event-taxonomy")
def event_taxonomy() -> None:
    """Print non-decode event taxonomy (v3.5 Signal Cognition)."""
    d = ToolDispatcher()
    try:
        rprint(d.dispatch("get_event_taxonomy"))
    finally:
        d.close()


@app.command("calibration-state")
def calibration_state() -> None:
    """Show host noise-floor / pulse-rate baselines."""
    d = ToolDispatcher()
    try:
        rprint(d.dispatch("get_calibration_state"))
    finally:
        d.close()


@app.command("observe-noise-floor")
def observe_noise_floor_cmd(
    freq: int = typer.Option(433_920_000, help="Lab frequency Hz"),
    ms: int = typer.Option(200, help="Dwell ms"),
) -> None:
    """Passive sample that updates host band calibration baseline."""
    d = ToolDispatcher()
    try:
        rprint(d.dispatch("observe_noise_floor", {"freq_hz": freq, "ms": ms}))
    finally:
        d.close()


@app.command("observe-compare")
def observe_compare_cmd(
    freq_a: int = typer.Option(433_920_000, "--freq-a"),
    freq_b: int = typer.Option(315_000_000, "--freq-b"),
    ms: int = typer.Option(250, help="Dwell ms per band"),
) -> None:
    """Passive two-band compare (energy + calibrated occupancy)."""
    d = ToolDispatcher()
    try:
        rprint(
            d.dispatch(
                "observe_compare",
                {"freq_a_hz": freq_a, "freq_b_hz": freq_b, "ms": ms},
            )
        )
    finally:
        d.close()


@app.command("craft")
def craft(
    capture: str = typer.Argument(..., help="Path to capture jsonl"),
    out_dir: str = typer.Option("skills/crafted", help="Output skill directory"),
) -> None:
    path = craft_skill_from_capture(capture, out_dir)
    rprint(f"[green]Crafted skill draft at[/green] {path}")
    rprint("[yellow]Human approve before deploy. Never auto-TX.[/yellow]")


@app.command("lab-beacon-encode")
def lab_beacon_encode_cmd(
    lab_id: int = typer.Option(1, help="Lab id 0-65535"),
    counter: int = typer.Option(1, help="Educational plain counter"),
    message: str = typer.Option("LAB", help="Short UTF-8 lab message"),
    out: Optional[Path] = typer.Option(None, help="Write JSON artifact path"),
) -> None:
    """Encode owned-lab GLK1 beacon (not a rolling code)."""
    d = ToolDispatcher()
    try:
        r = d.dispatch(
            "lab_beacon_encode",
            {"lab_id": lab_id, "counter": counter, "message": message},
        )
        rprint(r)
        if out and r.get("ok"):
            out.write_text(json.dumps(r["result"], indent=2), encoding="utf-8")
            rprint(f"[green]Wrote[/green] {out}")
    finally:
        d.close()


@app.command("lab-beacon-decode")
def lab_beacon_decode_cmd(
    hex_str: str = typer.Option(..., "--hex", help="Hex frame (GLK1 only)"),
) -> None:
    """Decode ONLY GrokLink lab beacons; rejects third-party payloads."""
    d = ToolDispatcher()
    try:
        rprint(d.dispatch("lab_beacon_decode", {"hex": hex_str}))
    finally:
        d.close()


@app.command("lab-replay-demo")
def lab_replay_demo_cmd(
    counter: int = typer.Option(7),
    counter_b: Optional[int] = typer.Option(None, help="Second counter (default: same)"),
    message: str = typer.Option("LAB"),
) -> None:
    """Show educational replay of identical GLK1 frames."""
    d = ToolDispatcher()
    try:
        args: dict = {"counter": counter, "message": message}
        if counter_b is not None:
            args["counter_b"] = counter_b
        rprint(d.dispatch("lab_replay_demo", args))
    finally:
        d.close()


@app.command("explain-rolling-codes")
def explain_rolling_codes_cmd() -> None:
    """Security education: rolling codes overview (no attack tooling)."""
    d = ToolDispatcher()
    try:
        rprint(d.dispatch("explain_rolling_codes"))
    finally:
        d.close()


@app.command("lab-beacon-tx-plan")
def lab_beacon_tx_plan_cmd(
    lab_id: int = typer.Option(1),
    counter: int = typer.Option(1),
    message: str = typer.Option("LAB"),
    freq: int = typer.Option(433_920_000),
) -> None:
    """Print human-gated TX plan for an owned-lab beacon (does not TX)."""
    d = ToolDispatcher()
    try:
        enc = d.dispatch(
            "lab_beacon_encode",
            {"lab_id": lab_id, "counter": counter, "message": message},
        )
    finally:
        d.close()
    plan = {
        "ok": True,
        "will_tx": False,
        "freq_hz": freq,
        "steps": [
            "1. Operator confirms owned-lab frequency and legal authority",
            "2. groklink-os edu-ack",
            "3. groklink-os confirm-issue --action subghz_tx --freq <Hz>",
            "4. Human copies confirm token from device/policy",
            "5. Only then subghz_tx (carrier/path as supported by firmware build)",
            "6. Decode received GLK1 hex with lab-beacon-decode",
        ],
        "encoded": enc.get("result") if enc.get("ok") else enc,
        "safety": {
            "auto_tx": False,
            "human_confirm_required": True,
            "third_party_remotes": False,
            "rolling_code_prediction": False,
            "note": "This command never transmits. Pattern OOK TX depends on firmware; host encode/decode works offline.",
        },
        "narrative": (
            "TX plan prepared for owned-lab GLK1 beacon. Human confirm required. "
            "No automatic TX, no third-party clone, no rolling-code prediction."
        ),
    }
    rprint(plan)


@app.command("tools-schema")
def tools_schema(
    out: Optional[Path] = typer.Option(None, help="Write JSON to path"),
) -> None:
    """Print OpenAI-compatible tool definitions for signal observation."""
    data = tools_openai_format()
    text = json.dumps(data, indent=2)
    if out:
        out.write_text(text, encoding="utf-8")
        rprint(f"Wrote {out}")
    else:
        sys.stdout.write(text + "\n")


@app.command("observe-rx")
def observe_rx(
    freq: int = typer.Option(433_920_000, help="Frequency Hz"),
    ms: int = typer.Option(400, help="Dwell ms"),
) -> None:
    """Passive RX packaged as a self-describing observation (LLM-ready)."""
    d = ToolDispatcher()
    try:
        rprint(d.dispatch("observe_rx", {"freq_hz": freq, "ms": ms}))
    finally:
        d.close()


@app.command("observe-spectrum")
def observe_spectrum(
    freqs: str = typer.Option("315000000,433920000,868350000", help="Comma-separated Hz"),
    ms: int = 400,
    settle: int = 2000,
) -> None:
    fl = [int(x.strip()) for x in freqs.split(",") if x.strip()]
    d = ToolDispatcher()
    try:
        rprint(d.dispatch("observe_spectrum", {"freqs_hz": fl, "ms": ms, "settle_ms": settle}))
    finally:
        d.close()


@app.command("observe-status")
def observe_status() -> None:
    d = ToolDispatcher()
    try:
        rprint(d.dispatch("get_device_status", {"probe_radio": True}))
    finally:
        d.close()


@app.command("observe-recent")
def observe_recent(limit: int = 10) -> None:
    store = ObservationStore()
    rprint({"summary": store.summarize_recent(limit), "recent": store.recent(limit)})


@app.command("tool-call")
def tool_call(
    name: str = typer.Argument(..., help="Tool name, e.g. observe_rx"),
    args_json: str = typer.Option("{}", "--args", help="JSON object of arguments"),
) -> None:
    """Dispatch one OpenAI-style tool by name (for agents / scripts)."""
    try:
        arguments = json.loads(args_json) if args_json else {}
    except json.JSONDecodeError as exc:
        rprint(f"[red]Invalid --args JSON:[/red] {exc}")
        raise typer.Exit(2) from exc
    d = ToolDispatcher()
    try:
        rprint(d.dispatch(name, arguments))
    finally:
        d.close()


@app.command("observe-serve")
def observe_serve(
    host: str = typer.Option("127.0.0.1", help="Bind host (default loopback)"),
    port: int = typer.Option(8741, help="HTTP port"),
) -> None:
    """Start local OpenAI-tools-compatible observe API (passive only)."""
    d = ToolDispatcher()
    server = ObserveAPIServer(d, host=host, port=port)
    url = server.start(background=True)
    rprint(f"[green]Listening[/green] {url}")
    rprint("GET /v1/tools  POST /v1/tools/call  POST /v1/session/run  GET /v1/observe/stream")
    rprint("Ctrl+C to stop.")
    try:
        threading_wait()
    except KeyboardInterrupt:
        pass
    finally:
        server.stop()
        d.close()


@app.command("observe-session")
def observe_session(
    freqs: str = typer.Option("433920000", help="Comma-separated Hz"),
    dwell_ms: int = typer.Option(200, help="RX dwell ms"),
    spectrum: bool = typer.Option(True, help="Include spectrum scan"),
    monitor_chunks: int = typer.Option(0, help="If >0, collect N monitor chunks"),
) -> None:
    """Run a scripted multi-step passive observation session (LLM-ready narratives)."""
    fl = [int(x.strip()) for x in freqs.split(",") if x.strip()]
    d = ToolDispatcher()
    try:
        rprint(
            run_scripted_observation_session(
                d,
                freqs_hz=fl,
                dwell_ms=dwell_ms,
                spectrum=spectrum,
                monitor_chunks=monitor_chunks,
            )
        )
    finally:
        d.close()


def threading_wait() -> None:
    import time

    while True:
        time.sleep(3600)


@app.command("grok-prompt")
def grok_prompt() -> None:
    """Print system rules for terminal Grok / multi-LLM signal observation."""
    sys.stdout.write(SYSTEM_PROMPT + "\n")


# --- MedSec / lab evidence (PC-side; NOT a medical device) ---

lab_app = typer.Typer(
    help="MedSec/lab evidence: engagement, casefile, anomaly, export. NOT a medical device.",
    no_args_is_help=True,
)
app.add_typer(lab_app, name="lab")


@lab_app.command("engagement-init")
def lab_engagement_init(
    operator: str = typer.Option(..., "--operator", help="Lab operator label (no PHI)"),
    engagement: str = typer.Option(..., "--engagement", help="Engagement id e.g. ENG-2026-001"),
    site: str = typer.Option("lab", "--site", help="Site label (no PHI)"),
    roe_ack: bool = typer.Option(False, "--roe-ack", help="Confirm written RoE acknowledged"),
    profile: str = typer.Option("medsec-strict", "--profile", help="default|medsec-strict"),
) -> None:
    """Initialize PC engagement context for evidence stamping."""
    from groklink_os.lab.engagement import DISCLAIMER, save_engagement

    if not roe_ack:
        rprint("[red]Refusing: pass --roe-ack after written RoE is acknowledged.[/red]")
        raise typer.Exit(2)
    path = save_engagement(
        operator_id=operator,
        engagement_id=engagement,
        site_label=site,
        roe_ack=True,
        profile=profile,
    )
    rprint(
        {
            "ok": True,
            "path": str(path),
            "operator_id": operator,
            "engagement_id": engagement,
            "site_label": site,
            "profile": profile,
            "not_medical_device": True,
            "disclaimer": DISCLAIMER,
        }
    )


@lab_app.command("engagement-show")
def lab_engagement_show() -> None:
    from groklink_os.lab.engagement import load_engagement

    rprint(load_engagement())


@lab_app.command("stamp-audit")
def lab_stamp_audit(
    inp: Path = typer.Option(..., "--input", help="Audit JSONL path"),
    out: Path = typer.Option(..., "--out", help="Stamped output JSONL"),
) -> None:
    from groklink_os.lab.engagement import stamp_audit_jsonl

    rprint(stamp_audit_jsonl(inp, out))


@lab_app.command("casefile")
def lab_casefile(
    case_dir: Path = typer.Option(..., "--dir", help="Case directory"),
    title: str = typer.Option(..., "--title"),
    hypothesis: str = typer.Option(..., "--hypothesis"),
    notes: str = typer.Option("", "--notes"),
    freqs: str = typer.Option("433920000,315000000", "--freqs", help="Comma Hz"),
    capture: Optional[Path] = typer.Option(None, "--capture", help="Optional capture file"),
    narrative: str = typer.Option("", "--narrative", help="Markdown narrative body"),
) -> None:
    """Create CASEFILE.json + CASEFILE.md (authorized lab evidence)."""
    from groklink_os.lab.casefile import create_casefile, write_casefile_report

    fl = [int(x.strip()) for x in freqs.split(",") if x.strip()]
    path = create_casefile(
        case_dir,
        title=title,
        hypothesis=hypothesis,
        freqs_hz=fl,
        notes=notes,
        capture_path=capture,
    )
    md = write_casefile_report(case_dir, narrative=narrative)
    rprint({"ok": True, "manifest": str(path), "report": str(md), "not_medical_device": True})


@lab_app.command("anomaly")
def lab_anomaly(
    history: Path = typer.Option(..., "--history", help="Observation JSONL"),
    out: Path = typer.Option(..., "--out", help="Anomaly report JSON"),
) -> None:
    """Heuristic anomaly score vs baseline. Never auto-TX. Not clinical."""
    from groklink_os.lab.anomaly import write_anomaly_report

    path = write_anomaly_report(history, out)
    rprint({"ok": True, "out": str(path), "never_auto_tx": True, "not_medical_device": True})


@lab_app.command("export-csv")
def lab_export_csv(
    history: Path = typer.Option(..., "--history"),
    out: Path = typer.Option(..., "--out"),
) -> None:
    from groklink_os.lab.export import export_history_csv

    rprint(export_history_csv(history, out))


@lab_app.command("export-json")
def lab_export_json(
    history: Path = typer.Option(..., "--history"),
    out: Path = typer.Option(..., "--out"),
) -> None:
    from groklink_os.lab.export import export_history_json

    rprint(export_history_json(history, out))


@lab_app.command("export-research")
def lab_export_research(
    history: Path = typer.Option(..., "--history"),
    out: Path = typer.Option(..., "--out"),
    title: str = typer.Option("GrokLink research observation bundle", "--title"),
) -> None:
    """Experimental research bundle — NOT FHIR clinical / NOT EHR."""
    from groklink_os.lab.export import export_research_bundle

    rprint(export_research_bundle(history, out, title=title))


@lab_app.command("medsec-demo")
def lab_medsec_demo(
    steps: int = typer.Option(8, help="Mission IR steps"),
    mission_id: str = typer.Option("medsec_lab_passive_ism", "--id"),
) -> None:
    """15-minute path: edu-ack, passive MedSec mission, vault-tail, banner."""
    rprint(
        "[bold cyan]GrokLink MedSec demo[/bold cyan] — NOT a medical device. "
        "Authorized research only."
    )
    d = ToolDispatcher()
    try:
        st = d.dispatch("get_device_status", {"probe_radio": False})
        rprint({"status": st})
        r = d.dispatch("run_passive_mission", {"mission_id": mission_id, "steps": steps})
        rprint(r)
        vt = d.dispatch("get_vault_tail", {"n": 8})
        rprint(vt)
    finally:
        d.close()
    rprint(
        "[green]Next:[/green] groklink-os lab engagement-init --operator … --engagement … --roe-ack; "
        "groklink-os lab casefile --dir cases/… ; plug-sync if unplugged."
    )


@lab_app.command("export-siem")
def lab_export_siem(
    history: Path = typer.Option(..., "--history"),
    out: Path = typer.Option(..., "--out", help="NDJSON path for SIEM ingest"),
    limit: int = typer.Option(10000, help="Max events"),
) -> None:
    """SIEM-friendly NDJSON (research severity only; clinical_use=false)."""
    from groklink_os.lab.siem import export_siem_ndjson

    rprint(export_siem_ndjson(history, out, limit=limit))


@lab_app.command("vault-seal")
def lab_vault_seal(
    src: Path = typer.Option(..., "--dir", help="Case directory to seal"),
    out: Path = typer.Option(..., "--out", help="Output .glkseal path"),
    password: str = typer.Option(..., "--password", help="Seal password (lab only)"),
) -> None:
    """Password-seal a case directory for private vault storage (PC-side)."""
    from groklink_os.lab.vault_seal import seal_directory

    path = seal_directory(src, out, password)
    rprint({"ok": True, "out": str(path), "not_medical_device": True, "note": "Lab confidentiality aid"})


@lab_app.command("vault-unseal")
def lab_vault_unseal(
    seal: Path = typer.Option(..., "--seal", help=".glkseal path"),
    dest: Path = typer.Option(..., "--dest", help="Extract directory"),
    password: str = typer.Option(..., "--password"),
) -> None:
    from groklink_os.lab.vault_seal import unseal_to_directory

    path = unseal_to_directory(seal, dest, password)
    rprint({"ok": True, "dest": str(path)})


@lab_app.command("phi-check")
def lab_phi_check(
    text: str = typer.Argument(..., help="String to scan for PHI-like patterns"),
) -> None:
    """Scan a label/note for accidental PHI-like patterns."""
    from groklink_os.lab.phi import find_phi_hits

    hits = find_phi_hits(text)
    rprint({"ok": len(hits) == 0, "hits": hits, "not_medical_device": True})


if __name__ == "__main__":
    app()
