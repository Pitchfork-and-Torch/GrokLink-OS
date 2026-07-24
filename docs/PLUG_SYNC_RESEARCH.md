# Plug-sync research pipeline

**Every USB reconnect** after unplugged field explore should pull on-device lessons into the PC learning store for examination and research updates.

## Workflow

```
  unplugged (field agent)          plug USB              PC research store
  ─────────────────────           ────────              ─────────────────
  passive RX / vault     ──►   plug-sync   ──►  field_trips/*.json
  mission IR / infer                │           notes/unplugged_research_journal.md
                                    │           research/plug_sync_index.json
                                    │           research/lessons_learned_summary.json
                                    ▼
                            optional: vault_clear
                            optional: observe_rx (live)
```

## Commands

```powershell
# One-shot after you plug in
groklink-os plug-sync

# Drain RAM vault after copy (recommended on field unit)
groklink-os plug-sync --clear-vault

# Automation: wait until device appears
groklink-os plug-sync --wait --timeout 300

# Background watcher (reconnect → auto ingest)
.\tools\watch_plug_sync.ps1
.\tools\watch_plug_sync.ps1 -ClearVaultAfterIngest
```

LLM tool: `ingest_unplugged_lessons`  
Skill script: `agent-skill/groklink-os/scripts/ingest_unplugged.py`

## What gets examined

| Source | Use |
|--------|-----|
| `vault_tail` | RX / infer / done / auto events while unplugged |
| `agent_status` | offline flag, active mission, cycles, vault size |
| `status` / `ping` | firmware version, mission count |

Derived **lessons** (examples):

- Event counts by kind and mission  
- Pulse min/max/mean on RX ticks  
- Empty vault → power loss vs no activity  
- Explicit: no third-party decode from pulse counts  

## Where it lives (local only)

Default: `~/.grok/state/groklink-os/`

| Path | Content |
|------|---------|
| `field_trips/<id>.json` | Full snapshot + analysis |
| `notes/unplugged_research_journal.md` | Human-readable append-only journal |
| `research/plug_sync_index.json` | Trip index |
| `research/lessons_learned_summary.json` | Latest rollup |
| `sessions/unplugged_*.json` | Session-shaped records |

**Do not commit** this store to public git (may contain lab context).  

## Agent rule

On any session where the Flipper is (re)connected:

1. `ingest_unplugged_lessons` / `plug-sync`  
2. Read `lessons_learned_summary.json` + latest field trip  
3. Optionally `observe_noise_floor` + `observe_rx` for fresh host packaging  
4. Update research notes / operator summary  

Never invent protocol IDs from vault pulse counts.
