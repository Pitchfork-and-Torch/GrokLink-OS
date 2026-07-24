# Threat Model — GrokLink OS 3.0

## Assets

- Radio transmit capability
- GPIO / contact interfaces
- Audit log integrity
- Skill packages
- Operator credentials / vault data
- Session edu state / confirm tokens

## Adversaries

| Actor | Intent |
|-------|--------|
| Malicious PC client | Force TX without auth |
| Compromised skill package | Elevate risk class / actuate |
| Physical attacker | Skip confirms, wipe blacklist |
| Network BLE peer | Spoof status/control |
| Careless operator | Accidental TX |

## Controls

| Threat | Control |
|--------|---------|
| Unauthorized TX | Default-deny + confirm + blacklist + edu ack |
| Token replay | Single-use + TTL |
| Blacklist wipe via RPC | No wipe opcode; offline edit only |
| Audit tamper | Hash chain; export verify |
| Skill malware | Risk ceiling + optional signatures |
| Radio thrash / DoS self | Rate limits + circuit breaker |
| ML abuse | Infer never actuates |
| Lost device | Vault encryption hooks; no secrets in git |

## Residual risks

- Physical debug access can dump RAM.
- Host sim is not a security boundary for real RF.
- Unsigned skills in dev builds trust the operator filesystem.

## Out of scope

Requests to weaken safety for unauthorized access are rejected.
