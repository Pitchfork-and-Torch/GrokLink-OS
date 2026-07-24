#!/usr/bin/env python3
"""
1) Open Flipper CLI serial, start RPC, reboot into DFU
2) Wait for STM32 DFU 0483:DF11
3) Call qFlipper-cli to flash the CDC DFU image
"""
from __future__ import annotations

import argparse
import struct
import subprocess
import sys
import time
from pathlib import Path

try:
    import serial
    from serial.tools import list_ports
except ImportError:
    print("pip install pyserial", file=sys.stderr)
    raise SystemExit(1)


def encode_varint(n: int) -> bytes:
    out = bytearray()
    while True:
        b = n & 0x7F
        n >>= 7
        out.append(b | (0x80 if n else 0))
        if not n:
            break
    return bytes(out)


def pb_main_reboot_dfu(command_id: int = 1) -> bytes:
    # RebootRequest { mode = DFU = 1 }  => field1 varint 1
    reboot = b"\x08\x01"
    # Main { command_id, system_reboot_request=field 31 }
    body = b"\x08" + encode_varint(command_id)
    # field 31, wire type 2 (len-delimited): tag = (31<<3)|2 = 250
    body += encode_varint((31 << 3) | 2) + encode_varint(len(reboot)) + reboot
    return encode_varint(len(body)) + body


def find_flipper_port() -> str | None:
    for p in list_ports.comports():
        desc = f"{p.description} {p.hwid} {p.manufacturer} {p.product}".upper()
        if "FLIP" in desc or "0483" in desc or "FLIPPER" in desc:
            return p.device
        if "VID:PID=0483:5740" in p.hwid.upper().replace(" ", ""):
            return p.device
    # fallback common
    for p in list_ports.comports():
        if p.device.upper().startswith("COM") and p.vid == 0x0483:
            return p.device
    return None


def reboot_to_dfu(port: str) -> None:
    print(f"Opening {port}…")
    ser = serial.Serial(port, 230400, timeout=1.5, write_timeout=1.5)
    time.sleep(0.4)
    ser.reset_input_buffer()
    ser.write(b"\r\n")
    time.sleep(0.2)
    probe = ser.read(ser.in_waiting or 200)
    print(f"  probe: {probe[:100]!r}")

    # Path A: GrokLink OS CDC JSON reboot (3.0.1+ with reboot_dfu)
    print("Trying JSON reboot_dfu…")
    ser.write(b'{"cmd":"reboot_dfu"}\n')
    time.sleep(0.5)
    jr = ser.read(ser.in_waiting or 200)
    print(f"  json resp: {jr[:100]!r}")
    if b"reboot_dfu" in jr and b'"ok":true' in jr.replace(b" ", b""):
        try:
            ser.close()
        except Exception:
            pass
        print("JSON reboot_dfu accepted.")
        time.sleep(1.0)
        return

    # Path B: Flipper Momentum CLI + protobuf reboot DFU
    print("start_rpc_session (Flipper path)…")
    ser.write(b"start_rpc_session\r")
    time.sleep(0.5)
    junk = ser.read(ser.in_waiting or 64)
    print(f"  after session: {junk[:80]!r}")
    ver = b"\x08\x01" + encode_varint((39 << 3) | 2) + encode_varint(0)
    ser.write(encode_varint(len(ver)) + ver)
    time.sleep(0.3)
    print(f"  version resp: {ser.read(ser.in_waiting or 128)[:80]!r}")
    msg = pb_main_reboot_dfu(2)
    print(f"Sending protobuf reboot DFU ({msg.hex()})")
    ser.write(msg)
    time.sleep(0.2)
    try:
        ser.close()
    except Exception:
        pass
    print("Closed serial (device should reboot to DFU).")


def wait_dfu(timeout: float = 30.0) -> bool:
    """Poll Windows PnP for STM32 DFU (no libusb backend required)."""
    deadline = time.time() + timeout
    while time.time() < deadline:
        try:
            r = subprocess.run(
                [
                    "powershell",
                    "-NoProfile",
                    "-Command",
                    "Get-PnpDevice -PresentOnly | "
                    "Where-Object { $_.InstanceId -match 'VID_0483&PID_DF11' } | "
                    "Select-Object -ExpandProperty Status",
                ],
                capture_output=True,
                text=True,
                timeout=5,
            )
            if "OK" in (r.stdout or ""):
                print("Found STM32 DFU device (PnP)")
                return True
        except Exception as e:
            print(f"PnP poll: {e}")
        # also: serial ports with 5740 gone is a weak signal
        has_vcp = any(
            (p.vid == 0x0483 and p.pid == 0x5740) for p in list_ports.comports() if p.vid
        )
        if not has_vcp:
            print("Flipper VCP gone — likely DFU or disconnect")
        time.sleep(0.6)
    return False


def qflipper_flash(dfu_path: Path) -> int:
    cli = Path(r"C:\Program Files\qFlipper\qFlipper-cli.exe")
    if not cli.exists():
        print("qFlipper-cli not found", file=sys.stderr)
        return 2
    print(f"qFlipper-cli firmware {dfu_path}")
    r = subprocess.run(
        [str(cli), "-d", "2", "firmware", str(dfu_path)],
        capture_output=False,
    )
    return r.returncode


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument(
        "--dfu",
        type=Path,
        default=Path(__file__).resolve().parents[1]
        / "dist"
        / "dfu"
        / "GrokLink-OS-v3.2.0-radio.dfu",
    )
    ap.add_argument("--port", default="")
    args = ap.parse_args()
    if not args.dfu.exists():
        print(f"missing {args.dfu}", file=sys.stderr)
        return 1

    port = args.port or find_flipper_port()
    if not port:
        print("No Flipper serial port; if already in DFU, flashing anyway…")
    else:
        try:
            reboot_to_dfu(port)
        except Exception as e:
            print(f"RPC reboot failed: {e}", file=sys.stderr)
            print("Hold BACK, plug USB for DFU, waiting…")

    if not wait_dfu(35.0):
        # Windows may not expose DFU to pyusb without WinUSB; still try qFlipper
        print("DFU not confirmed via pyusb; invoking qFlipper-cli (needs DFU or serial).")
    return qflipper_flash(args.dfu)


if __name__ == "__main__":
    raise SystemExit(main())
