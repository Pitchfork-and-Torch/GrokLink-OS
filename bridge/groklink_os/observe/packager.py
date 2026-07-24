"""Package raw device RPC into self-describing observations (v2 Signal Cognition)."""

from __future__ import annotations

from typing import Any, Optional

from groklink_os.observe.calibration import CalibrationStore
from groklink_os.observe.schema import (
    ObservationKind,
    band_label,
    build_observation,
    calibrated_occupancy,
    energy_score,
    occupancy_from_activity,
    pulse_rate_hz,
)


class ObservationPackager:
    """Host-side packaging — never issues TX, never decodes payloads."""

    def __init__(
        self,
        device_ctx: Optional[dict[str, Any]] = None,
        *,
        calibration: Optional[CalibrationStore] = None,
    ) -> None:
        self.device_ctx = dict(device_ctx or {})
        self.calibration = calibration or CalibrationStore()

    def update_device_ctx(self, **kwargs: Any) -> None:
        self.device_ctx.update(kwargs)

    def _policy(self) -> dict[str, Any]:
        return {
            "passive_only": True,
            "edu_required": True,
            "edu_acked": bool(self.device_ctx.get("edu")),
            "tx_available": False,
            "note": "Observation path is strictly passive; TX remains default-deny.",
        }

    def _extract_stats(self, raw: dict[str, Any], *, dwell_ms: int, pulses_i: int, rssi: Optional[int]) -> dict[str, Any]:
        pr = raw.get("pulse_rate_hz")
        if pr is None:
            pr = pulse_rate_hz(pulses_i, dwell_ms)
        else:
            try:
                pr = float(pr)
            except (TypeError, ValueError):
                pr = pulse_rate_hz(pulses_i, dwell_ms)
        rmin = raw.get("rssi_min")
        rmax = raw.get("rssi_max")
        if rmin is None:
            rmin = raw.get("rssi_min_dbm")
        if rmax is None:
            rmax = raw.get("rssi_max_dbm")
        try:
            rmin_i = int(rmin) if rmin is not None else rssi
        except (TypeError, ValueError):
            rmin_i = rssi
        try:
            rmax_i = int(rmax) if rmax is not None else rssi
        except (TypeError, ValueError):
            rmax_i = rssi
        return {
            "pulse_rate_hz": pr,
            "rssi_min_dbm": rmin_i,
            "rssi_max_dbm": rmax_i,
            "rssi_dbm": rssi,
            "pulses": pulses_i,
            "dwell_ms": dwell_ms,
        }

    def _build_events(
        self,
        *,
        pulses_i: int,
        dwell_ms: int,
        pr: float,
        cal_occ: str,
        snr: Optional[float],
        is_noise_sample: bool = False,
    ) -> list[dict[str, Any]]:
        events: list[dict[str, Any]] = []
        if is_noise_sample:
            events.append(
                {
                    "type": "noise_floor_sample",
                    "duration_ms": dwell_ms,
                    "pulse_rate_hz": pr,
                    "payload_hex": None,
                    "note": "Used to update host band baseline; not a decode.",
                }
            )
        if pulses_i == 0:
            events.append(
                {
                    "type": "quiet_dwell",
                    "duration_ms": dwell_ms,
                    "pulse_count": 0,
                    "pulse_rate_hz": 0.0,
                    "payload_hex": None,
                    "note": "No GDO0 edges in dwell window.",
                }
            )
        else:
            events.append(
                {
                    "type": "edge_activity",
                    "duration_ms": dwell_ms,
                    "pulse_count": pulses_i,
                    "pulse_rate_hz": pr,
                    "modulation_hint": "ook_async_edges",
                    "payload_hex": None,
                    "note": "Edge counts only; no payload decode.",
                }
            )
        if cal_occ in ("elevated", "busy") and (snr is None or snr >= 3):
            events.append(
                {
                    "type": "elevated_vs_noise",
                    "duration_ms": dwell_ms,
                    "pulse_rate_hz": pr,
                    "delta_db": snr,
                    "payload_hex": None,
                    "note": "Relative elevation vs host calibration baseline.",
                }
            )
        return events

    def package_rx(
        self,
        raw: dict[str, Any],
        *,
        request: Optional[dict[str, Any]] = None,
        update_calibration: bool = False,
        as_noise_floor: bool = False,
    ) -> dict[str, Any]:
        freq = int(raw.get("freq_hz") or (request or {}).get("freq_hz") or 433_920_000)
        ms = int(raw.get("ms") or raw.get("duration_ms") or (request or {}).get("ms") or 400)
        pulses = raw.get("pulses")
        rssi = raw.get("rssi") if "rssi" in raw else raw.get("rssi_est")
        if rssi is not None:
            try:
                rssi = int(rssi)
            except (TypeError, ValueError):
                rssi = None
        try:
            pulses_i = int(pulses) if pulses is not None else 0
        except (TypeError, ValueError):
            pulses_i = 0

        stats = self._extract_stats(raw, dwell_ms=ms, pulses_i=pulses_i, rssi=rssi)
        pr = float(stats["pulse_rate_hz"] or 0.0)
        occ = occupancy_from_activity(pulses_i, rssi, dwell_ms=ms)
        score = energy_score(pulses_i, rssi, dwell_ms=ms)
        sim = bool(raw.get("sim") or raw.get("simulated") or self.device_ctx.get("simulated"))

        if as_noise_floor or update_calibration:
            self.calibration.update_from_sample(
                freq_hz=freq,
                rssi_dbm=rssi,
                pulse_rate_hz=pr,
                method="observe_noise_floor" if as_noise_floor else "host_rolling",
            )

        cal = self.calibration.calibration_block_for(freq)
        cal_occ, conf, snr = calibrated_occupancy(
            pulse_rate=pr,
            rssi_dbm=rssi,
            baseline_pulse_rate=cal.get("baseline_pulse_rate_hz"),
            noise_floor_dbm=cal.get("noise_floor_dbm"),
        )
        cal["snr_est_db"] = snr

        events = self._build_events(
            pulses_i=pulses_i,
            dwell_ms=ms,
            pr=pr,
            cal_occ=cal_occ,
            snr=snr,
            is_noise_sample=as_noise_floor,
        )

        narrative = (
            f"Passive RX at {freq / 1e6:.3f} MHz for {ms} ms: "
            f"pulses={pulses_i}, rate={pr:.1f} Hz, rssi={rssi if rssi is not None else 'n/a'} dBm, "
            f"occupancy={occ}, calibrated={cal_occ}"
        )
        if snr is not None:
            narrative += f", snr_est={snr:.1f} dB"
        narrative += f", simulated={sim}."

        kind = ObservationKind.NOISE_FLOOR if as_noise_floor else ObservationKind.RX_SNAPSHOT
        return build_observation(
            kind,
            device={
                **self.device_ctx,
                "simulated": sim,
                "ok": bool(raw.get("ok", True)),
                "err": raw.get("err"),
            },
            policy_context=self._policy(),
            rf={
                "freq_hz": freq,
                "freq_mhz": round(freq / 1e6, 6),
                "bandwidth_hz": None,
                "dwell_ms": ms,
                "band_label": band_label(freq),
            },
            stats=stats,
            calibration=cal,
            activity={
                "pulses": pulses_i,
                "rssi_dbm": rssi,
                "energy_score": score,
                "occupancy": occ,
                "calibrated_occupancy": cal_occ,
                "confidence": conf,
                "modulation_hint": "ook_async_edges",
            },
            events=events,
            raw_device=raw,
            narrative=narrative,
            device_mono_ms=_as_int(raw.get("ts_ms")),
        )

    def package_spectrum(
        self,
        raw: dict[str, Any],
        *,
        request: Optional[dict[str, Any]] = None,
    ) -> dict[str, Any]:
        bands_in = raw.get("bands") or []
        dwell = int((request or {}).get("ms") or raw.get("ms") or 400)
        settle = int((request or {}).get("settle_ms") or raw.get("settle_ms") or 2000)
        bands_out: list[dict[str, Any]] = []
        hottest: Optional[dict[str, Any]] = None
        events: list[dict[str, Any]] = []
        for b in bands_in:
            if not isinstance(b, dict):
                continue
            freq = int(b.get("freq_hz") or 0)
            pulses = int(b.get("pulses") or 0)
            rssi = b.get("rssi") if "rssi" in b else b.get("rssi_est")
            if rssi is not None:
                try:
                    rssi = int(rssi)
                except (TypeError, ValueError):
                    rssi = None
            pr = float(b.get("pulse_rate_hz") or pulse_rate_hz(pulses, dwell))
            occ = occupancy_from_activity(pulses, rssi, dwell_ms=dwell)
            score = energy_score(pulses, rssi, dwell_ms=dwell)
            cal = self.calibration.calibration_block_for(freq) if freq else {
                "noise_floor_dbm": None,
                "baseline_pulse_rate_hz": None,
                "method": "none",
            }
            cal_occ, conf, snr = calibrated_occupancy(
                pulse_rate=pr,
                rssi_dbm=rssi,
                baseline_pulse_rate=cal.get("baseline_pulse_rate_hz"),
                noise_floor_dbm=cal.get("noise_floor_dbm"),
            )
            row = {
                "freq_hz": freq,
                "freq_mhz": round(freq / 1e6, 6),
                "pulses": pulses,
                "pulse_rate_hz": pr,
                "rssi_dbm": rssi,
                "occupancy": occ,
                "calibrated_occupancy": cal_occ,
                "confidence": conf,
                "snr_est_db": snr,
                "energy_score": score,
                "band_label": band_label(freq) if freq else "unknown",
            }
            bands_out.append(row)
            if hottest is None or score > float(hottest.get("energy_score") or 0):
                hottest = row
        if hottest:
            events.append(
                {
                    "type": "band_hotspot",
                    "pulse_count": hottest.get("pulses"),
                    "pulse_rate_hz": hottest.get("pulse_rate_hz"),
                    "payload_hex": None,
                    "note": f"Hottest band {hottest.get('freq_mhz')} MHz by energy_score (relative).",
                }
            )
        narrative = (
            f"Spectrum scan of {len(bands_out)} band(s), dwell={dwell} ms, settle={settle} ms. "
        )
        if hottest:
            narrative += (
                f"Hottest: {hottest['freq_mhz']} MHz "
                f"(pulses={hottest['pulses']}, calibrated={hottest['calibrated_occupancy']}, "
                f"occupancy={hottest['occupancy']})."
            )
        else:
            narrative += "No band results."
        return build_observation(
            ObservationKind.SPECTRUM_SCAN,
            device={**self.device_ctx, "ok": bool(raw.get("ok", True))},
            policy_context=self._policy(),
            spectrum={
                "bands": bands_out,
                "dwell_ms": dwell,
                "settle_ms": settle,
                "hottest": hottest,
            },
            events=events,
            activity={
                "band_count": len(bands_out),
                "hottest_freq_hz": (hottest or {}).get("freq_hz"),
                "hottest_energy_score": (hottest or {}).get("energy_score"),
                "occupancy": (hottest or {}).get("occupancy") or "unknown",
                "calibrated_occupancy": (hottest or {}).get("calibrated_occupancy") or "unknown",
            },
            raw_device=raw,
            narrative=narrative,
            device_mono_ms=_as_int(raw.get("ts_ms")),
        )

    def package_compare(
        self,
        raw_a: dict[str, Any],
        raw_b: dict[str, Any],
        *,
        freq_a: int,
        freq_b: int,
        ms: int,
    ) -> dict[str, Any]:
        oa = self.package_rx(raw_a, request={"freq_hz": freq_a, "ms": ms})
        ob = self.package_rx(raw_b, request={"freq_hz": freq_b, "ms": ms})
        ea = float((oa.get("activity") or {}).get("energy_score") or 0)
        eb = float((ob.get("activity") or {}).get("energy_score") or 0)
        hotter = "a" if ea >= eb else "b"
        hot_obs = oa if hotter == "a" else ob
        narrative = (
            f"Compare passive RX: A={freq_a/1e6:.3f} MHz energy={ea:.3f} "
            f"cal={(oa.get('activity') or {}).get('calibrated_occupancy')}; "
            f"B={freq_b/1e6:.3f} MHz energy={eb:.3f} "
            f"cal={(ob.get('activity') or {}).get('calibrated_occupancy')}. "
            f"Hotter={hotter.upper()} ({(hot_obs.get('rf') or {}).get('freq_mhz')} MHz)."
        )
        return build_observation(
            ObservationKind.BAND_COMPARE,
            device=dict(self.device_ctx),
            policy_context=self._policy(),
            window={
                "freq_a_hz": freq_a,
                "freq_b_hz": freq_b,
                "dwell_ms": ms,
                "hotter": hotter,
                "energy_a": ea,
                "energy_b": eb,
            },
            activity={
                "occupancy": (hot_obs.get("activity") or {}).get("occupancy"),
                "calibrated_occupancy": (hot_obs.get("activity") or {}).get("calibrated_occupancy"),
                "energy_score": max(ea, eb),
            },
            events=[
                {
                    "type": "band_hotspot",
                    "payload_hex": None,
                    "note": f"Compare winner: {hotter}",
                }
            ],
            narrative=narrative,
            extra={"observation_a": oa, "observation_b": ob},
        )

    def package_status(self, raw: dict[str, Any], *, probe: Optional[dict[str, Any]] = None) -> dict[str, Any]:
        self.update_device_ctx(
            version=raw.get("version"),
            api=raw.get("api"),
            edu=raw.get("edu"),
            radio=raw.get("radio"),
            heap_free=raw.get("heap_free"),
            simulated=None if probe is None else not bool(probe.get("hw")),
        )
        if probe:
            self.update_device_ctx(
                partnum=probe.get("partnum"),
                cc1101_version=probe.get("version"),
                hw=probe.get("hw"),
            )
        narrative = (
            f"Device status version={raw.get('version')} edu={raw.get('edu')} "
            f"radio={raw.get('radio')} heap_free={raw.get('heap_free')}."
        )
        return build_observation(
            ObservationKind.DEVICE_STATUS,
            device={**self.device_ctx, **{k: raw.get(k) for k in raw if k != "ok"}},
            policy_context=self._policy(),
            raw_device={"status": raw, "probe": probe},
            narrative=narrative,
            device_mono_ms=_as_int(raw.get("ts_ms")),
        )

    def package_calibration_state(self) -> dict[str, Any]:
        state = self.calibration.as_dict()
        narrative = (
            f"Host calibration: {state.get('count', 0)} band baseline(s) stored. "
            f"Passive only; baselines are local heuristics, not lab instruments."
        )
        return build_observation(
            ObservationKind.CALIBRATION_STATE,
            device=dict(self.device_ctx),
            policy_context=self._policy(),
            window=state,
            narrative=narrative,
        )

    def package_monitor_chunk(
        self,
        samples: list[dict[str, Any]],
        *,
        session_id: str,
        freqs_hz: list[int],
        interval_ms: int,
        chunk_index: int,
    ) -> dict[str, Any]:
        occ_counts: dict[str, int] = {}
        cal_counts: dict[str, int] = {}
        max_score = 0.0
        peak_freq = None
        for s in samples:
            act = s.get("activity") or {}
            occ = str(act.get("occupancy") or "unknown")
            occ_counts[occ] = occ_counts.get(occ, 0) + 1
            c_occ = str(act.get("calibrated_occupancy") or "unknown")
            cal_counts[c_occ] = cal_counts.get(c_occ, 0) + 1
            sc = float(act.get("energy_score") or 0)
            if sc >= max_score:
                max_score = sc
                peak_freq = (s.get("rf") or {}).get("freq_hz")
        narrative = (
            f"Monitor chunk #{chunk_index} session={session_id}: "
            f"{len(samples)} sample(s) across {freqs_hz}, "
            f"peak_energy={max_score:.3f} @ {peak_freq}, "
            f"calibrated_counts={cal_counts}."
        )
        return build_observation(
            ObservationKind.MONITOR_CHUNK,
            device=dict(self.device_ctx),
            policy_context=self._policy(),
            window={
                "session_id": session_id,
                "chunk_index": chunk_index,
                "sample_count": len(samples),
                "freqs_hz": list(freqs_hz),
                "interval_ms": interval_ms,
                "occupancy_counts": occ_counts,
                "calibrated_occupancy_counts": cal_counts,
                "peak_energy_score": max_score,
                "peak_freq_hz": peak_freq,
            },
            events=[{"type": "sample_ref", "observation_id": s.get("observation_id"), "payload_hex": None} for s in samples],
            activity={
                "sample_count": len(samples),
                "occupancy_counts": occ_counts,
                "calibrated_occupancy_counts": cal_counts,
                "energy_score": max_score,
                "occupancy": max(occ_counts, key=occ_counts.get) if occ_counts else "unknown",
                "calibrated_occupancy": max(cal_counts, key=cal_counts.get) if cal_counts else "unknown",
            },
            narrative=narrative,
            extra={"samples": samples},
        )

    def package_activity_summary(self, summary: dict[str, Any]) -> dict[str, Any]:
        narrative = (
            f"Recent activity: {summary.get('count', 0)} observations; "
            f"occupancy={summary.get('occupancy_counts')}; "
            f"bands={summary.get('by_band')}."
        )
        return build_observation(
            ObservationKind.ACTIVITY_SUMMARY,
            device=dict(self.device_ctx),
            policy_context=self._policy(),
            window=summary,
            activity={
                "count": summary.get("count"),
                "occupancy_counts": summary.get("occupancy_counts"),
            },
            narrative=narrative,
        )


def _as_int(v: Any) -> Optional[int]:
    if v is None:
        return None
    try:
        return int(v)
    except (TypeError, ValueError):
        return None
