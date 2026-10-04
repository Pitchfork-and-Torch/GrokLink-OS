#!/usr/bin/env python3
"""
Standalone DfuSe packer (Flipper / ST compatible).
Compatible with qFlipper "Install from file" for a single flash target.

Usage:
  py -3 tools/bin2dfu.py -i firmware.bin -o firmware.dfu -a 0x08000000 -l "GrokLink OS"
"""
from __future__ import annotations

import argparse
import struct
import sys
from pathlib import Path
from zlib import crc32


def pack_dfu(
    bindata: bytes,
    address: int,
    label: str,
    vid: int = 0x0483,
    pid: int = 0xDF11,
) -> bytes:
    # Element: address + size + payload
    element = struct.pack("<II", address, len(bindata)) + bindata

    # Target prefix: "Target", alt, named, name[255], target_size, num_elements
    name = label.encode("ascii")[:254]
    target_body = element
    target = (
        struct.pack(
            "<6sBI255sII",
            b"Target",
            0,  # alt setting
            1,  # named
            name,
            len(target_body),
            1,  # one element
        )
        + target_body
    )

    # DfuSe prefix
    prefix = struct.pack("<5sBIB", b"DfuSe", 0x01, len(target) + 11, 1)
    data = prefix + target

    # Suffix: bcdDevice, pid, vid, bcdDFU, 'UFD', length
    data += struct.pack("<HHHH3sB", 0xFFFF, pid, vid, 0x011A, b"UFD", 16)
    dw_crc = (~crc32(data)) & 0xFFFFFFFF
    data += struct.pack("<I", dw_crc)
    return data


def parse_dfu(path: Path) -> None:
    data = path.read_bytes()
    if data[:5] != b"DfuSe":
        raise SystemExit(f"Not a DfuSe file: {path}")
    ver = data[5]
    size, ntargets = struct.unpack_from("<IB", data, 6)
    print(f"DfuSe v{ver} declared_size={size} targets={ntargets} file_len={len(data)}")
    off = 11
    for t in range(ntargets):
        # Match Flipper bin2dfu: <6sBI255sII
        sig, alt, named, name_raw, tsize, nelem = struct.unpack_from("<6sBI255sII", data, off)
        off += 6 + 1 + 4 + 255 + 4 + 4
        name = name_raw.split(b"\x00", 1)[0].decode("ascii", "replace")
        print(
            f"  target[{t}] sig={sig!r} alt={alt} named={named} "
            f"name={name!r} tsize={tsize} elements={nelem}"
        )
        for e in range(nelem):
            addr, esize = struct.unpack_from("<II", data, off)
            off += 8
            print(f"    elem[{e}] address=0x{addr:08X} size={esize}")
            if off + esize > len(data):
                raise SystemExit(f"corrupt DFU: element size {esize} overruns file")
            off += esize
    print(f"  suffix_bytes={len(data) - off} (expect 16+4 crc)")


def main() -> int:
    ap = argparse.ArgumentParser(description="Pack/inspect ST DfuSe DFU images")
    ap.add_argument("-i", "--input", help="Input .bin")
    ap.add_argument("-o", "--output", help="Output .dfu")
    ap.add_argument(
        "-a",
        "--address",
        type=lambda x: int(x, 0),
        default=0x08000000,
        help="Flash load address (default 0x08000000)",
    )
    ap.add_argument("-l", "--label", default="GrokLink OS", help="DFU target label")
    ap.add_argument("--vid", type=lambda x: int(x, 0), default=0x0483)
    ap.add_argument("--pid", type=lambda x: int(x, 0), default=0xDF11)
    ap.add_argument("--inspect", help="Inspect an existing .dfu and exit")
    args = ap.parse_args()

    if args.inspect:
        parse_dfu(Path(args.inspect))
        return 0

    if not args.input or not args.output:
        ap.error("--input and --output required (or use --inspect)")

    bindata = Path(args.input).read_bytes()
    if len(bindata) < 8:
        print("ERROR: binary too small", file=sys.stderr)
        return 1
    # crude vector-table sanity: SP in RAM, Reset in flash
    sp, reset = struct.unpack_from("<II", bindata, 0)
    print(f"vector SP=0x{sp:08X} Reset=0x{reset:08X} size={len(bindata)}")
    if not (0x20000000 <= sp <= 0x20040000):
        print("WARNING: initial SP not in typical WB55 SRAM range", file=sys.stderr)
    if not (0x08000000 <= (reset & ~1) < 0x08100000):
        print("WARNING: Reset handler not in typical flash range", file=sys.stderr)

    out = pack_dfu(bindata, args.address, args.label, args.vid, args.pid)
    Path(args.output).write_bytes(out)
    print(f"Wrote {args.output} ({len(out)} bytes) label={args.label!r} @ 0x{args.address:08X}")
    parse_dfu(Path(args.output))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
