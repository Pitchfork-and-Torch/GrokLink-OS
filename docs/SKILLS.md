# Skills — GrokLink OS 3.0

## Layout

```text
/groklink/skills/<skill_id>/
  manifest.json
  rules.json
  protocol.json      # optional PC decoder hints
  module.bin         # optional native/bytecode
  README.md
```

## manifest.json

```json
{
  "id": "lab_passive_listen",
  "version": "3.0.0",
  "risk_class": "passive_rx",
  "hw": ["subghz"],
  "description": "..."
}
```

`risk_class`: `passive_rx` | `active_tx` | `gpio` | `contact` | `system`

## Signing (optional)

When `GLK_FEATURE_SIGNED_SKILLS=1`, `module.bin` / package must verify against
device public key. Default development builds accept unsigned packages.

## Hot load

`glk_skill_scan(skills_root)` registers manifests. Agent and RPC can list skills.
Bytecode sandbox is architected; v3.0.0 loads metadata + rules primarily.

## Craft loop

1. Capture on device / host sim  
2. `groklink-os craft capture.jsonl`  
3. Human review  
4. Copy to SD `skills/`  
5. Rescan  
