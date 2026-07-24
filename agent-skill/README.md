# Agent skills for GrokLink OS

Portable [Agent Skills](https://agentskills.io) that teach coding agents how to operate
**GrokLink OS** safely through the PC bridge.

| Skill | Path | Purpose |
|-------|------|---------|
| **groklink-os** | [groklink-os/](groklink-os/) | Adaptive bridge ops: sync firmware knowledge, probe capabilities, passive RX, craft skills, human-gated TX |

## Install (any Agent Skills client)

```powershell
# Grok
Copy-Item -Recurse groklink-os $HOME/.grok/skills/groklink-os

# Cursor
Copy-Item -Recurse groklink-os $HOME/.cursor/skills/groklink-os

# Claude Code / agents
Copy-Item -Recurse groklink-os $HOME/.claude/skills/groklink-os
Copy-Item -Recurse groklink-os $HOME/.agents/skills/groklink-os
```

Then install the Python bridge from `../bridge` and run:

```powershell
py -3 $HOME/.grok/skills/groklink-os/scripts/refresh_all.py
```

## Legal

Authorized research and education only. See skill `references/legal.md` and the root `README.md` warning.
Learning stores hold local lab data — do not publish `~/.grok/state/groklink-os/`.
