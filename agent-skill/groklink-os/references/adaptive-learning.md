# Adaptive learning model

The skill stays current without hardcoding firmware details that rot.

## Stores

| Path | Purpose |
|------|---------|
| `$GROKLINK_OS_LEARN_DIR` or `~/.grok/state/groklink-os/` | Mutable learning root |
| `firmware_snapshot.json` | Docs + release + CLI map from public repo / local clone |
| `capabilities.json` | Live probe of device/RPC (passive only) |
| `learnings_index.json` | Index of ingested sessions/captures/notes |
| `band_notes.json` | Lab frequency buckets from authorized captures |
| `sessions/` `captures/` `notes/` `drafts/` | Raw artifacts (local only; not published) |

## Refresh loop (session start + after flash)

```powershell
py -3 scripts/refresh_all.py
# or
py -3 scripts/main.py refresh
```

1. **sync_firmware_knowledge** — local clone and/or GitHub latest release + RPC.md + VERSION
2. **probe_capabilities** — passive RPC (ping/status/lists); never TX
3. **learn summary** — show what the store already knows

## Learn from new data

```powershell
py -3 scripts/learn_from_data.py --capture path/to/rx.jsonl
py -3 scripts/learn_from_data.py --session path/to/session.json
py -3 scripts/learn_from_data.py --note "433.92 quiet after 22:00 local" --title lab-band
py -3 scripts/learn_from_data.py --summary
```

Session helper auto-logs to `sessions/` unless `--no-learn`.

## Agent rules for adaptation

1. Prefer `capabilities.json` + `firmware_snapshot.json` over SKILL.md CLI tables when they disagree.
2. If `delta.changes` reports new RPC/CLI commands after sync, use those and update operator report.
3. After craft, ingest the capture so band notes accumulate.
4. Never commit learning store contents to public repos (may contain lab captures).
5. Safety rules never adapt away — default-deny and human TX confirm are fixed.

## Environment

| Variable | Meaning |
|----------|---------|
| `GROKLINK_OS_ROOT` | Local firmware clone |
| `GROKLINK_OS_LEARN_DIR` | Override learning store path |
| `GLK_RPC_HOST` / `GLK_RPC_PORT` | RPC endpoint (default 127.0.0.1:7341) |
