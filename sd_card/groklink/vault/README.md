# Private vault

Sensitive research notes / keys (operator-managed). Optional encryption hooks
in `glk_storage` + `glk_vault` (v3.8 persistent). Runtime files:

- `events.jsonl` - append-only hash-chained events (created at runtime)
- `chain.meta` - chain root + sealed flag
- `.integrity` - integrity marker

Do not commit secrets or live capture data. Host PHI seal remains in the bridge
(`groklink-os lab vault-seal`). Not for medical records.
