from groklink_os.bench import DFU_HINT, snapshot_ports


def test_bench_dfu_hint() -> None:
    assert "Human-gated DFU" in DFU_HINT
    assert "3.9.0" in DFU_HINT
    ports = snapshot_ports()
    assert isinstance(ports, list)
