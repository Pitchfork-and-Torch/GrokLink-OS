# Security Policy — GrokLink OS

## Supported versions

| Version | Supported |
|---------|-----------|
| 3.1.x (GrokLink Native) | Yes — current |
| 3.0.x | Best-effort |
| 2.x overlay (GrokLink-Firmware) | Via that repo only |
| &lt; 2.0 | No |

## Reporting a vulnerability

If you discover a safety bypass, audit-log integrity flaw, confirm-token forgery path,
blacklist fail-open bug, or privilege escalation in GrokLink OS:

1. **Do not** open a public issue with exploit detail.
2. Use the private GitHub security advisory channel for this repository.
3. Include: version string, platform (host sim / STM32WB55), reproduction steps,
   and impact assessment (especially if TX/GPIO can fire without confirm).

We aim to acknowledge within 7 days and ship a fix or mitigation for confirmed
critical safety defects as soon as practical.

## Security model (summary)

- **Default-deny** for TX, GPIO output, contact, and system actions.
- **Confirm tokens**: short-lived, single-use, action-scoped.
- **Physical confirm** required for system-class actions on device.
- **Blacklists** offline-editable only; corrupt blacklist fails closed for TX.
- **Audit log**: append-only with cryptographic chaining (exportable).
- **Skills**: risk-classified; elevated skills need human approval before deploy.
- **ML outputs never auto-actuate** hardware — always filtered by policy.
- **Secure boot / signed skills**: hooks present; enable for release images.

## Out of scope

- Requests to remove safety gates, blacklists, or audit for convenience.
- Weaponization of radio / access-control capabilities.
- Bypassing regional RF regulations.
