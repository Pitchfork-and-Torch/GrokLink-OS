"""MedSec / lab evidence tools (PC-side). NOT a medical device."""

from groklink_os.lab.engagement import (
    DISCLAIMER,
    load_engagement,
    save_engagement,
    stamp_audit_jsonl,
    stamp_record,
)
from groklink_os.lab.casefile import create_casefile, write_casefile_report
from groklink_os.lab.anomaly import score_observations_anomaly, write_anomaly_report
from groklink_os.lab.export import export_history_csv, export_history_json, export_research_bundle
from groklink_os.lab.siem import export_siem_ndjson
from groklink_os.lab.vault_seal import seal_directory, unseal_to_directory
from groklink_os.lab.phi import PhiHygieneError, assert_no_phi, find_phi_hits

__all__ = [
    "DISCLAIMER",
    "load_engagement",
    "save_engagement",
    "stamp_audit_jsonl",
    "stamp_record",
    "create_casefile",
    "write_casefile_report",
    "score_observations_anomaly",
    "write_anomaly_report",
    "export_history_csv",
    "export_history_json",
    "export_research_bundle",
    "export_siem_ndjson",
    "seal_directory",
    "unseal_to_directory",
    "PhiHygieneError",
    "assert_no_phi",
    "find_phi_hits",
]
