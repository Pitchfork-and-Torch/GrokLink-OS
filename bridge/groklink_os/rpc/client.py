"""JSON-line RPC client for GrokLink OS — TCP host-sim or USB CDC serial."""

from __future__ import annotations

import json
import os
import socket
import time
from typing import Any, Optional, Protocol


class _Transport(Protocol):
    def send_line(self, line: str) -> None: ...
    def recv_line(self, timeout: float) -> str: ...
    def close(self) -> None: ...


class _TcpTransport:
    def __init__(self, host: str, port: int, timeout: float) -> None:
        self._timeout = timeout
        self._sock = socket.create_connection((host, port), timeout=timeout)
        self._sock.settimeout(timeout)
        self._buf = b""

    def send_line(self, line: str) -> None:
        data = line if line.endswith("\n") else line + "\n"
        self._sock.sendall(data.encode("utf-8"))

    def recv_line(self, timeout: float) -> str:
        self._sock.settimeout(timeout)
        while b"\n" not in self._buf:
            chunk = self._sock.recv(4096)
            if not chunk:
                raise ConnectionError("RPC connection closed")
            self._buf += chunk
        raw, self._buf = self._buf.split(b"\n", 1)
        return raw.decode("utf-8", errors="replace").strip()

    def close(self) -> None:
        try:
            self._sock.close()
        except OSError:
            pass


class _SerialTransport:
    """USB CDC serial (device). pyserial preferred; Win32 CreateFile fallback on Windows."""

    def __init__(self, port: str, baud: int, timeout: float) -> None:
        self._mode = "pyserial"
        self._ser = None
        self._h = None
        self._timeout = timeout
        self._buf = ""
        # On Windows, GrokLink CDC often rejects pyserial SetCommState; use Win32 first.
        if os.name == "nt":
            try:
                self._open_win32(port, baud, timeout)
            except Exception:
                self._open_pyserial(port, baud, timeout)
        else:
            self._open_pyserial(port, baud, timeout)
        # Drain boot banners
        time.sleep(0.15)
        self._drain()

    def _open_pyserial(self, port: str, baud: int, timeout: float) -> None:
        try:
            import serial  # type: ignore
        except ImportError as exc:
            raise ImportError(
                "USB CDC transport requires pyserial. Install: pip install pyserial "
                "or pip install 'groklink-os[serial]'"
            ) from exc
        self._mode = "pyserial"
        self._ser = serial.Serial(
            port=port,
            baudrate=baud,
            timeout=timeout,
            write_timeout=timeout,
            dsrdtr=True,
            rtscts=False,
        )

    def _open_win32(self, port: str, baud: int, timeout: float) -> None:
        import ctypes
        import ctypes.wintypes as w

        kernel32 = ctypes.WinDLL("kernel32", use_last_error=True)
        GENERIC_READ = 0x80000000
        GENERIC_WRITE = 0x40000000
        OPEN_EXISTING = 3
        FILE_ATTRIBUTE_NORMAL = 0x80

        class DCB(ctypes.Structure):
            _fields_ = [
                ("DCBlength", w.DWORD),
                ("BaudRate", w.DWORD),
                ("fBinary", w.DWORD, 1),
                ("fParity", w.DWORD, 1),
                ("fOutxCtsFlow", w.DWORD, 1),
                ("fOutxDsrFlow", w.DWORD, 1),
                ("fDtrControl", w.DWORD, 2),
                ("fDsrSensitivity", w.DWORD, 1),
                ("fTXContinueOnXoff", w.DWORD, 1),
                ("fOutX", w.DWORD, 1),
                ("fInX", w.DWORD, 1),
                ("fErrorChar", w.DWORD, 1),
                ("fNull", w.DWORD, 1),
                ("fRtsControl", w.DWORD, 2),
                ("fAbortOnError", w.DWORD, 1),
                ("fDummy2", w.DWORD, 17),
                ("wReserved", w.WORD),
                ("XonLim", w.WORD),
                ("XoffLim", w.WORD),
                ("ByteSize", ctypes.c_byte),
                ("Parity", ctypes.c_byte),
                ("StopBits", ctypes.c_byte),
                ("XonChar", ctypes.c_char),
                ("XoffChar", ctypes.c_char),
                ("ErrorChar", ctypes.c_char),
                ("EofChar", ctypes.c_char),
                ("EvtChar", ctypes.c_char),
                ("wReserved1", w.WORD),
            ]

        class COMMTIMEOUTS(ctypes.Structure):
            _fields_ = [
                ("ReadIntervalTimeout", w.DWORD),
                ("ReadTotalTimeoutMultiplier", w.DWORD),
                ("ReadTotalTimeoutConstant", w.DWORD),
                ("WriteTotalTimeoutMultiplier", w.DWORD),
                ("WriteTotalTimeoutConstant", w.DWORD),
            ]

        path = port if port.startswith("\\\\.\\") else f"\\\\.\\{port}"
        h = kernel32.CreateFileW(
            path, GENERIC_READ | GENERIC_WRITE, 0, None, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, None
        )
        hv = int(ctypes.cast(h, ctypes.c_void_p).value or 0)
        if hv in (0, 0xFFFFFFFF):
            err = ctypes.get_last_error()
            raise OSError(err, f"CreateFile failed for {port}")
        kernel32.SetupComm(h, 8192, 8192)
        dcb = DCB()
        dcb.DCBlength = ctypes.sizeof(DCB)
        kernel32.GetCommState(h, ctypes.byref(dcb))
        dcb.BaudRate = int(baud)
        dcb.ByteSize = 8
        dcb.Parity = 0
        dcb.StopBits = 0
        dcb.fBinary = 1
        dcb.fParity = 0
        dcb.fOutxCtsFlow = 0
        dcb.fOutxDsrFlow = 0
        dcb.fDtrControl = 1
        dcb.fRtsControl = 1
        dcb.fOutX = 0
        dcb.fInX = 0
        dcb.fNull = 0
        dcb.fAbortOnError = 0
        # SetCommState often fails on GrokLink CDC — ignore and keep handle
        kernel32.SetCommState(h, ctypes.byref(dcb))
        # Short read polls so recv_line can honor long deadlines (RX dwells)
        timeouts = COMMTIMEOUTS(10, 0, 80, 0, 2000)
        kernel32.SetCommTimeouts(h, ctypes.byref(timeouts))
        kernel32.PurgeComm(h, 0x000F)
        self._mode = "win32"
        self._h = h
        self._k32 = kernel32
        self._w = w
        self._ctypes = ctypes
        self._COMMTIMEOUTS = COMMTIMEOUTS

    def _drain(self) -> None:
        try:
            if self._mode == "pyserial" and self._ser is not None:
                self._ser.reset_input_buffer()
            elif self._mode == "win32" and self._h is not None:
                self._k32.PurgeComm(self._h, 0x000F)
        except Exception:  # noqa: BLE001
            pass

    def send_line(self, line: str) -> None:
        data = (line if line.endswith("\n") else line + "\n").encode("utf-8")
        if self._mode == "pyserial" and self._ser is not None:
            self._ser.write(data)
            self._ser.flush()
            return
        written = self._w.DWORD(0)
        if not self._k32.WriteFile(self._h, data, len(data), self._ctypes.byref(written), None):
            raise OSError(self._ctypes.get_last_error(), "WriteFile failed")
        self._k32.FlushFileBuffers(self._h)

    def recv_line(self, timeout: float) -> str:
        deadline = time.monotonic() + max(timeout, 0.1)
        if self._mode == "pyserial" and self._ser is not None:
            self._ser.timeout = 0.1
            while time.monotonic() < deadline:
                if "\n" in self._buf:
                    raw, self._buf = self._buf.split("\n", 1)
                    line = raw.strip().strip("\r")
                    if line:
                        return line
                    continue
                chunk = self._ser.read(512)
                if chunk:
                    self._buf += chunk.decode("utf-8", errors="replace")
            if self._buf.strip():
                line = self._buf.strip().splitlines()[0].strip()
                self._buf = ""
                if line:
                    return line
            raise TimeoutError(f"serial RPC timeout after {timeout}s")

        # win32 (timeouts set at open; short ReadFile polls until deadline)
        while time.monotonic() < deadline:
            if "\n" in self._buf:
                raw, self._buf = self._buf.split("\n", 1)
                line = raw.strip().strip("\r")
                if line:
                    return line
                continue
            buf = self._ctypes.create_string_buffer(1024)
            n = self._w.DWORD(0)
            self._k32.ReadFile(self._h, buf, 1024, self._ctypes.byref(n), None)
            if n.value:
                self._buf += buf.raw[: n.value].decode("utf-8", errors="replace")
        if self._buf.strip():
            line = self._buf.strip().splitlines()[0].strip()
            self._buf = ""
            if line:
                return line
        raise TimeoutError(f"serial RPC timeout after {timeout}s")

    def close(self) -> None:
        try:
            if self._mode == "pyserial" and self._ser is not None:
                self._ser.close()
            elif self._mode == "win32" and self._h is not None:
                self._k32.CloseHandle(self._h)
        except Exception:  # noqa: BLE001
            pass
        self._ser = None
        self._h = None


def _looks_like_serial(port_or_host: str) -> bool:
    p = port_or_host.strip()
    if not p:
        return False
    up = p.upper()
    if up.startswith("COM") and up[3:].isdigit():
        return True
    if p.startswith("/dev/"):
        return True
    if p.startswith("tty") or "usbserial" in p.lower() or "usbmodem" in p.lower():
        return True
    return False


def list_serial_ports() -> list[dict[str, str]]:
    """List available serial ports (empty if pyserial missing)."""
    try:
        from serial.tools import list_ports  # type: ignore
    except ImportError:
        return []
    out: list[dict[str, str]] = []
    for p in list_ports.comports():
        out.append(
            {
                "device": p.device,
                "description": p.description or "",
                "hwid": p.hwid or "",
            }
        )
    return out


# GrokLink CDC PID (distinct from Flipper stock 0x5740). Also accept legacy 5740.
GLK_USB_VID = 0x0483
GLK_USB_PID_CDC = 0x6C4B
GLK_USB_PID_CDC_LEGACY = 0x5740  # pre-identity firmware / ST default


def find_device_serial_port() -> Optional[str]:
    """Prefer GrokLink CDC (0483:6C4B), then legacy 5740 / product string."""
    try:
        from serial.tools import list_ports  # type: ignore
    except ImportError:
        return None
    ports = list(list_ports.comports())
    for p in ports:
        if p.vid == GLK_USB_VID and p.pid == GLK_USB_PID_CDC:
            return p.device
    for p in ports:
        desc = (p.description or "") + " " + (p.manufacturer or "")
        if "GrokLink" in desc:
            return p.device
    for p in ports:
        if p.vid == GLK_USB_VID and p.pid == GLK_USB_PID_CDC_LEGACY:
            return p.device
    for p in ports:
        hwid = (p.hwid or "").upper()
        if "VID_0483" in hwid and ("PID_6C4B" in hwid or "PID_5740" in hwid):
            return p.device
    return None


def open_client(
    *,
    host: Optional[str] = None,
    port: Optional[int] = None,
    serial_port: Optional[str] = None,
    baud: Optional[int] = None,
    timeout: float = 10.0,
    prefer: Optional[str] = None,
) -> "GrokLinkClient":
    """Create a client from env / args.

    Priority:
      1. explicit serial_port or GLK_SERIAL_PORT
      2. prefer=serial | tcp
      3. GLK_RPC_TRANSPORT=serial|tcp
      4. if GLK_RPC_HOST looks like COMx → serial
      5. auto STM32 CDC 0483:5740 if present
      6. TCP 127.0.0.1:7341
    """
    env_serial = os.environ.get("GLK_SERIAL_PORT") or os.environ.get("GLK_CDC_PORT")
    transport = (prefer or os.environ.get("GLK_RPC_TRANSPORT") or "").strip().lower()
    h = host if host is not None else os.environ.get("GLK_RPC_HOST", "127.0.0.1")
    sp = serial_port or env_serial
    if not sp and _looks_like_serial(h):
        sp = h
    if not sp and transport != "tcp":
        sp = find_device_serial_port()
    use_serial = bool(sp) or transport == "serial"
    if transport == "tcp":
        use_serial = False
    c = GrokLinkClient(
        host=None if use_serial else h,
        port=port,
        serial_port=sp if use_serial else None,
        baud=baud,
        timeout=timeout,
    )
    c.connect()
    return c


class GrokLinkClient:
    """JSON-line GrokRPC over TCP (host sim) or USB CDC serial (device)."""

    def __init__(
        self,
        host: Optional[str] = None,
        port: Optional[int] = None,
        timeout: float = 10.0,
        *,
        serial_port: Optional[str] = None,
        baud: Optional[int] = None,
    ) -> None:
        self.host = host if host is not None else os.environ.get("GLK_RPC_HOST", "127.0.0.1")
        raw_port = port if port is not None else os.environ.get("GLK_RPC_PORT", "7341")
        # Allow GLK_RPC_PORT=COM5 mistake → treat as serial
        if isinstance(raw_port, str) and _looks_like_serial(raw_port):
            self.serial_port = raw_port
            self.port = 7341
        else:
            self.port = int(raw_port)
            self.serial_port = serial_port or os.environ.get("GLK_SERIAL_PORT") or os.environ.get(
                "GLK_CDC_PORT"
            )
        if self.serial_port is None and _looks_like_serial(str(self.host)):
            self.serial_port = str(self.host)
        self.baud = int(baud or os.environ.get("GLK_SERIAL_BAUD") or os.environ.get("GLK_CDC_BAUD") or 230400)
        self.timeout = timeout
        self._t: Optional[_Transport] = None
        self.transport_name = "serial" if self.serial_port else "tcp"

    def connect(self) -> None:
        # Avoid leaking exclusive COM handles (Windows CDC allows one opener).
        if self._t is not None:
            return
        if self.serial_port:
            self._t = _SerialTransport(self.serial_port, self.baud, self.timeout)
            self.transport_name = "serial"
        else:
            self._t = _TcpTransport(str(self.host), int(self.port), self.timeout)
            self.transport_name = "tcp"

    def close(self) -> None:
        if self._t:
            try:
                self._t.close()
            except Exception:  # noqa: BLE001
                pass
            self._t = None

    def __enter__(self) -> "GrokLinkClient":
        self.connect()
        return self

    def __exit__(self, *args: object) -> None:
        self.close()

    def call(self, cmd: str, **kwargs: Any) -> dict[str, Any]:
        if not self._t:
            self.connect()
        assert self._t is not None
        payload = {"cmd": cmd, **kwargs}
        line = json.dumps(payload, separators=(",", ":"))
        self._t.send_line(line)
        # RX windows can take longer than default connect timeout
        rx_timeout = self.timeout
        if cmd in ("subghz_rx", "spectrum"):
            ms = int(kwargs.get("ms") or 500)
            settle = int(kwargs.get("settle_ms") or 0)
            n_freq = len(kwargs.get("freqs") or [1])
            rx_timeout = max(self.timeout, (ms + settle) * max(n_freq, 1) / 1000.0 + 3.0)
        raw = self._t.recv_line(rx_timeout)
        # Serial may interleave banners; find JSON object
        if not raw.startswith("{"):
            for part in raw.splitlines():
                part = part.strip()
                if part.startswith("{") and part.endswith("}"):
                    raw = part
                    break
            else:
                # try extract first {...}
                i = raw.find("{")
                j = raw.rfind("}")
                if i >= 0 and j > i:
                    raw = raw[i : j + 1]
        return json.loads(raw)

    def ping(self) -> dict[str, Any]:
        return self.call("ping")

    def edu_ack(self, phrase: str = "I_WILL_USE_ONLY_AUTHORIZED_TARGETS") -> dict[str, Any]:
        return self.call("edu_ack", phrase=phrase)

    def status(self) -> dict[str, Any]:
        return self.call("status")

    def subghz_rx(self, freq_hz: int = 433_920_000, ms: int = 500) -> dict[str, Any]:
        return self.call("subghz_rx", freq_hz=freq_hz, ms=ms)

    def subghz_probe(self) -> dict[str, Any]:
        return self.call("subghz_probe")

    def subghz_tx(
        self,
        freq_hz: int,
        path: str,
        confirm_id: str,
    ) -> dict[str, Any]:
        return self.call(
            "subghz_tx",
            freq_hz=freq_hz,
            path=path,
            confirm_id=confirm_id,
        )

    def confirm_issue(self, action: str, ttl_sec: int = 60, freq_hz: int = 0) -> dict[str, Any]:
        return self.call("confirm_issue", action=action, ttl_sec=ttl_sec, freq_hz=freq_hz)

    def spectrum(self, freqs: list[int], ms: int = 400, settle_ms: int = 2000) -> dict[str, Any]:
        return self.call("spectrum", freqs=freqs, ms=ms, settle_ms=settle_ms)

    def mission_list(self) -> dict[str, Any]:
        return self.call("mission_list")

    def mission_arm(self, mission_id: str) -> dict[str, Any]:
        return self.call("mission_arm", id=mission_id)

    def mission_disarm(self, mission_id: str = "") -> dict[str, Any]:
        if mission_id:
            return self.call("mission_disarm", id=mission_id)
        return self.call("mission_disarm")

    def mission_status(self, mission_id: str = "") -> dict[str, Any]:
        if mission_id:
            return self.call("mission_status", id=mission_id)
        return self.call("mission_status")

    def mission_step(self) -> dict[str, Any]:
        return self.call("mission_step")

    def mission_run(self, steps: int = 8) -> dict[str, Any]:
        return self.call("mission_run", steps=int(steps))

    def skill_list(self) -> dict[str, Any]:
        return self.call("skill_list")

    def audit_tail(self) -> dict[str, Any]:
        return self.call("audit_tail")

    def agent_status(self) -> dict[str, Any]:
        return self.call("agent_status")

    def agent_offline(self, on: bool = True) -> dict[str, Any]:
        return self.call("agent_offline", on=on)

    def agent_auto(self, mission_id: str, on: bool = True) -> dict[str, Any]:
        return self.call("agent_auto", id=mission_id, on=on)

    def vault_tail(self, n: int = 8) -> dict[str, Any]:
        return self.call("vault_tail", n=int(n))

    def vault_clear(self) -> dict[str, Any]:
        return self.call("vault_clear")

    def catalog_reload(self) -> dict[str, Any]:
        return self.call("catalog_reload")
