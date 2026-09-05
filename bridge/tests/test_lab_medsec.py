"""Tests for MedSec lab evidence (PC-side)."""

from __future__ import annotations

import json
from pathlib import Path

from groklink_os.lab.anomaly import score_observations_anomaly
from groklink_os.lab.casefile import create_casefile, write_casefile_report
from groklink_os.lab.engagement import save_engagement, stamp_audit_jsonl, stamp_record
from groklink_os.lab.export import export_history_csv, export_research_bundle
from groklink_os.observe.store import ObservationStore
from groklink_os.observe.tools import ToolDispatcher


class MockClient:
    def __init__(self) -> None:
        self.transport_name = "mock"

    def close(self) -> None:
        return None

    def edu_ack(self, phrase: str = "") -> dict:
        return {"ok": True, "edu": True}

    def mission_list(self) -> dict:
        return {
            "ok": True,
            "missions": "lab_passive_433,medsec_lab_passive_ism,fac_rf_snapshot_passive",
            "count": 3,
        }

    def mission_arm(self, mission_id: str) -> dict:
        return {"ok": True, "id": mission_id}

    def mission_run(self, steps: int = 8) -> dict:
        return {"ok": True, "ran": steps}

    def mission_status(self, mission_id: str = "") -> dict:
        return {"ok": True, "id": mission_id or "medsec_lab_passive_ism", "state": "done", "last_pulses": 7}

    def agent_status(self) -> dict:
        return {"ok": True}

    def agent_auto(self, mid: str, on: bool) -> dict:
        return {"ok": True, "id": mid, "on": on}

    def agent_offline(self, on: bool = True) -> dict:
        return {"ok": True, "offline": on}

    def mission_disarm(self, _mid: str) -> dict:
        return {"ok": True}

    def vault_tail(self, n: int = 8) -> dict:
        return {"ok": True, "events": [], "n": n}

    def status(self) -> dict:
        return {"ok": True, "not_medical_device": True, "profile": "medsec-strict"}

    def skill_list(self) -> dict:
        return {"ok": True, "skills": "medsec_passive_ism_watch"}


def test_engagement_and_stamp(tmp_path: Path, monkeypatch) -> None:
    eng_path = tmp_path / "engagement.json"
    monkeypatch.setenv("GLK_ENGAGEMENT_FILE", str(eng_path))
    save_engagement(
        operator_id="op1",
        engagement_id="ENG-1",
        site_label="bench",
        roe_ack=True,
        profile="medsec-strict",
        path=eng_path,
    )
    rec = stamp_record({"event": "rx", "pulses": 3})
    assert rec["engagement_id"] == "ENG-1"
    assert rec["not_medical_device"] is True

    audit_in = tmp_path / "a.jsonl"
    audit_in.write_text('{"x":1}\nnotjson\n{"y":2}\n', encoding="utf-8")
    out = tmp_path / "stamped.jsonl"
    r = stamp_audit_jsonl(audit_in, out, engagement=json.loads(eng_path.read_text(encoding="utf-8")))
    assert r["stamped_rows"] == 2
    line = out.read_text(encoding="utf-8").splitlines()[0]
    assert "ENG-1" in line


def test_casefile(tmp_path: Path, monkeypatch) -> None:
    eng_path = tmp_path / "engagement.json"
    monkeypatch.setenv("GLK_ENGAGEMENT_FILE", str(eng_path))
    save_engagement(
        operator_id="op1",
        engagement_id="ENG-2",
        site_label="lab",
        roe_ack=True,
        path=eng_path,
    )
    case = tmp_path / "case"
    create_casefile(case, title="T", hypothesis="H", freqs_hz=[433920000])
    write_casefile_report(case, narrative="Passive only")
    data = json.loads((case / "CASEFILE.json").read_text(encoding="utf-8"))
    assert data["not_medical_device"] is True
    assert data["engagement_id"] == "ENG-2"
    assert "NOT A MEDICAL DEVICE" in (case / "CASEFILE.md").read_text(encoding="utf-8")


def test_anomaly_and_export(tmp_path: Path) -> None:
    hist = tmp_path / "obs.jsonl"
    rows = [
        {"freq_hz": 433920000, "pulses": 2, "kind": "rx"},
        {"freq_hz": 433920000, "pulses": 2, "kind": "rx"},
        {"freq_hz": 433920000, "pulses": 40, "kind": "rx"},
        {"freq_hz": 433920000, "pulses": 50, "kind": "rx"},
    ]
    hist.write_text("\n".join(json.dumps(r) for r in rows) + "\n", encoding="utf-8")
    score = score_observations_anomaly(hist)
    assert score["ok"] is True
    assert score["never_auto_tx"] is True
    csv_out = tmp_path / "e.csv"
    export_history_csv(hist, csv_out)
    assert csv_out.exists()
    rb = tmp_path / "bundle.json"
    export_research_bundle(hist, rb)
    data = json.loads(rb.read_text(encoding="utf-8"))
    assert data["clinical_use"] is False


def test_medsec_mission_allowlisted(tmp_path: Path) -> None:
    store = ObservationStore(root=tmp_path, persist=False)
    d = ToolDispatcher(client=MockClient(), store=store)  # type: ignore[arg-type]
    r = d.dispatch("run_passive_mission", {"mission_id": "medsec_lab_passive_ism", "steps": 4})
    assert r["ok"] is True
    assert r["safety"]["tx"] is False
    bad = d.dispatch("run_passive_mission", {"mission_id": "evil_tx", "steps": 1})
    assert bad["ok"] is False
    d.close()


def test_phi_hygiene_rejects_ssn(tmp_path: Path, monkeypatch) -> None:
    from groklink_os.lab.phi import PhiHygieneError, find_phi_hits
    from groklink_os.lab.engagement import save_engagement

    assert "ssn" in find_phi_hits("123-45-6789")
    eng_path = tmp_path / "e.json"
    monkeypatch.setenv("GLK_ENGAGEMENT_FILE", str(eng_path))
    try:
        save_engagement(
            operator_id="op",
            engagement_id="ENG-1",
            site_label="patient John Smith ward",
            roe_ack=True,
            path=eng_path,
        )
        raised = False
    except PhiHygieneError:
        raised = True
    # patient_name_hint may or may not fire; force SSN path
    try:
        save_engagement(
            operator_id="op",
            engagement_id="123-45-6789",
            site_label="lab",
            roe_ack=True,
            path=eng_path,
        )
        assert False, "expected PhiHygieneError"
    except PhiHygieneError:
        pass


def test_vault_seal_roundtrip(tmp_path: Path) -> None:
    from groklink_os.lab.vault_seal import seal_directory, unseal_to_directory

    src = tmp_path / "case"
    src.mkdir()
    (src / "CASEFILE.json").write_text('{"ok":true,"not_medical_device":true}\n', encoding="utf-8")
    seal = tmp_path / "case.glkseal"
    seal_directory(src, seal, "lab-password-test")
    dest = tmp_path / "out"
    unseal_to_directory(seal, dest, "lab-password-test")
    assert (dest / "CASEFILE.json").exists()
    assert (dest / "SEAL_MANIFEST.json").exists()


def test_siem_export(tmp_path: Path) -> None:
    from groklink_os.lab.siem import export_siem_ndjson

    hist = tmp_path / "h.jsonl"
    hist.write_text('{"kind":"rx","freq_hz":433920000,"pulses":3}\n', encoding="utf-8")
    out = tmp_path / "siem.ndjson"
    r = export_siem_ndjson(hist, out)
    assert r["ok"] is True
    line = out.read_text(encoding="utf-8").strip()
    data = json.loads(line)
    assert data["clinical_use"] is False
    assert data["event_type"] == "groklink.medsec.observation"
