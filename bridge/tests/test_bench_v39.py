from groklink_os.bench import DFU_HINT, bench_status, snapshot_ports


def test_bench_dfu_hint() -> None:
    assert "Human-gated DFU" in DFU_HINT
    assert "3.8.0" in DFU_HINT
    assert "does not recut DFU" in DFU_HINT
    ports = snapshot_ports()
    assert isinstance(ports, list)


def test_bench_status_offline_no_hang() -> None:
    snap = bench_status(serial_port=None, timeout=0.4)
    assert snap["tx"] is False
    assert "edu" in snap
    if snap.get("ok") is False:
        assert snap.get("error")
