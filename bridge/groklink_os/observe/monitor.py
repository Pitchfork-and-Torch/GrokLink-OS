"""Host-side passive monitor sessions (chunked continuous observation)."""

from __future__ import annotations

import threading
import time
import uuid
from collections import deque
from typing import Any, Callable, Deque, Optional

from groklink_os.observe.packager import ObservationPackager
from groklink_os.observe.store import ObservationStore


class MonitorSession:
    """Background passive RX loop on the host; device stays request/response."""

    def __init__(
        self,
        rx_fn: Callable[[int, int], dict[str, Any]],
        packager: ObservationPackager,
        store: ObservationStore,
        *,
        freqs_hz: Optional[list[int]] = None,
        dwell_ms: int = 200,
        interval_ms: int = 800,
        chunk_size: int = 4,
        max_chunks: int = 64,
    ) -> None:
        self.rx_fn = rx_fn
        self.packager = packager
        self.store = store
        self.freqs_hz = list(freqs_hz or [433_920_000])
        self.dwell_ms = max(50, min(int(dwell_ms), 500))
        self.interval_ms = max(200, int(interval_ms))
        self.chunk_size = max(1, int(chunk_size))
        self.session_id = uuid.uuid4().hex[:12]
        self._stop = threading.Event()
        self._thread: Optional[threading.Thread] = None
        self._lock = threading.RLock()
        self._pending_samples: list[dict[str, Any]] = []
        self._chunks: Deque[dict[str, Any]] = deque(maxlen=max_chunks)
        self._chunk_index = 0
        self._error: Optional[str] = None
        self.running = False

    def start(self) -> dict[str, Any]:
        if self.running:
            return self.status()
        self._stop.clear()
        self.running = True
        self._thread = threading.Thread(target=self._loop, name=f"glk-mon-{self.session_id}", daemon=True)
        self._thread.start()
        self.store.audit(
            "monitor_start",
            {
                "session_id": self.session_id,
                "freqs_hz": self.freqs_hz,
                "dwell_ms": self.dwell_ms,
                "interval_ms": self.interval_ms,
            },
        )
        return self.status()

    def stop(self) -> dict[str, Any]:
        self._stop.set()
        if self._thread and self._thread.is_alive():
            self._thread.join(timeout=max(2.0, (self.interval_ms + self.dwell_ms) / 1000.0 + 1.0))
        self.running = False
        # flush remaining samples
        with self._lock:
            if self._pending_samples:
                self._emit_chunk(list(self._pending_samples))
                self._pending_samples.clear()
        self.store.audit("monitor_stop", {"session_id": self.session_id})
        return self.status()

    def status(self) -> dict[str, Any]:
        with self._lock:
            return {
                "ok": True,
                "session_id": self.session_id,
                "running": self.running and not self._stop.is_set(),
                "freqs_hz": list(self.freqs_hz),
                "dwell_ms": self.dwell_ms,
                "interval_ms": self.interval_ms,
                "chunks_ready": len(self._chunks),
                "chunk_index_next": self._chunk_index,
                "error": self._error,
                "safety": {"tx": False, "path": "passive_monitor"},
            }

    def get_chunk(self, *, wait_ms: int = 0) -> Optional[dict[str, Any]]:
        deadline = time.monotonic() + max(0, wait_ms) / 1000.0
        while True:
            with self._lock:
                if self._chunks:
                    return self._chunks.popleft()
            if time.monotonic() >= deadline:
                return None
            time.sleep(0.05)

    def _loop(self) -> None:
        freq_i = 0
        try:
            while not self._stop.is_set():
                freq = self.freqs_hz[freq_i % len(self.freqs_hz)]
                freq_i += 1
                try:
                    raw = self.rx_fn(freq, self.dwell_ms)
                    obs = self.packager.package_rx(raw, request={"freq_hz": freq, "ms": self.dwell_ms})
                    self.store.append(obs)
                    with self._lock:
                        self._pending_samples.append(obs)
                        if len(self._pending_samples) >= self.chunk_size:
                            chunk = self._emit_chunk(list(self._pending_samples))
                            self._pending_samples.clear()
                            # chunk already stored in _emit_chunk
                            _ = chunk
                except Exception as exc:  # noqa: BLE001
                    self._error = str(exc)
                    self.store.audit("monitor_error", {"session_id": self.session_id, "error": str(exc)})
                # wait interval (USB-safe spacing on host)
                self._stop.wait(self.interval_ms / 1000.0)
        finally:
            self.running = False

    def _emit_chunk(self, samples: list[dict[str, Any]]) -> dict[str, Any]:
        chunk = self.packager.package_monitor_chunk(
            samples,
            session_id=self.session_id,
            freqs_hz=self.freqs_hz,
            interval_ms=self.interval_ms,
            chunk_index=self._chunk_index,
        )
        self._chunk_index += 1
        self.store.append(chunk)
        self._chunks.append(chunk)
        return chunk
