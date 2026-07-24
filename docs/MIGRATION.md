# Migration — GrokLink-Firmware v2 → GrokLink OS 3.0

## Conceptual continuity

You do **not** need a new mental model for day-to-day lab work:

- Edu phrase unchanged  
- Risk classes unchanged  
- Confirm tokens unchanged  
- Skill craft loop unchanged  
- SD tree still `/groklink/{missions,skills,logs,config,blacklist}`  

## What changes

| Area | v2 | OS 3.0 |
|------|----|--------|
| Runtime | Overlay on Furi/Momentum | Native RTOS |
| Build | `apply_overlay` + fbt | CMake host / ARM toolchain |
| Bridge package | `groklink` | `groklink-os` |
| RPC | CLI JSON primarily | TCP/USB JSON + binary frames |
| Spectrum | Often PC-only | Device planner OK with settle |
| Agent | ~1 Hz | Event-driven + IR opcodes |
| Flash image | Full Momentum + overlay | GrokLink OS image |

## Porting assets

1. Copy SD `groklink/` tree — manifests and missions largely load as-is.  
2. Re-test any `active_tx` skills under confirm flow.  
3. Replace `py -m groklink.cli` with `groklink-os`.  
4. Retire overlay DFUs; do not mix Furi FAPs expecting stock OS services.

## Healthcare templates

v2 healthcare folders remain ethically valid as **passive research only**.
Copy templates into OS SD tree; do not enable clinical claims.
