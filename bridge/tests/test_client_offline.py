"""Offline craft tests (no device required)."""

from pathlib import Path

from groklink_os.skills.craft import craft_skill_from_capture


def test_craft(tmp_path: Path) -> None:
    cap = tmp_path / "c.jsonl"
    cap.write_text('{"pulses":12}\n{"pulses":30}\n', encoding="utf-8")
    out = craft_skill_from_capture(str(cap), str(tmp_path / "skills"))
    assert Path(out, "manifest.json").exists()
