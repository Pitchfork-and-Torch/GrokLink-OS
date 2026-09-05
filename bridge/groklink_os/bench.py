"""GrokLink Bench Desktop - COM, edu-ack, status, observe. No TX.

Launch from a normal terminal (not a Grok TUI job):
  py -3 -m groklink_os.bench
Or: tools/start_bench.ps1 (returns immediately).
Not Grok Orbit. Observe-only.
"""

from __future__ import annotations

import json
from typing import Any

from groklink_os import EDU_ACK, __version__
from groklink_os.rpc.client import list_serial_ports, open_client

DFU_HINT = (
    "Human-gated DFU: do not flash from a Grok TUI job.\n"
    "  powershell -File tools\\build_dfu.ps1 -Profile OsRadio\n"
    "  powershell -File tools\\flash_os_dfu_only.ps1 -DfuPath dist\\dfu\\GrokLink-OS-v3.9.0-radio.dfu\n"
    "  Then: groklink-os list-ports && groklink-os edu-ack && groklink-os storage-status"
)


def snapshot_ports() -> list[dict[str, str]]:
    return list_serial_ports()


def bench_status(serial_port: str | None = None) -> dict[str, Any]:
    """Connect, edu-ack, status, storage, ble. Observe-only."""
    out: dict[str, Any] = {"version": __version__, "edu": EDU_ACK, "tx": False}
    c = open_client(serial_port=serial_port) if serial_port else open_client()
    try:
        out["transport"] = c.transport_name
        out["ping"] = c.ping()
        out["edu_ack"] = c.edu_ack(EDU_ACK)
        out["status"] = c.status()
        try:
            out["storage"] = c.storage_status()
        except Exception as e:  # noqa: BLE001
            out["storage"] = {"ok": False, "error": str(e)}
        try:
            out["ble"] = c.ble_status()
        except Exception as e:  # noqa: BLE001
            out["ble"] = {"ok": False, "error": str(e)}
    finally:
        c.close()
    return out


def main() -> None:
    try:
        import tkinter as tk
        from tkinter import ttk, messagebox, scrolledtext
    except ImportError:
        print("tkinter missing. CLI snapshot:")
        print(json.dumps({"ports": snapshot_ports(), "dfu": DFU_HINT}, indent=2))
        return

    root = tk.Tk()
    root.title("GrokLink Bench 3.9 Field Card")
    root.geometry("640x520")

    frm = ttk.Frame(root, padding=10)
    frm.pack(fill=tk.BOTH, expand=True)

    ttk.Label(frm, text="GrokLink Bench (observe only, not Orbit)").pack(anchor=tk.W)
    ports_var = tk.StringVar()
    port_box = ttk.Combobox(frm, textvariable=ports_var, width=50)
    port_box.pack(fill=tk.X, pady=4)

    def refresh_ports() -> None:
        ports = snapshot_ports()
        labels = [p.get("device") or p.get("name") or str(p) for p in ports]
        port_box["values"] = labels
        if labels and not ports_var.get():
            ports_var.set(labels[0])

    log = scrolledtext.ScrolledText(frm, height=18, wrap=tk.WORD)
    log.pack(fill=tk.BOTH, expand=True, pady=6)

    def write(msg: str) -> None:
        log.insert(tk.END, msg + "\n")
        log.see(tk.END)

    def run_status() -> None:
        try:
            snap = bench_status(ports_var.get() or None)  # COM device string
            write(json.dumps(snap, indent=2, default=str))
        except Exception as e:  # noqa: BLE001
            write("error: " + str(e))
            messagebox.showerror("Bench", str(e))

    def observe() -> None:
        try:
            from groklink_os.observe.tools import ToolDispatcher

            d = ToolDispatcher()
            try:
                r = d.dispatch("observe_rx", {"freq_hz": 433920000, "ms": 400})
                write(json.dumps(r, indent=2, default=str)[:4000])
            finally:
                d.close()
        except Exception as e:  # noqa: BLE001
            write("observe error: " + str(e))

    row = ttk.Frame(frm)
    row.pack(fill=tk.X)
    ttk.Button(row, text="Refresh ports", command=refresh_ports).pack(side=tk.LEFT, padx=2)
    ttk.Button(row, text="edu-ack + status", command=run_status).pack(side=tk.LEFT, padx=2)
    ttk.Button(row, text="observe-rx 433.92", command=observe).pack(side=tk.LEFT, padx=2)

    hint = ttk.Label(frm, text=DFU_HINT, justify=tk.LEFT)
    hint.pack(anchor=tk.W, pady=6)
    refresh_ports()
    write("Ready. No TX controls on this bench.")
    root.mainloop()


if __name__ == "__main__":
    main()
